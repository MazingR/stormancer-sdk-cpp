#include "stormancer/stdafx.h"
#include "stormancer/P2P/P2PService.h"
#include "stormancer/P2P/P2PSession.h"
#include "stormancer/SystemRequestIDTypes.h"
#include "stormancer/P2P/RakNet/P2PTunnels.h"
#include "stormancer/SafeCapture.h"

namespace Stormancer
{
	P2PService::P2PService(std::shared_ptr<IConnectionManager> connections, std::shared_ptr<RequestProcessor> sysClient, std::shared_ptr<ITransport> transport, std::shared_ptr<Serializer> serializer, std::shared_ptr<P2PTunnels> tunnels)
	{
		_connections = connections;
		_sysCall = sysClient;
		_transport = transport;
		_serializer = serializer;
		_tunnels = tunnels;
	}

	std::shared_ptr<P2PTunnel> P2PService::registerP2PServer(const std::string& p2pServerId)
	{
		return _tunnels->createServer(p2pServerId, _tunnels);
	}

	pplx::task<std::shared_ptr<P2PTunnel>> P2PService::openTunnel(uint64 connectionId, const std::string& id, pplx::cancellation_token ct)
	{
		return _tunnels->openTunnel(connectionId, id, ct);
	}

	pplx::task<std::shared_ptr<IConnection>> P2PService::openP2PConnection(const std::string& p2pToken, pplx::cancellation_token ct)
	{
		auto server = _connections->getConnection(0);
		if (!server)
		{
			return pplx::task_from_exception<std::shared_ptr<IConnection>>(std::runtime_error("Cannot establish P2P connection. The client must be connected to the server"));
		}

		return _sysCall->sendSystemRequest<P2PSession>(server.get(), (byte)SystemRequestIDTypes::ID_P2P_CREATE_SESSION, p2pToken, ct)
			.then(STRM_SAFE_CAPTURE([this](P2PSession session)
		{
				return _connections->getConnection(session.remotePeer);
		}), ct);
	}
};
