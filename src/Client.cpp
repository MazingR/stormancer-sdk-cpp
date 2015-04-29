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
		throw exception("Client::ConnectionHandler::getConnection not implemented.");
	}

	void Client::ConnectionHandler::closeConnection(IConnection* connection, wstring reason)
	{
	}


	// Client

	Client::Client(ClientConfiguration* config)
		: _initialized(false),
		_logger(DefaultLogger::instance()),
		_accountId(config->account),
		_applicationName(config->application),
		_tokenHandler(new TokenHandler()),
		_apiClient(new ApiClient(config, _tokenHandler)),
		_transport(config->transport),
		_dispatcher(config->dispatcher),
		_requestProcessor(new RequestProcessor(_logger, vector<IRequestModule*>())),
		_scenesDispatcher(),
		_metadata(config->metadata),
		_maxPeers(config->maxPeers)
	{
		_metadata[L"serializers"] = L"msgpack/array";
		_metadata[L"transport"] = _transport->name();
		_metadata[L"version"] = L"1.0.0";
		_metadata[L"platform"] = L"cpp";

		initialize();

		_logger->log(LogLevel::Info, L"", L"Client created", to_wstring(Helpers::ptrToUint64(this)));
	}

	Client::~Client()
	{
		_logger->log(LogLevel::Info, L"", L"Client destroyed", to_wstring(Helpers::ptrToUint64(this)));
	}

	void Client::initialize()
	{
		if (!_initialized)
		{
			_initialized = true;
			auto d2 = this->_dispatcher;
			_transport->packetReceived += new function<void(Packet<>*)>(([this](Packet<>* packet) {
				this->transport_packetReceived(packet);
			}));
		}
	}

	wstring Client::applicationName()
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
			_logger = DefaultLogger::instance();
		}
	}

	pplx::task<Scene*> Client::getPublicScene(wstring sceneId, wstring userData)
	{
		_logger->log(LogLevel::Trace, L"", L"Client::getPublicScene", sceneId);

		return _apiClient->getSceneEndpoint(_accountId, _applicationName, sceneId, userData).then([this, sceneId](SceneEndpoint* sep) {
			return this->getScene(sceneId, sep);
		});
	}

	pplx::task<Scene*> Client::getScene(wstring sceneId, SceneEndpoint* sep)
	{
		_logger->log(LogLevel::Trace, L"", L"Client::getScene", sceneId);

		return Helpers::taskIf(_serverConnection == nullptr, [this, sep]() {
			return Helpers::taskIf(!_transport->isRunning(), [this]() {
				_cts = pplx::cancellation_token_source();
				return _transport->start(L"client", new ConnectionHandler(), _cts.get_token(), 11, (uint16)(_maxPeers + 1));
			}).then([this, sep]() {
				wstring endpoint = sep->tokenData->Endpoints[_transport->name()];
				return _transport->connect(endpoint).then([this](IConnection* connection) {
					_serverConnection = connection;

					for (auto& it : this->_metadata)
					{
						_serverConnection->metadata[it.first] = it.second;
					}
				});
			});
		}).then([this, sep]() {
				SceneInfosRequestDto parameter;
				parameter.Metadata = _serverConnection->metadata;
				parameter.Token = sep->token;

				return sendSystemRequest<SceneInfosRequestDto, SceneInfosDto>((byte)MessageIDTypes::ID_GET_SCENE_INFOS, parameter);
			}).then([this, sep, sceneId](SceneInfosDto result) {
				return new Scene(_serverConnection, this, sceneId, sep->token, result);
			});
	}

	pplx::task<void> Client::connectToScene(Scene* scene, wstring& token, vector<Route*> localRoutes)
	{
		ConnectToSceneMsg parameter;
		parameter.Token = token;
		for (auto r : localRoutes)
		{
			RouteDto routeDto;
			routeDto.Handle = r->index;
			routeDto.Metadata = r->metadata();
			routeDto.Name = r->name();
			parameter.Routes << routeDto;
		}
		parameter.ConnectionMetadata = _serverConnection->metadata;

		return Client::sendSystemRequest<ConnectToSceneMsg, ConnectionResult>((byte)MessageIDTypes::ID_CONNECT_TO_SCENE, parameter)
			.then([this, scene](ConnectionResult result) {
			scene->completeConnectionInitialization(result);
			this->_scenesDispatcher->addScene(scene);
		});
	}

	void Client::disconnect()
	{
		if (_serverConnection != nullptr)
		{
			_serverConnection->close();
		}
	}

	pplx::task<void> Client::disconnect(Scene* scene, byte sceneHandle)
	{
		auto _scenesDispatcher = this->_scenesDispatcher;
		return sendSystemRequest<byte, EmptyDto>((byte)MessageIDTypes::ID_DISCONNECT_FROM_SCENE, sceneHandle).then([this, _scenesDispatcher, sceneHandle](pplx::task<EmptyDto> t) {
			_scenesDispatcher->removeScene(sceneHandle);
		});
	}

	void Client::transport_packetReceived(Packet<>* packet)
	{
		// TODO plugins

		this->_dispatcher->dispatchPacket(packet);
	}
};
