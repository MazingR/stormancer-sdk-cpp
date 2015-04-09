#pragma once
#include "headers.h"
#include "ITransport.h"
#include "Infrastructure/IPacketDispatcher.h"

namespace Stormancer
{
	class ClientConfiguration
	{
	public:
		ClientConfiguration(wstring account, wstring application);
		~ClientConfiguration();

		wstring getApiEndpoint();
		ClientConfiguration& setMetadata(wstring key, wstring value);
		//void addPlugin(IClientPlugin* plugin);

	public:
		wstring account;
		wstring application;
		wstring serverEndpoint;
		IPacketDispatcher* dispatcher;
		ITransport* transport;
		vector<ISerializer*> serializers;
		uint16 maxPeers;
		//vector<IClientPlugin*> plugins;
		stringMap metadata;

	private:
		wstring apiEndpoint = L"http://localhost:8081/";
	};
};
