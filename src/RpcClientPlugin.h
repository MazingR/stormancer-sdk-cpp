#pragma once
#include "headers.h"
#include "IClientPlugin.h"

namespace Stormancer
{
	class RpcClientPlugin : public IClientPlugin
	{
	public:
		void build(PluginBuildContext& ctx);

	public:
		static const char* pluginName;
		static const char* serviceName;
		static const char* version;
		STORMANCER_DLL_API static const char* nextRouteName;
		STORMANCER_DLL_API static const char* errorRouteName;
		STORMANCER_DLL_API static const char* completeRouteName;
		static const char* cancellationRouteName;
	};
};
