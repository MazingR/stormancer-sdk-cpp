#include "stdafx.h"
#include "AssetsStoragePlugin.h"
#include "AssetsStorageService.h"

namespace Stormancer
{
	void AssetsStoragePlugin::sceneCreated(Scene* scene)
	{
		if (scene)
		{	
			auto name = scene->getHostMetadata("stormancer.assetsstorage");
			if (!name.empty())
			{						
				auto service = std::make_shared<AssetsStorageService>(scene->shared_from_this());
				scene->dependencyResolver()->registerDependency<AssetsStorageService>(service);
			}
		}
	}
};
