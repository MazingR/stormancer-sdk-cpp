#include "stormancer.h"

namespace Stormancer
{
	// ConnectionHandler

	Client::ConnectionHandler::ConnectionHandler()
	{
		IConnectionManager::connectionCount = 0;
	}

	Client::ConnectionHandler::~ConnectionHandler()
	{
	}

	uint64 Client::ConnectionHandler::generateNewConnectionId()
	{
		return _current++;
	}

	void Client::ConnectionHandler::newConnection(IConnection* connection)
	{
	}

	IConnection* Client::ConnectionHandler::getConnection(uint64 id)
	{
		throw std::runtime_error("Client::ConnectionHandler::getConnection not implemented.");
	}

	void Client::ConnectionHandler::closeConnection(IConnection* connection, std::string reason)
	{
	}


	// Client

	Client::Client(Configuration* config)
		: _initialized(false),
		_logger(ILogger::instance()),
		_scheduler(config->scheduler),
		_accountId(config->account),
		_applicationName(config->application),
		_tokenHandler(new TokenHandler()),
		_apiClient(new ApiClient(config, _tokenHandler)),
		_transport(config->transportFactory(std::map<std::string, void*>{ { "ILogger", (void*)_logger }, { "IScheduler", (void*)_scheduler } })),
		_dispatcher(config->dispatcher),
		_requestProcessor(new RequestProcessor(_logger, std::vector<IRequestModule*>())),
		_scenesDispatcher(new SceneDispatcher),
		_metadata(config->_metadata),
		_maxPeers(config->maxPeers),
		_pingInterval(config->pingInterval)
	{
		_dispatcher->addProcessor(_requestProcessor);
		_dispatcher->addProcessor(_scenesDispatcher);

		_metadata["serializers"] = "msgpack/array";
		_metadata["transport"] = _transport->name();
		_metadata["version"] = "1.0.0a";
		_metadata["platform"] = "cpp";
		_metadata["protocol"] = "2";

		for (auto plugin : config->plugins())
		{
			plugin->build(_pluginCtx);
		}

		_pluginCtx.clientCreated(this);

		initialize();
	}

	Client::~Client()
	{
		disconnect();

		delete _scenesDispatcher;
		delete _requestProcessor;
		delete _apiClient;
		delete _tokenHandler;

		if (_serverConnection)
		{
			delete _serverConnection;
		}

		if (_dispatcher)
		{
			delete _dispatcher;
		}

		if (_transport)
		{
			delete _transport;
		}
	}

	void Client::initialize()
	{
		if (!_initialized)
		{
			_initialized = true;
			_transport->packetReceived += std::function<void(Packet_ptr)>(([this](Packet_ptr packet) {
				this->transport_packetReceived(packet);
			}));
			_watch.reset();
		}
	}

	std::string Client::applicationName()
	{
		return _applicationName;
	}

	ILogger* Client::logger()
	{
		return _logger;
	}

	void Client::setLogger(ILogger* logger)
	{
		if (logger != nullptr)
		{
			_logger = logger;
		}
		else
		{
			_logger = ILogger::instance();
		}
	}

	pplx::task<Scene_ptr> Client::getPublicScene(std::string sceneId, std::string userData)
	{
		_logger->log(LogLevel::Debug, "Client::getPublicScene", sceneId, userData);

		return _apiClient->getSceneEndpoint(_accountId, _applicationName, sceneId, userData).then([this, sceneId](pplx::task<SceneEndpoint> t) {
			try
			{
				SceneEndpoint sep = t.get();
				return this->getScene(sceneId, sep);
			}
			catch (const std::exception& e)
			{
				std::runtime_error error(std::string(e.what()) + "\nUnable to get the scene endpoint / Failed to connect to the scene.");
				_logger->log(error);
				throw error;
			}
		});
	}

	pplx::task<Scene_ptr> Client::getScene(std::string token)
	{
		auto sep = _tokenHandler->decodeToken(token);
		return getScene(sep.tokenData.SceneId, sep);
	}

	pplx::task<Scene_ptr> Client::getScene(std::string sceneId, SceneEndpoint sep)
	{
		_logger->log(LogLevel::Debug, "Client::getScene", sceneId, sep.token);

		return taskIf(_serverConnection == nullptr, [this, sep]() {
			if (!_transport->isRunning()) {
				_cts = pplx::cancellation_token_source();
				try
				{
					_transport->start("client", new ConnectionHandler(), _cts.get_token(), 10, (uint16)(_maxPeers + 1));
				}
				catch (const std::exception& e)
				{
					throw std::runtime_error(std::string(e.what()) + "\nFailed to start the transport.");
				}
			}
			std::string endpoint = sep.tokenData.Endpoints.at(_transport->name());
			_logger->log(LogLevel::Trace, "Client::getScene", "Connecting transport", "");
			try
			{
				return _transport->connect(endpoint).then([this](pplx::task<IConnection*> t) {
					try
					{
						IConnection* connection = t.get();
						_logger->log(LogLevel::Trace, "Client::getScene", "Client::transport connected", "");
						_serverConnection = connection;
						_serverConnection->metadata = _metadata;
					}
					catch (const std::exception& e)
					{
						throw std::runtime_error(std::string(e.what()) + "\nFailed to get the transport peer connection.");
					}
				});
			}
			catch (const std::exception& e)
			{
				throw std::runtime_error(std::string(e.what()) + "\nFailed to connect the transport.");
			}
		}).then([this, sep](pplx::task<void> t) {
			try
			{
				t.wait();
				return updateServerMetadata();
			}
			catch (const std::exception& e)
			{
				throw e;
			}
		}).then([this, sep](pplx::task<void> t) {
			try
			{
				t.wait();
			}
			catch (const std::exception& e)
			{
				throw std::runtime_error(std::string(e.what()) + "\nFailed to start the sync clock.");
			}

			if (sep.tokenData.Version > 0)
			{
				startSyncClock();
			}
			SceneInfosRequestDto parameter;
			parameter.Metadata = _serverConnection->metadata;
			parameter.Token = sep.token;
			_logger->log(LogLevel::Debug, "Client::getScene", "send SceneInfosRequestDto", "");
			return sendSystemRequest<SceneInfosRequestDto, SceneInfosDto>((byte)SystemRequestIDTypes::ID_GET_SCENE_INFOS, parameter);
		}).then([this, sep, sceneId](pplx::task<SceneInfosDto> t) {
			SceneInfosDto result;
			try
			{
				result = t.get();
			}
			catch (const std::exception& e)
			{
				throw std::runtime_error(std::string(e.what()) + "\nFailed to get scene infos.");
			}

			std::stringstream ss;
			ss << result.SceneId << " " << result.SelectedSerializer << " Routes:[";
			for (uint32 i = 0; i < result.Routes.size(); i++)
			{
				ss << result.Routes.at(i).Handle << ":" << result.Routes.at(i).Name << ";";
			}
			ss << "] Metadata:[";
			for (auto it : result.Metadata)
			{
				ss << it.first << ":" << it.second << ";";
			}
			ss << "]";
			_logger->log(LogLevel::Debug, "Client::getScene", "SceneInfosDto received", ss.str());
			_serverConnection->metadata["serializer"] = result.SelectedSerializer;
			return updateServerMetadata().then([this, sep, sceneId, result](pplx::task<void> t) {
				try
				{
					t.wait();
					
					Scene_ptr scene;
					if (mapContains(_scenes, sceneId))
					{
						scene = _scenes[sceneId].lock();
					}
					else
					{
						scene = Scene_ptr(new Scene(_serverConnection, this, sceneId, sep.token, result, Action<void>([this, sceneId]() {
							if (mapContains(_scenes, sceneId))
							{
								_scenes.erase(sceneId);
							}
						})));
						_scenes[sceneId] = scene;
					}
					_pluginCtx.sceneCreated(scene.get());
					return scene;
				}
				catch (const std::exception& e)
				{
					throw e;
				}
			});
		});
	}

	pplx::task<void> Client::updateServerMetadata()
	{
		_logger->log(LogLevel::Trace, "Client::updateServerMetadata", "sending system request.", "");
		return _requestProcessor->sendSystemRequest(_serverConnection, (byte)SystemRequestIDTypes::ID_SET_METADATA, [this](bytestream* bs) {
			msgpack::pack(bs, _serverConnection->metadata);
		}).then([](pplx::task<Packet_ptr> t) {
			try
			{
				t.wait();
			}
			catch (std::exception& e) {
				throw std::logic_error(std::string(e.what()) + "\nFailed to update the server metadata.");
			}
		});
	}

	pplx::task<void> Client::connectToScene(Scene* scene, std::string& token, std::vector<Route_ptr> localRoutes)
	{
		ConnectToSceneMsg parameter;
		parameter.Token = token;
		for (auto r : localRoutes)
		{
			RouteDto routeDto;
			routeDto.Handle = r->_handle;
			routeDto.Metadata = r->metadata();
			routeDto.Name = r->name();
			parameter.Routes << routeDto;
		}
		parameter.ConnectionMetadata = _serverConnection->metadata;

		return Client::sendSystemRequest<ConnectToSceneMsg, ConnectionResult>((byte)SystemRequestIDTypes::ID_CONNECT_TO_SCENE, parameter).then([this, scene](pplx::task<ConnectionResult> t) {
			try
			{
				auto result = t.get();
				scene->completeConnectionInitialization(result);
				_scenesDispatcher->addScene(scene);
				_pluginCtx.sceneConnected(scene);
			}
			catch (const std::exception& e)
			{
				auto e2 = std::runtime_error(std::string(e.what()) + "\nFailed to connect to the scene.");
				_logger->log(e2);
				throw e2;
			}
		});
	}

	void Client::disconnect()
	{
		_cts.cancel();

		stopSyncClock();

		disconnectAllScenes();

		if (_serverConnection != nullptr)
		{
			_serverConnection->close();
			_serverConnection = nullptr;
		}
	}

	pplx::task<void> Client::disconnect(Scene* scene, byte sceneHandle)
	{
		DisconnectFromSceneDto dto(sceneHandle);
		return sendSystemRequest<DisconnectFromSceneDto, EmptyDto>((byte)SystemRequestIDTypes::ID_DISCONNECT_FROM_SCENE, dto)
			.then([this, sceneHandle, scene](pplx::task<EmptyDto> t) {
			_scenesDispatcher->removeScene(sceneHandle);
			_pluginCtx.sceneDisconnected(scene);
		});
	}

	void Client::disconnectAllScenes()
	{
		for (auto it : _scenes)
		{
			auto scene = it.second.lock();
			if (scene)
			{
				scene->disconnect();
			}
		}
	}

	void Client::transport_packetReceived(Packet_ptr packet)
	{
		_pluginCtx.packetReceived(packet);

		this->_dispatcher->dispatchPacket(packet);
	}

	int64 Client::clock()
	{
		return _watch.getElapsedTime() + _offset;
	}

	int64 Client::lastPing()
	{
		return _lastPing;
	}

	void Client::startSyncClock()
	{
		if (_scheduler)
		{
			_syncClockSubscription = _scheduler->schedulePeriodic((int)_pingInterval, Action<>(std::function<void()>([this]() {
				this->syncClockImpl();
			})));
		}
	}

	void Client::stopSyncClock()
	{
		_syncClockSubscription->unsubscribe();
	}

	pplx::task<void> Client::syncClockImpl()
	{
		try
		{
			int64 tStart = _watch.getElapsedTime();
			return _requestProcessor->sendSystemRequest(_serverConnection, (byte)SystemRequestIDTypes::ID_PING, [&tStart](bytestream* bs) {
				*bs << tStart;
			}, PacketPriority::IMMEDIATE_PRIORITY).then([this, tStart](Packet_ptr p) {
				int64 tEnd = this->_watch.getElapsedTime();
				uint64 tRef;
				*p->stream >> tRef;
				this->_lastPing = tEnd - tStart;
				this->_offset = (int64)tRef - this->_lastPing / 2 - tStart;
			});
		}
		catch (std::exception e)
		{
			_logger->log(LogLevel::Error, "Client::syncClockImpl", "Failed to ping server.", "");
			throw std::runtime_error("Failed to ping server.");
		}
	}

	// Class Client::Clock
	Client::Watch::Watch()
	{
		reset();
	}

	Client::Watch::~Watch()
	{
	}

	void Client::Watch::reset()
	{
		_startTime = std::chrono::high_resolution_clock::now();
	}

	int64 Client::Watch::getElapsedTime()
	{
		auto now = std::chrono::high_resolution_clock::now();
		auto dif = now - _startTime;
		auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(dif);
		return ms.count() + _baseTime;
	}

	void Client::Watch::setBaseTime(int64 baseTime)
	{
		reset();
		_baseTime = baseTime;
	}
};
