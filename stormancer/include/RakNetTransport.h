#pragma once
#include "headers.h"
#include "ITransport.h"
#include "ILogger.h"
#include "RaknetConnection.h"
#include <RakPeerInterface.h>
#include "DependencyResolver.h"

namespace Stormancer
{
	/// Core functions for communicating over the network using RakNet.
	class RakNetTransport : public ITransport
	{
	public:
		RakNetTransport(DependencyResolver* resolver);
		virtual ~RakNetTransport();

	public:
		void start(std::string type, IConnectionManager* handler, pplx::cancellation_token token, uint16 maxConnections = 10, uint16 serverPort = 0);
		pplx::task<IConnection*> connect(std::string endpoint);
		void stop();

	private:
		void initialize(uint16 maxConnections, uint16 serverPort = 0);
		void run();
		void onConnectionIdReceived(uint64 p);
		RakNetConnection* onConnection(RakNet::Packet* packet);
		void onDisconnection(RakNet::Packet* packet, std::string reason);
		void onMessageReceived(RakNet::Packet* packet);
		RakNetConnection* getConnection(RakNet::RakNetGUID guid);
		RakNetConnection* createNewConnection(RakNet::RakNetGUID raknetGuid, RakNet::RakPeerInterface* peer);
		RakNetConnection* removeConnection(RakNet::RakNetGUID guid);
		void onRequestClose(RakNetConnection* c);
		bool isRunning() const;
		std::string name() const;
		uint64 id() const;
		DependencyResolver* dependencyResolver() const;
		void onPacketReceived(std::function<void(Packet_ptr)> callback);
		const char* host() const;
		uint16 port() const;
		void onTransportEvent(std::function<void(void*, void*, void*)> callback);

	private:
		DependencyResolver* _dependencyResolver = nullptr;
		IConnectionManager* _handler = nullptr;
		std::shared_ptr<RakNet::RakPeerInterface> _peer = nullptr;
		std::shared_ptr<ILogger> _logger = nullptr;
		std::string _type;
		std::map<uint64, RakNetConnection*> _connections;
		const int connectionTimeout = 5000;
		std::map<std::string, pplx::task_completion_event<IConnection*>> _pendingConnections;
		std::shared_ptr<IScheduler> _scheduler = nullptr;
		rxcpp::subscription _scheduledTransportLoop;
		RakNet::SocketDescriptor* _socketDescriptor = nullptr;
		std::mutex _mutex;
		std::string _name;
		uint64 _id = 0;
		Action<Packet_ptr> _onPacketReceived;
		std::string _host;
		uint16 _port = 0;
		std::function<void(void*, void*, void*)> _onTransportEvent;
		RakNet::RakNetGUID _serverRakNetGUID;
	};
};
