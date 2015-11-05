#pragma once
#include "headers.h"

namespace Stormancer
{
	class Scene;

	/// Represents a route on a scene.
	class Route
	{
	public:
		Route();
		Route(Scene_wptr scene, std::string& routeName, uint16 handle, stringMap metadata = stringMap());
		Route(Scene_wptr scene, std::string& routeName, stringMap metadata = stringMap());
		virtual ~Route();

	public:
		Scene_wptr scene();
		std::string name();
		stringMap& metadata();

	public:
		uint16 _handle;
		std::list<std::function<void(Packet_ptr)>> handlers;

	private:
		Scene_wptr _scene;
		std::string _name;
		stringMap _metadata;
	};

	using Route_ptr = std::shared_ptr<Route>;
};
