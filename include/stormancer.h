#pragma once

#include "headers.h"

#include "basic_bytebuf.h"
#include "basic_bytestream.h"

#include "Dto/ConnectionData.h"
#include "Dto/ConnectToSceneMsg.h"
#include "Dto/ConnectionResult.h"
#include "Dto/EmptyDto.h"
#include "Dto/RouteDto.h"
#include "Dto/SceneInfosDto.h"
#include "Dto/SceneInfosRequestDto.h"

#include "ApiClient.h"
#include "Client.h"
#include "ClientConfiguration.h"
#include "ConnectionState.h"
#include "ILogger.h"
#include "NullLogger.h"
#include "FileLogger.h"
#include "DefaultPacketDispatcher.h"
#include "Helpers.h"
#include "IConnection.h"
#include "IConnectionManager.h"
#include "IPacketProcessor.h"
#include "IRequest.h"
#include "IScenePeer.h"
#include "ISerializable.h"
#include "ITransport.h"
#include "MessageIDTypes.h"
#include "Packet.h"
#include "PacketProcessorConfig.h"
#include "RequestContext.h"
#include "RequestModuleBuilder.h"
#include "RequestProcessor.h"
#include "Route.h"
#include "Scene.h"
#include "SceneDispatcher.h"
#include "SceneEndpoint.h"
#include "ScenePeer.h"
#include "IPacketDispatcher.h"
#include "IRequestModule.h"
#include "ITokenHandler.h"
#include "TokenHandler.h"

//#include "Plugins/IClientPlugin.h"
//#include "Plugins/PluginBuildContext.h"
//#include "Plugins/Request.h"
//#include "Plugins/RpcClientPlugin.h"
//#include "Plugins/RpcRequestContext.h"
//#include "Plugins/RpcService.h"

#include "Transports/RakNetConnection.h"
#include "Transports/RakNetTransport.h"
