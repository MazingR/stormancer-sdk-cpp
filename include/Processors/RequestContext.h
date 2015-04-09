#pragma once
#include "headers.h"
#include "Core/Packet.h"

namespace Stormancer
{
	class RequestContext
	{
	public:
		RequestContext(Packet<>* packet);
		virtual ~RequestContext();

	public:
		Packet<>* packet();
		byteStream* inputStream();
		bool isComplete();
		void send(function<void(byteStream*)> writer);
		void complete();
		void error(function<void(byteStream*)> writer);

	private:
		Packet<>* _packet;
		byte _requestId[2];
		byteBuffer* _buffer;
		byteStream* _stream;
		bool _didSendValues = false;
		bool _isComplete;
	};
};
