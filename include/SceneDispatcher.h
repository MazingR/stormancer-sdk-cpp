#pragma once
#include "headers.h"
#include "IPacketProcessor.h"

namespace Stormancer
{
	class SceneDispatcher : public IPacketProcessor
	{
	public:
		SceneDispatcher();
		virtual ~SceneDispatcher();

	public:
		void registerProcessor(PacketProcessorConfig* config);
		void addScene(Scene* scene);
		void removeScene(byte sceneHandle);

	private:
		bool handler_impl(byte sceneHandle, Packet<>* packet);

	private:
		processorFunction* handler = nullptr;
		vector<Scene*> _scenes;
	};
};
