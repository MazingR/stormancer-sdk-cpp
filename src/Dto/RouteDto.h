#pragma once
#include "headers.h"

namespace Stormancer
{
	/// Information about a scene route.
	struct RouteDto
	{
	public:
		RouteDto();
		RouteDto(bytestream* stream);
		RouteDto(MsgPack::Element* element);
		virtual ~RouteDto();

	public:
		void serialize(bytestream* stream);
		void deserialize(bytestream* stream);

	public:
		wstring Name;
		uint16 Handle;
		stringMap Metadata;
	};
};