#pragma once
#include "headers.h"
#include "IPacketProcessor.h"

namespace Stormancer
{
	/// Dispatch the network messages to the right scene.
	class SceneDispatcher : public IPacketProcessor
	{
		friend class Client;

	public:
		SceneDispatcher();
		virtual ~SceneDispatcher();

	public:
		void registerProcessor(PacketProcessorConfig& config);
		void addScene(Scene* scene);
		void removeScene(uint8 sceneHandle);

	private:
		bool handler_impl(uint8 sceneHandle, shared_ptr<Packet<>> packet);

	private:
		processorFunction* handler = nullptr;
		vector<Scene*> _scenes;
	};
};
