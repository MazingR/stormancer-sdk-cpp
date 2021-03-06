#include "stormancer/stdafx.h"
#include "GetTime.h"
#include "BitStream.h"
#include "stormancer/RakNet/RakNetTransport.h"
#include "stormancer/IScheduler.h"
#include "stormancer/MessageIDTypes.h"
#include "MessageIdentifiers.h"
#include "stormancer/SafeCapture.h"
#include "PacketFileLogger.h"

#if defined(_WIN32)
//
#else
#include <unistd.h>
#endif

namespace Stormancer
{
	RakNetTransport::RakNetTransport(DependencyResolver* resolver)
		: _dependencyResolver(resolver)
		, _logger(resolver->resolve<ILogger>())
		, _scheduler(resolver->resolve<IScheduler>())
	{
	}

	RakNetTransport::~RakNetTransport()
	{
		_logger->log(LogLevel::Trace, "RakNetTransport", "Deleting RakNet transport...");

		if (_isRunning)
		{
			stop();
		}

		_logger->log(LogLevel::Trace, "RakNetTransport", "RakNet transport deleted");
	}

	void RakNetTransport::start(std::string type, std::shared_ptr<IConnectionManager> handler, pplx::cancellation_token ct, uint16 serverPort, uint16 maxConnections)
	{
		_logger->log(LogLevel::Trace, "RakNetTransport", "Starting RakNet transport...");

		if (!compareExchange(_mutex, _isRunning, false, true))
		{
			throw std::runtime_error("RakNet transport is already started.");
		}

		if (handler == nullptr && maxConnections > 0)
		{
			throw std::invalid_argument("Handler is null for server.");
		}

		_type = type;
		_handler = handler;
		initialize(maxConnections, serverPort);

		_scheduler->schedulePeriodic(15, STRM_SAFE_CAPTURE([this]() {
			std::lock_guard<std::mutex> lock(_mutex);
			if (_isRunning)
			{
				run();
			}
		}), ct);

		ct.register_callback(STRM_SAFE_CAPTURE([this]() {
			stop();
		}));

		_logger->log(LogLevel::Trace, "RakNetTransport", "RakNet transport started");
	}

	void RakNetTransport::initialize(uint16 maxConnections, uint16 serverPort)
	{
		try
		{
			_logger->log(LogLevel::Trace, "RakNetTransport", "Initializing raknet transport", _type.c_str());

			RakNet::PacketFileLogger* rakNetLogger = nullptr;
#ifdef STORMANCER_PACKETFILELOGGER
			rakNetLogger = new RakNet::PacketFileLogger();
			rakNetLogger->StartLog("packetLogs");
#endif

			_peer = std::shared_ptr<RakNet::RakPeerInterface>(RakNet::RakPeerInterface::GetInstance(), STRM_SAFE_CAPTURE([=](RakNet::RakPeerInterface* peer) {
				_logger->log(LogLevel::Trace, "RakNetTransport", "Deleting RakPeerInterface...");
				RakNet::RakPeerInterface::DestroyInstance(peer);
				if (rakNetLogger)
				{
					delete rakNetLogger;
				}
			}));

#ifdef STORMANCER_PACKETFILELOGGER
			_peer->AttachPlugin(rakNetLogger);
#endif

			_dependencyResolver->registerDependency(_peer);
			if (serverPort != 0)
			{
				_socketDescriptor = std::make_shared<RakNet::SocketDescriptor>(serverPort, (const char*)nullptr);
			}
			else
			{
				_socketDescriptor = std::make_shared<RakNet::SocketDescriptor>();
			}
			_socketDescriptor->socketFamily = AF_INET;
			DataStructures::List<RakNet::SocketDescriptor> socketDescriptorsList;
			socketDescriptorsList.Push(*_socketDescriptor, _FILE_AND_LINE_);
			auto startupResult = _peer->Startup(maxConnections, socketDescriptorsList, 1);
			if (startupResult != RakNet::StartupResult::RAKNET_STARTED)
			{
				throw std::runtime_error(std::string("RakNet peer startup failed (RakNet::StartupResult == ") + std::to_string(startupResult) + ')');
			}
			_peer->SetMaximumIncomingConnections(maxConnections);
			_logger->log(LogLevel::Trace, "RakNetTransport", "Raknet transport initialized", _type.c_str());
		}
		catch (const std::exception& ex)
		{
			throw std::runtime_error(std::string() + "Failed to initialize the RakNet peer (" + ex.what() + ")");
		}
	}

	void RakNetTransport::run()
	{
		try
		{
			RakNet::Packet* rakNetPacket = nullptr;
			while ((rakNetPacket = _peer->Receive()) != nullptr)
			{
				if (rakNetPacket->length == 0)
				{
					assert(false);
					_peer->DeallocatePacket(rakNetPacket);
					continue;
				}

				byte ID = rakNetPacket->data[0];
				//_logger->log(LogLevel::Trace, "RakNetTransport", "RakNet packet received ", std::to_string(ID));
#ifdef STORMANCER_LOG_RAKNET_PACKETS
				std::vector<byte> receivedData(rakNetPacket->data, rakNetPacket->data + rakNetPacket->length);
				auto bytes = stringifyBytesArray(receivedData, true);
				_logger->log(LogLevel::Trace, "RakNetTransport", "RakNet packet received", bytes.c_str());
#endif

				try
				{
					switch (ID)
					{
						// RakNet messages types
					case DefaultMessageIDTypes::ID_CONNECTION_REQUEST_ACCEPTED:
					{
						std::lock_guard<std::mutex> lg(_pendingConnection_mtx);

						std::string packetSystemAddressStr = rakNetPacket->systemAddress.ToString(true, ':');
						if (!_pendingConnections.empty())
						{
							auto rq = _pendingConnections.front();
							if (!_serverConnected)
							{
								_pendingConnections.pop();
								if (rq.cancellationToken.is_canceled())
								{
									_peer->CloseConnection(rakNetPacket->guid, false);
								}
								else
								{
									_logger->log(LogLevel::Trace, "RakNetTransport", "Connection request accepted", packetSystemAddressStr.c_str());
									_serverConnected = true;
									_serverRakNetGUID = rakNetPacket->guid;
									auto connection = onConnection(rakNetPacket->systemAddress, rakNetPacket->guid, 0);
									rq.tce.set(connection);
								}
								startNextPendingConnections();
							}
							else
							{
								if (rq.cancellationToken.is_canceled())
								{
									_peer->CloseConnection(rakNetPacket->guid, false);
								}
								else
								{
									RakNet::BitStream data;
									data.Write((byte)MessageIDTypes::ID_ADVERTISE_PEERID);
									data.Write(_id);
									data.Write(false);
									_peer->Send(&data, PacketPriority::MEDIUM_PRIORITY, PacketReliability::RELIABLE, 0, rakNetPacket->guid, false);
								}
							}
						}
						else
						{
							_logger->log(LogLevel::Error, "RakNetTransport", "Can't get the pending connection TCE", packetSystemAddressStr.c_str());
						}
						break;
					}
					case DefaultMessageIDTypes::ID_INCOMPATIBLE_PROTOCOL_VERSION:
					case DefaultMessageIDTypes::ID_NO_FREE_INCOMING_CONNECTIONS:
					case DefaultMessageIDTypes::ID_CONNECTION_ATTEMPT_FAILED:
					{
						std::string packetSystemAddressStr = rakNetPacket->systemAddress.ToString(true, ':');

						//_peer->CloseConnection(rakNetPacket->guid, false);

						std::lock_guard<std::mutex> lg(_pendingConnection_mtx);

						if (!_pendingConnections.empty())
						{
							auto rq = _pendingConnections.front();
							_pendingConnections.pop();
							if (!rq.cancellationToken.is_canceled())
							{
								_logger->log(LogLevel::Trace, "RakNetTransport", "Connection request failed", packetSystemAddressStr.c_str());
								rq.tce.set_exception(std::runtime_error("Connection attempt failed"));
							}
							startNextPendingConnections();
						}
						else
						{
							_logger->log(LogLevel::Error, "RakNetTransport", "Can't get the pending connection TCE", packetSystemAddressStr.c_str());
						}
						break;
					}
					case DefaultMessageIDTypes::ID_ALREADY_CONNECTED:
					{
						_logger->log(LogLevel::Error, "RakNetTransport", "Peer already connected", rakNetPacket->systemAddress.ToString(true, ':'));
						break;
					}
					case DefaultMessageIDTypes::ID_NEW_INCOMING_CONNECTION:
					{
						_logger->log(LogLevel::Trace, "RakNetTransport", "Incoming connection", rakNetPacket->systemAddress.ToString(true, ':'));
						RakNet::BitStream data;
						data.Write((char)MessageIDTypes::ID_ADVERTISE_PEERID);
						data.Write(_id);
						data.Write(true);
						_peer->Send(&data, PacketPriority::MEDIUM_PRIORITY, PacketReliability::RELIABLE, 0, rakNetPacket->guid, false);
						break;
					}
					case DefaultMessageIDTypes::ID_DISCONNECTION_NOTIFICATION:
					{
						_logger->log(LogLevel::Trace, "RakNetTransport", "Peer disconnected", rakNetPacket->systemAddress.ToString(true, ':'));
						onDisconnection(rakNetPacket, "CLIENT_DISCONNECTED");
						break;
					}
					case DefaultMessageIDTypes::ID_CONNECTION_LOST:
					{
						_logger->log(LogLevel::Trace, "RakNetTransport", "Peer lost the connection", rakNetPacket->systemAddress.ToString(true, ':'));
						onDisconnection(rakNetPacket, "CLIENT_CONNECTION_LOST");
						break;
					}
					case DefaultMessageIDTypes::ID_CONNECTION_BANNED:
					{
						_logger->log(LogLevel::Trace, "RakNetTransport", "We are banned from the system we attempted to connect to", rakNetPacket->systemAddress.ToString(true, ':'));
						break;
					}
					case DefaultMessageIDTypes::ID_INVALID_PASSWORD:
					{
						_logger->log(LogLevel::Trace, "RakNetTransport", "The remote system is using a password and has refused our connection because we did not set the correct password", rakNetPacket->systemAddress.ToString(true, ':'));
						break;
					}
					case DefaultMessageIDTypes::ID_IP_RECENTLY_CONNECTED:
					{
						_logger->log(LogLevel::Trace, "RakNetTransport", "this IP address connected recently, and can't connect again as a security measure", rakNetPacket->systemAddress.ToString(true, ':'));
						break;
					}
					case DefaultMessageIDTypes::ID_UNCONNECTED_PONG:
					{

						auto address = std::string(rakNetPacket->systemAddress.ToString(true, ':'));
						_logger->log(LogLevel::Debug, "RakNetTransport", "Received pong message.", address);
						RakNet::BitStream data(rakNetPacket->data + 1, rakNetPacket->length - 1, false);
						RakNet::TimeMS sentOn;

						data.Read<RakNet::TimeMS>(sentOn);

						auto it = _pendingPings.find(address);

						if (it != _pendingPings.end())
						{
							it->second.set(RakNet::GetTimeMS() - sentOn);
						}
						break;
					}
					case DefaultMessageIDTypes::ID_UNCONNECTED_PING_OPEN_CONNECTIONS:
					{
						break;
					}
					case DefaultMessageIDTypes::ID_ADVERTISE_SYSTEM:
					{
						_logger->log(LogLevel::Trace, "RakNetTransport", "Inform a remote system of our IP/Port", rakNetPacket->systemAddress.ToString(true, ':'));
						break;
					}
					// Stormancer messages types
					case (byte)MessageIDTypes::ID_CONNECTION_RESULT:
					{
						int64 sid;
						std::memcpy(&sid, (rakNetPacket->data + 1), sizeof(sid));
						_logger->log(LogLevel::Trace, "RakNetTransport", "Connection ID received", std::to_string(sid));
						onConnectionIdReceived(sid);
						break;
					}
					case (byte)DefaultMessageIDTypes::ID_UNCONNECTED_PING:
						break;
					case (byte)MessageIDTypes::ID_ADVERTISE_PEERID:
					{
						std::lock_guard<std::mutex> lg(_pendingConnection_mtx);
						std::string packetSystemAddressStr = rakNetPacket->systemAddress.ToString(true, ':');
						uint64 remotePeerId = 0;
						RakNet::BitStream data(rakNetPacket->data + 1, rakNetPacket->length - 1, false);
						data.Read(remotePeerId);
						bool waitingConnection;
						data.Read(waitingConnection);

						if (waitingConnection)
						{
							auto rq = _pendingConnections.front();
							_pendingConnections.pop();
							if (rq.cancellationToken.is_canceled())
							{
								_peer->CloseConnection(rakNetPacket->guid, false);
							}
							else
							{
								_logger->log(LogLevel::Trace, "RakNetTransport", "Connection request accepted", packetSystemAddressStr.c_str());
								auto connection = onConnection(rakNetPacket->systemAddress, rakNetPacket->guid, (uint64)remotePeerId);
								rq.tce.set(connection);
							}
							startNextPendingConnections();
						}
						else
						{
							// TODO: review. we don't store the RaknetConnection ?
							onConnection(rakNetPacket->systemAddress, rakNetPacket->guid, (uint64)remotePeerId);
						}
						break;
					}
					case DefaultMessageIDTypes::ID_TIMESTAMP:
					case (byte)MessageIDTypes::ID_SYSTEM_REQUEST:
					case (byte)MessageIDTypes::ID_REQUEST_RESPONSE_COMPLETE:
					case (byte)MessageIDTypes::ID_REQUEST_RESPONSE_ERROR:
					case (byte)MessageIDTypes::ID_REQUEST_RESPONSE_MSG:
					case (byte)MessageIDTypes::ID_SCENES:

					default:
					{
						if (ID >= DefaultMessageIDTypes::ID_USER_PACKET_ENUM || (ID >= 58 && ID <= 59))
						{
							onMessageReceived(rakNetPacket);
						}
						else
						{
							_logger->log(LogLevel::Trace, "RakNetTransport", "Unprocessed RakNet message ID", std::to_string(ID));
						}
						rakNetPacket = nullptr;
						break;
					}
					}
					if (rakNetPacket)
					{
						_peer->DeallocatePacket(rakNetPacket);
					}
				}
				catch (const std::exception& ex)
				{
					_logger->log(LogLevel::Error, "RakNetTransport", "An error occured while handling a message", ex.what());
				}
			}
		}
		catch (const std::exception& ex)
		{
			_logger->log(LogLevel::Error, "RakNetTransport", "An error occured while running the transport", ex.what());
		}
	}

	void RakNetTransport::stop()
	{
		_logger->log(LogLevel::Trace, "RakNetTransport", "Stopping RakNet transport...");

		if (!compareExchange(_mutex, _isRunning, true, false))
		{
			throw std::runtime_error("RakNet transport is not started");
		}

		auto peer = _peer;
		_peer.reset();
		if (peer)
		{
			if (peer->IsActive())
			{
				peer->Shutdown(1000);
			}

			if (_socketDescriptor)
			{
				_socketDescriptor.reset();
			}

			if (_handler)
			{
				_handler.reset();
			}

			_logger->log(LogLevel::Trace, "RakNetTransport", "RakNet transport stopped");
		}
	}

	std::vector<std::string> RakNetTransport::externalAddresses() const
	{
		throw std::runtime_error("not implemented");
		/*std::vector<std::string> addrs;
		addrs.push_back(_peer->GetSystemAddressFromGuid(_peer->GetMyGUID()).ToString(true, ':'));

		if (isRunning())
		{
			addrs.push_back(_peer->GetExternalID(_peer->GetSystemAddressFromGuid(_serverRakNetGUID)).ToString(true, ':'));
		}
		return addrs;*/
	}

	void RakNetTransport::onConnectionIdReceived(uint64 p)
	{
		_id = p;
	}

	pplx::task<std::shared_ptr<IConnection>> RakNetTransport::connect(std::string endpoint, pplx::cancellation_token ct)
	{
		std::lock_guard<std::mutex> lock(_pendingConnection_mtx);

		auto shouldStart = _pendingConnections.empty();
		ConnectionRequest rq;
		rq.endpoint = endpoint;
		rq.cancellationToken = ct;
		_pendingConnections.push(rq);
		if (shouldStart)
		{
			startNextPendingConnections();
		}

		return pplx::task<std::shared_ptr<IConnection>>(rq.tce, ct);
	}

	void RakNetTransport::startNextPendingConnections()
	{
		if (_pendingConnections.empty())
		{
			return;
		}

		auto rq = _pendingConnections.front();

		if (rq.cancellationToken.is_canceled())
		{
			_pendingConnections.pop();
			startNextPendingConnections();
		}

		auto endpoint = rq.endpoint;
		auto split = stringSplit(endpoint, ':');
		auto tce = rq.tce;
		if (split.size() < 2)
		{
			tce.set_exception(std::invalid_argument("Bad server endpoint, no port (" + endpoint + ')'));
			return;
		}

		std::string portString = split[1];
		if (endpoint.size() - 1 <= portString.size())
		{
			tce.set_exception(std::invalid_argument("Bad server endpoint, no host (" + endpoint + ')'));
			return;
		}

		_port = (uint16)std::atoi(portString.c_str());
		if (_port == 0)
		{
			tce.set_exception(std::runtime_error("Server endpoint port should not be 0 (" + endpoint + ')'));
			return;
		}

		auto hostStr = split[0];

		if (_peer == nullptr || !_peer->IsActive())
		{
			tce.set_exception(std::runtime_error("Transport not started. Make sure you started it."));
			return;
		}

		auto result = _peer->Connect(hostStr.c_str(), _port, nullptr, 0, nullptr, 0, 12, 500, 30000);
		if (result != RakNet::ConnectionAttemptResult::CONNECTION_ATTEMPT_STARTED)
		{
			tce.set_exception(std::runtime_error(std::string("Bad RakNet connection attempt result (") + std::to_string(result) + ')'));
			return;
		}
	}

	std::shared_ptr<RakNetConnection> RakNetTransport::onConnection(RakNet::SystemAddress systemAddress, RakNet::RakNetGUID guid, uint64 peerId)
	{
		auto msg = std::string() + systemAddress.ToString(true, ':') + " connected";
		_logger->log(LogLevel::Trace, "RakNetTransport", msg.c_str());

		auto connection = createNewConnection(guid, peerId);

		_handler->newConnection(connection);

		pplx::task<void>([=]() {
			// Start this asynchronously because we locked the mutex in run and the user can do something that tries to lock again this mutex
			connection->setConnectionState(ConnectionState::Connecting); // we should set connecting state sooner (when the user call connect)
			connection->setConnectionState(ConnectionState::Connected);
		});

		return connection;
	}

	void RakNetTransport::onDisconnection(RakNet::Packet* packet, std::string reason)
	{
		auto msg = std::string() + packet->systemAddress.ToString(true, ':') + " disconnected";
		_logger->log(LogLevel::Trace, "RakNetTransport", msg.c_str(), reason.c_str());

		auto connection = removeConnection(packet->guid);

		_handler->closeConnection(connection, reason);

		connection->_closeAction(reason);

		pplx::task<void>([=]() {
			// Start this asynchronously because we locked the mutex in run and the user can do something that tries to lock again this mutex
			connection->setConnectionState(ConnectionState::Disconnected);
		});
	}

	void RakNetTransport::onMessageReceived(RakNet::Packet* rakNetPacket)
	{
#if defined(STORMANCER_LOG_PACKETS) && !defined(STORMANCER_LOG_RAKNET_PACKETS)
		std::vector<byte> tempBytes(rakNetPacket->data, rakNetPacket->data + rakNetPacket->length);
		auto bytes = stringifyBytesArray(tempBytes, true);
		_logger->log(LogLevel::Trace, "RakNetTransport", "Packet received", bytes.c_str());
#endif

		auto connection = getConnection(rakNetPacket->guid);
		auto stream = new ibytestream((byte*)rakNetPacket->data, (std::streamsize)rakNetPacket->length);

		Packet_ptr packet(new Packet<>(connection, stream), deleter<Packet<>>());
		auto peer = _peer;
		packet->cleanup += [=]() {
			if (stream)
			{
				stream->rdbuf()->pubsetbuf(nullptr, 0);
				delete stream;
			}
			if (peer && rakNetPacket)
			{
				peer->DeallocatePacket(rakNetPacket);
			}
		};

		_onPacketReceived(packet);
	}

	std::shared_ptr<RakNetConnection> RakNetTransport::getConnection(RakNet::RakNetGUID guid)
	{
		return _connections[guid.g];
	}

	std::shared_ptr<RakNetConnection> RakNetTransport::createNewConnection(RakNet::RakNetGUID raknetGuid, uint64 peerId)
	{
		if (_peer)
		{
			int64 cid = peerId;
			auto connection = std::make_shared<RakNetConnection>(raknetGuid, cid, _peer, _logger,_dependencyResolver);
			RakNet::RakNetGUID guid(connection->guid());
			auto logger = _logger;
			std::weak_ptr<RakNet::RakPeerInterface> weakPeer = _peer;
			connection->onClose([logger, weakPeer, guid](std::string reason) {
				auto peer = weakPeer.lock();
				if (peer)
				{
					logger->log(LogLevel::Trace, "RakNetTransport", "On close", guid.ToString());
					peer->CloseConnection(guid, true);
				}
			});
			_connections[raknetGuid.g] = connection;
			return connection;
		}
		return std::shared_ptr<RakNetConnection>();
	}

	std::shared_ptr<RakNetConnection> RakNetTransport::removeConnection(RakNet::RakNetGUID guid)
	{
		auto connection = _connections[guid.g];
		_connections.erase(guid.g);
		return connection;
	}

	bool RakNetTransport::isRunning() const
	{
		return _isRunning;
	}

	std::string RakNetTransport::name() const
	{
		return _name;
	}

	uint64 RakNetTransport::id() const
	{
		return _id;
	}

	DependencyResolver* RakNetTransport::dependencyResolver() const
	{
		return _dependencyResolver;
	}

	void RakNetTransport::onPacketReceived(std::function<void(Packet_ptr)> callback)
	{
		_onPacketReceived += callback;
	}

	std::string RakNetTransport::host() const
	{
		return _host.c_str();
	}

	uint16 RakNetTransport::port() const
	{
		auto boundAddress = _peer->GetMyBoundAddress(0);
		return boundAddress.GetPort();
	}

	pplx::task<int> RakNetTransport::sendPing(const std::string& address, pplx::cancellation_token ct)
	{
		return this->sendPing(address, 2, ct);
	}

	bool RakNetTransport::sendPingImpl(const std::string& address)
	{
		auto els = stringSplit(address, ':');
		assert(els.size() >= 2);

		auto port = (uint16)std::atoi(els[1].c_str());

		auto peer = _peer;
		if (peer)
		{
			return peer->Ping(els[0].c_str(), port, false);
		}
		else
		{
			// ping fails: no peer to perform the ping!
			return false;
		}

		/*auto p2 = RakNet::RakPeerInterface::GetInstance();
		p2->Startup(10, new RakNet::SocketDescriptor(), 1);
		auto result = p2->Connect(els[0].c_str(), (uint16)std::atoi(els[1].c_str()),nullptr,0);*/

		//_peer->Connect("127.0.0.1", port, nullptr, 0);
	}

	pplx::task<int> RakNetTransport::sendPing(const std::string& address, const int nb, pplx::cancellation_token ct)
	{
		pplx::task_completion_event<int> tce;

		{
			std::lock_guard<std::mutex> lg(_pendingPingsMutex);
			_pendingPings[address] = tce;
		}

		pplx::task<int> eventSetTask(tce, ct);
		pplx::cancellation_token_source cts = ct.is_cancelable() ? pplx::cancellation_token_source::create_linked_source(ct) : pplx::cancellation_token_source();
		std::vector<pplx::task<bool>> tasks;
		for (int i = 0; i < nb; i++)
		{
			tasks.push_back(taskDelay(std::chrono::milliseconds(300 * i), cts.get_token())
				.then(STRM_SAFE_CAPTURE([this, address]()
			{
				return sendPingImpl(address);
			}), cts.get_token()));
		}

		auto logger = _logger;
		pplx::when_all(tasks.begin(), tasks.end()).then([logger, tce, address](std::vector<bool> results) {
			bool sent = false;

			for (bool result : results)
			{
				if (result)
				{
					sent = true;
					break;
				}
			}

			if (!sent)
			{
				logger->log(LogLevel::Debug, "RakNetTransport", "Pings to " + address + " failed: unreachable address.");
				tce.set(-1);
			}
		}, cts.get_token())
			.then([logger, address](pplx::task<void> t) {
			try
			{
				t.get();
			}
			catch (const std::exception&)
			{
				logger->log(LogLevel::Debug, "RakNetTransport", "Pings to " + address + " failed: ping cancelled.");
			}
		});

		return cancel_after_timeout(eventSetTask, cts, 1500)
			.then(STRM_SAFE_CAPTURE([this, address](pplx::task<int> t)
		{
			std::lock_guard<std::mutex> lg(_pendingPingsMutex);
			_pendingPings.erase(address); // destroys the tce and cancels the task
			try
			{
				return t.get();
			}
			catch (std::exception& ex)
			{
				_logger->log(LogLevel::Debug, "RakNetTransport", "Ping to " + address + " failed", ex.what());
				return -1;
			}
		}));
	}

	void RakNetTransport::openNat(const std::string& address)
	{
		auto els = stringSplit(address, ':');
		this->_peer->SendTTL(els[0].c_str(), (uint16)std::stoi(els[1]), 3);
	}

	std::vector<std::string> RakNetTransport::getAvailableEndpoints() const
	{
		auto nbIps = this->_peer->GetNumberOfAddresses();
		std::vector<std::string> ips;
		for (unsigned int i = 0; i < nbIps; i++)
		{
			auto boundAddress = _peer->GetMyBoundAddress(0);
			auto ip = std::string(_peer->GetLocalIP(i));
			if (is_ipv4_address(ip))
			{
				ips.push_back(ip + ":" + std::to_string(boundAddress.GetPort()));
			}
		}

		return ips;
	}
};
