#pragma once

#include "stormancer/headers.h"
#include "stormancer/IPlugin.h"

namespace Stormancer
{
	class GameSessionPlugin : public IPlugin
	{
	public:
		void sceneCreated(Scene* scene) override;
		void sceneDisconnecting(Scene* scene) override;
	};
};
