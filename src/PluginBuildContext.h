#pragma once
#include "headers.h"
#include "Action.h"

namespace Stormancer
{
	class Client;

	class Scene;

	template<typename T>
	class Packet;
	
	/// Object passed to the Build method of plugins to register to the available Stormancer client events.
	class PluginBuildContext
	{
	public:
		/// Event fired when a client object is created.
		Action<Client*> clientCreated;

		/// Event fired when a scene object is created.
		Action<Scene*> sceneCreated;

		/// Event fired when a a scene is connected to the server.
		Action<Scene*> sceneConnected;

		/// Event fired when a scene is disconnected.
		Action<Scene*> sceneDisconnected;

		/// Event fired when a scene is deleted.
		Action<Scene*> sceneDeleted;

		/// Event fired when a packet is received from a remote peer.
		Action<Packet_ptr> packetReceived;
	};
};