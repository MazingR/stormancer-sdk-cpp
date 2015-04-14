#include "stormancer.h"

namespace Stormancer
{
	Scene::Scene(IConnection* connection, Client* client, wstring id, wstring token, SceneInfosDto dto)
		: _id(id),
		_peer(connection),
		_token(token),
		_client(client),
		_metadata(dto.Metadata)
	{
		for (auto routeDto : dto.Routes)
		{
			_remoteRoutesMap[routeDto.Name] = Route(this, routeDto.Name, routeDto.Metadata);
		}
	}

	Scene::~Scene()
	{
	}

	wstring Scene::getHostMetadata(wstring key)
	{
		return _metadata[key];
	}

	byte Scene::handle()
	{
		return _handle;
	}

	wstring Scene::id()
	{
		return _id;
	}

	bool Scene::connected()
	{
		return _connected;
	}

	IConnection* Scene::hostConnection()
	{
		return _peer;
	}

	vector<Route*> Scene::localRoutes()
	{
		return Helpers::mapValuesPtr(_localRoutesMap);
	}

	vector<Route*> Scene::remoteRoutes()
	{
		return Helpers::mapValuesPtr(_remoteRoutesMap);
	}

	void Scene::addRoute(wstring routeName, function<void(Packet<IScenePeer>*)> handler, stringMap metadata)
	{
		if (routeName.length() == 0 || routeName[0] == '@')
		{
			throw string("A route cannot be empty or start with the '@' character.");
		}

		if (_connected)
		{
			throw string("You cannot register handles once the scene is connected.");
		}

		if (!Helpers::mapContains(_localRoutesMap, routeName))
		{
			_localRoutesMap[routeName] = Route(this, routeName, metadata);
		}

		onMessage(routeName)->subscribe(handler);
	}

	rx::observable<Packet<IScenePeer>*>* Scene::onMessage(Route* route)
	{
		throw string("LOL");
		/*rx::observable<Packet<IScenePeer>> ob = rx::observable<>::create<Packet<IScenePeer>>([this, &route](rx::subscriber<Packet<IScenePeer>> observer) {
			function<void(Packet<>*)> handler = [this, &observer](Packet<>* p) {
				Packet<IScenePeer> packet(host(), p.stream, p.metadata());
				observer.on_next(packet);
			};
			route.handlers.push_back(handler);

			return [&route, &handler]() {
				auto it = find(route.handlers.begin(), route.handlers.end(), handler);
				route.handlers.erase(it);
			};
		});
		return ob;*/
	}

	rx::observable<Packet<IScenePeer>*>* Scene::onMessage(wstring routeName)
	{
		if (_connected)
		{
			throw string("You cannot register handles once the scene is connected.");
		}

		if (!Helpers::mapContains(_localRoutesMap, routeName))
		{
			_localRoutesMap[routeName] = Route(this, routeName);
		}

		Route* route = &_localRoutesMap[routeName];
		return onMessage(route);
	}

	void Scene::sendPacket(wstring routeName, function<void(bytestream*)> writer, PacketPriority priority, PacketReliability reliability)
	{
		if (routeName.length() == 0)
		{
			throw string("routeName is empty.");
		}
		if (!_connected)
		{
			throw string("The scene must be connected to perform this operation.");
		}

		if (!Helpers::mapContains(_remoteRoutesMap, routeName))
		{
			throw string(Helpers::StringFormat(L"The route '{0}' doesn't exist on the scene.", routeName));
		}
		Route& route = _remoteRoutesMap[routeName];

		_peer->sendToScene(_handle, route.index, writer, priority, reliability);
	}

	pplx::task<void> Scene::connect()
	{
		return _client->connectToScene(this, _token, Helpers::mapValuesPtr(_localRoutesMap)).then([this]() {
			_connected = true;
		});
	}

	pplx::task<void> Scene::disconnect()
	{
		return _client->disconnect(this, _handle);
	}

	void Scene::completeConnectionInitialization(ConnectionResult& cr)
	{
		_handle = cr.SceneHandle;

		for (auto pair : _localRoutesMap)
		{
			pair.second.index = cr.RouteMappings[pair.first];
			_handlers[pair.second.index] = pair.second.handlers;
		}
	}

	void Scene::handleMessage(Packet<>* packet)
	{
		for (auto fun : packetReceived)
		{
			fun(packet);
		}

		byte tmp[2];
		*packet->stream >> tmp[0];
		*packet->stream >> tmp[1];
		uint16 routeId = tmp[0] * 256 + tmp[1];

		packet->setMetadata(L"routeId", new uint16(routeId));

		if (_handlers.find(routeId) != _handlers.end())
		{
			auto observer = _handlers[routeId];
			for (size_t i = 0; i < observer.size(); i++)
			{
				observer[i](packet);
			}
		}
	}

	vector<IScenePeer*> Scene::remotePeers()
	{
		return vector<IScenePeer*> { host() };
	}

	IScenePeer* Scene::host()
	{
		return new ScenePeer(_peer, _handle, _remoteRoutesMap, this);
	}
};
