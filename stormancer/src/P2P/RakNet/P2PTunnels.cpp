#include "stdafx.h"
#include "P2P/RakNet/P2PTunnels.h"
#include "IConnectionManager.h"
#include "SystemRequestIDTypes.h"
#include "MessageIDTypes.h"
#include "P2P/RakNet/P2PTunnelClient.h"


namespace Stormancer
{
	P2PTunnels::P2PTunnels(std::shared_ptr<RequestProcessor> sysCall, std::shared_ptr<IConnectionManager> connections, std::shared_ptr<Serializer> serializer, std::shared_ptr<Configuration> configuration)
	{
		_sysClient = sysCall;
		_connections = connections;
		_serializer = serializer;
		_config = configuration;
	}

	std::shared_ptr<P2PTunnel> P2PTunnels::createServer(std::string serverId, std::shared_ptr<P2PTunnels> tunnels)
	{
		auto tunnel = std::make_shared<P2PTunnel>([tunnels, serverId]() {
			tunnels->destroyServer(serverId);
		});
		tunnel->id = serverId;
		tunnel->ip = "127.0.0.1";
		tunnel->port = _config->p2pServerPort;
		tunnel->side = P2PTunnelSide::Host;
		ServerDescriptor descriptor;
		descriptor.id = tunnel->id;
		descriptor.hostName = tunnel->ip;
		descriptor.port = tunnel->port;
		_servers[descriptor.id] = descriptor;

		return tunnel;
	}

	pplx::task<std::shared_ptr<P2PTunnel>> P2PTunnels::openTunnel(uint64 connectionId, std::string serverId)
	{
		auto connection = _connections->getConnection(connectionId);

		if (!connection)
		{
			throw std::runtime_error("No p2p connection established to the target peer");
		}

		return _sysClient->sendSystemRequest(connection.get(), (byte)SystemRequestIDTypes::ID_P2P_OPEN_TUNNEL, [this, serverId](bytestream* s) {
			_serializer->serialize(serverId, s);
		}).then([this, connectionId, serverId](pplx::task<Packet_ptr> t) {

			auto handle = _serializer->deserialize<byte>(t.get()->stream);
			auto client = std::make_shared<P2PTunnelClient>([this, connectionId, serverId](P2PTunnelClient* client, RakNet::RNS2RecvStruct* msg) {this->onMsgReceived(client, msg); }, _sysClient);
			client->handle = handle;
			client->peerId = connectionId;
			client->serverId = serverId;
			client->serverSide = false;
			_tunnels[std::make_tuple(connectionId, handle)] = client;

			auto tunnel = std::make_shared<P2PTunnel>([this, connectionId, handle]() {
				destroyTunnel(connectionId, handle);
			});
			tunnel->id = serverId;
			tunnel->ip = "127.0.0.1";
			tunnel->port = client->socket->GetBoundAddress().GetPort();
			return tunnel;
		});
	}

	byte P2PTunnels::addClient(std::string serverId, uint64 clientPeerId)
	{
		auto serverIt = _servers.find(serverId);
		if (serverIt == _servers.end())
		{
			throw std::runtime_error("The server does not exist");
		}
		for (byte handle = 0; handle < 256; handle++)
		{
			peerHandle key(clientPeerId, handle);
			if (_tunnels.find(key) == _tunnels.end())
			{
				auto client = std::make_shared<P2PTunnelClient>([this](P2PTunnelClient* client, RakNet::RNS2RecvStruct* msg) { this->onMsgReceived(client, msg); }, _sysClient);
				client->handle = handle;
				client->peerId = clientPeerId;
				client->serverId = serverId;
				client->serverSide = true;
				client->hostPort = serverIt->second.port;
				_tunnels[key] = client;
				return handle;
			}
		}

		throw std::runtime_error("Unable to create tunnel handle : Too many tunnels opened between the peers.");
	}

	void P2PTunnels::closeTunnel(byte handle, uint64 peerId)
	{
		_tunnels.erase(std::make_tuple(peerId, handle));
	}

	pplx::task<void> P2PTunnels::destroyServer(std::string serverId)
	{
		std::vector<pplx::task<void>> tasks;
		std::vector<peerHandle> itemsToDelete;

		for (auto tunnel : _tunnels)
		{
			if (tunnel.second->serverId == serverId && tunnel.second->serverSide)
			{
				auto connection = _connections->getConnection(tunnel.second->peerId);
				itemsToDelete.push_back(tunnel.first);
				if (connection)
				{
					auto handle = std::get<1>(tunnel.first);
					tasks.push_back(_sysClient->sendSystemRequest(connection.get(), (byte)SystemRequestIDTypes::ID_P2P_CLOSE_TUNNEL, [this, handle](bytestream* s) {
						_serializer->serialize(handle, s);
					}).then([](pplx::task<Packet_ptr> t) {
						try
						{
							t.get();
						}
						catch (...) {}
					}));
				}

			}
		}

		for (auto item : itemsToDelete)
		{
			_tunnels.erase(item);
		}

		return pplx::when_all(tasks.begin(), tasks.end());
	}

	pplx::task<void> P2PTunnels::destroyTunnel(uint64 peerId, byte handle)
	{
		auto it = _tunnels.find(std::make_tuple(peerId, handle));
		if (it != _tunnels.end())
		{
			auto client = (*it).second;
			auto connection = _connections->getConnection(client->peerId);
			_tunnels.erase(it);
			if (connection)
			{
				return _sysClient->sendSystemRequest(connection.get(), (byte)SystemRequestIDTypes::ID_P2P_CLOSE_TUNNEL, [this, handle](bytestream* s) {
					_serializer->serialize(handle, s);
				}).then([](pplx::task<Packet_ptr> t) {
					try
					{
						t.get();
					}
					catch (...) {}
				});
			}
		}

		return pplx::task_from_result();
	}

	void P2PTunnels::receiveFrom(uint64 id, bytestream* stream)
	{
		char handle;
		stream->read(&handle, 1);
		char buffer[1464];
		stream->read(buffer, 1464);

		auto read = stream->gcount();

		auto itTunnel = _tunnels.find(std::make_tuple(id, (byte)handle));
		if (itTunnel != _tunnels.end())
		{
			auto client = (*itTunnel).second;
			RakNet::RNS2_SendParameters bsp;
			bsp.data = buffer;
			bsp.length = (int)read;
			bsp.systemAddress.FromStringExplicitPort("127.0.0.1", client->hostPort, client->socket->GetBoundAddress().GetIPVersion());

			(*itTunnel).second->socket->Send(&bsp, _FILE_AND_LINE_);
		}
	}

	void P2PTunnels::onMsgReceived(P2PTunnelClient* client, RakNet::RNS2RecvStruct* recvStruct)
	{
		auto connection = _connections->getConnection(client->peerId);
		if (connection)
		{
			if (client->hostPort == 0)
			{
				client->hostPort = recvStruct->systemAddress.GetPort();
			}
			connection->sendRaw((byte)MessageIDTypes::ID_P2P_TUNNEL, [client, recvStruct](bytestream* s) {
				*s << client->handle;
				s->write(recvStruct->data, recvStruct->bytesRead);

			}, PacketPriority::IMMEDIATE_PRIORITY, PacketReliability::UNRELIABLE);
		}
	}

	std::size_t PeerHandle_hash::operator()(const peerHandle & k) const
	{
		return (size_t)(std::get<0>(k) ^ std::get<1>(k));
	}

	bool PeerHandle_equal::operator()(const peerHandle & v0, const peerHandle & v1) const
	{
		return v0 == v1;
	}
};