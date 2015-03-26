#include "stormancer.h"

namespace Stormancer
{
	// ConnectionHandler

	/*uint64 Client::ConnectionHandler::generateNewConnectionId()
	{
		// TODO
		return _current++;
	}

	void Client::ConnectionHandler::newConnection(shared_ptr<IConnection*> connection)
	{
		throw new exception("Not Implemented");
	}

	shared_ptr<IConnection*> Client::ConnectionHandler::getConnection(uint64 id)
	{
		throw new exception("Not Implemented");
	}

	void Client::ConnectionHandler::closeConnection(shared_ptr<IConnection*> connection, wstring reason)
	{
		throw new exception("Not Implemented");
	}*/


	// Client

	Client::Client(ClientConfiguration& config)
		: _accountId(config.account),
		_applicationName(config.application),
		_tokenHandler(new TokenHandler()),
		_apiClient(config, _tokenHandler),
		//_transport(config.transport),
		//_dispatcher(config.dispatcher),
		//_requestProcessor(RequestProcessor(_logger, list<shared_ptr<IRequestModule*>>())),
		//_scenesDispatcher(SceneDispatcher()),
		_metadata(config.metadata),
		_maxPeers(config.maxPeers)
	{
		/*for each (ISerializer& serializer in config.serializers)
		{
			_serializers.insert(serializer.name, serializer);
		}*/

		//_metadata["serializers"] = Helpers::vectorJoin(Helpers::mapKeys(_serializers), ",");
		//_metadata["transport"] = _transport.name;
		_metadata[L"version"] = L"1.0.0a";
		_metadata[L"platform"] = L"cpp";

		initialize();
	}

	Client::~Client()
	{
	}

	void Client::initialize()
	{

	}

	wstring Client::applicationName()
	{
		return _applicationName;
	}

	/*shared_ptr<ILogger*> Client::logger()
	{
		return _logger;
	}

	void Client::setLogger(shared_ptr<ILogger*> logger)
	{
		if ((*logger) != nullptr)
		{
			_logger = logger;
		}
		else
		{
			_logger = DefaultLogger::instance();
		}
	}*/

	task<Scene> Client::getPublicScene(wstring sceneId, wstring userData)
	{
		return _apiClient.getSceneEndpoint(_accountId, _applicationName, sceneId, userData).then([&](SceneEndpoint& sep) {
			return getScene(sceneId, sep);
		});
	}

	task<Scene> Client::getScene(wstring sceneId, SceneEndpoint sep)
	{
		throw string("Not implemented");
	}
};
