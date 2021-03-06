#include "test.h"




using namespace std::literals;

namespace Stormancer
{
	void Tester::execNextTest()
	{
		if (!_tests.empty())
		{
			auto test = _tests[0];
			_tests.pop_front();
			pplx::create_task(test).wait();
		}
		else
		{
			_logger->log(LogLevel::Info, "execNextTest", "TESTS SUCCESSFUL !");
			_testsDone = true;
		}
	}

	std::string Tester::connectionStateToString(ConnectionState connectionState)
	{
		std::string stateStr = to_string((int)connectionState) + " ";
		switch (connectionState)
		{
		case ConnectionState::Disconnected:
			stateStr += "Disconnected";
			break;
		case ConnectionState::Connecting:
			stateStr += "Connecting";
			break;
		case ConnectionState::Connected:
			stateStr += "Connected";
			break;
		case ConnectionState::Disconnecting:
			stateStr += "Disconnecting";
			break;
		}
		return stateStr;
	}

	void Tester::onMessage(Packetisp_ptr packet)
	{
		Serializer serializer;
		std::string message = serializer.deserializeOne<std::string>(packet->stream);

		_logger->log(LogLevel::Debug, "onMessage", "Message received", message);

		if (message == "echo")
		{
			_logger->log(LogLevel::Debug, "onMessage", "ECHO OK");
			execNextTest();
		}
		else if (message == "rpcServer_CancelOk")
		{
			_logger->log(LogLevel::Debug, "onMessage", "RPC SERVER CANCEL OK");
			execNextTest();
		}
		else if (message == "rpcClient_ExceptionOk")
		{
			_logger->log(LogLevel::Debug, "onMessage", "RPC CLIENT EXCEPTION OK");
			execNextTest();
		}
	}

	pplx::task<void> Tester::test_rpc_client_received(RpcRequestContext_ptr rc)
	{
		Serializer serializer;
		std::string message = serializer.deserializeOne<std::string>(rc->inputStream());

		_logger->log(LogLevel::Debug, "test_rpc_client", "RPC request received", message);

		if (message == "stormancer")
		{
			_logger->log(LogLevel::Debug, "test_rpc_client", "RPC CLIENT OK");
			_logger->log(LogLevel::Debug, "test_rpc_client", "sending rpc response...");
			rc->sendValue([=](obytestream* stream) {
				serializer.serialize(stream, message);
			});
			execNextTest();
			return pplx::task_from_result();
		}
		else
		{
			_logger->log(LogLevel::Error, "test_rpc_client", "RPC server failed", "Bad message");
			throw std::runtime_error("RPC server failed (bad message)");
		}
	}

	pplx::task<void> Tester::test_rpc_client_cancel_received(RpcRequestContext_ptr rc)
	{
		rc->cancellationToken().register_callback([this]() {
			_logger->log(LogLevel::Debug, "test_rpc_client", "RPC CLIENT CANCEL OK");
			execNextTest();
		});

		Serializer serializer;
		std::string message = serializer.deserializeOne<std::string>(rc->inputStream());

		_logger->log(LogLevel::Debug, "test_rpc_client", "RPC request received", message);

		taskDelay(1000ms, rc->cancellationToken()).wait();

		if (!rc->cancellationToken().is_canceled())
		{
			_logger->log(LogLevel::Error, "test_rpc_client", "RPC server should be cancelled");
			throw std::runtime_error("RPC server should be cancelled");
		}

		return pplx::task_from_result();
	}

	pplx::task<void> Tester::test_rpc_client_exception_received(RpcRequestContext_ptr rc)
	{
		throw std::runtime_error("An excepion occured!");
	}

	void Tester::test_connect()
	{
		_logger->log(LogLevel::Info, "test_connect", "CONNECT");

		try
		{
			auto onNext = [this](ConnectionState state) {
				try
				{
					auto stateStr = connectionStateToString(state);
					_logger->log(LogLevel::Debug, "test_connect", "Client connection state changed", stateStr.c_str());

					// Test: delete the client in connection state changed event
					if (state == ConnectionState::Disconnected)
					{
						_client.reset();
						_config.reset();
					}
				}
				catch (const std::exception& ex)
				{
					_logger->log(ex);
				}
			};

			auto onError = [this](std::exception_ptr exptr) {
				try
				{
					std::rethrow_exception(exptr);
				}
				catch (const std::exception& ex)
				{
					_logger->log(LogLevel::Error, "Test", "Client connection state change failed", ex.what());
				}
			};

			_client->getConnectionStateChangedObservable().subscribe(onNext, onError);

			_logger->log(LogLevel::Debug, "test_connect", "Get scene");

			_client->connectToPublicScene(_sceneName, [this](Scene_ptr scene) {
				_logger->log(LogLevel::Debug, "test_connect", "Get scene OK");

				auto onNext = [this, scene](ConnectionState state) {
					auto stateStr = connectionStateToString(state);
					_logger->log(LogLevel::Debug, "test_connect", "Scene connection state changed", stateStr.c_str());
				};

				auto onError = [this](std::exception_ptr exptr) {
					// On error
					try
					{
						std::rethrow_exception(exptr);
					}
					catch (const std::exception& ex)
					{
						_logger->log(LogLevel::Error, "Test", "Scene connection state change failed", ex.what());
					}
				};

				scene->getConnectionStateChangedObservable().subscribe(onNext, onError);

				_logger->log(LogLevel::Debug, "test_connect", "Add route");
				scene->addRoute("message", [this](Packetisp_ptr p) {
					onMessage(p);
				});
				_logger->log(LogLevel::Debug, "test_connect", "Add route OK");

				_logger->log(LogLevel::Debug, "test_connect", "Add procedure");
				auto rpcService = scene->dependencyResolver()->resolve<RpcService>();
				rpcService->addProcedure("rpc", [this](RpcRequestContext_ptr rc) { return test_rpc_client_received(rc); }, MessageOriginFilter::Host, true);
				rpcService->addProcedure("rpcCancel", [this](RpcRequestContext_ptr rc) { return test_rpc_client_cancel_received(rc); }, MessageOriginFilter::Host, true);
				rpcService->addProcedure("rpcException", [this](RpcRequestContext_ptr rc) { return test_rpc_client_exception_received(rc); }, MessageOriginFilter::Host, true);
				_logger->log(LogLevel::Debug, "test_connect", "Add procedure OK");

				_logger->log(LogLevel::Debug, "test_connect", "Connect to scene");
			}).then([this](pplx::task<Scene_ptr> task) {
				try
				{
					auto scene = task.get();
					_sceneMain = scene;

					auto connection = scene->hostConnection().lock();
					if (!connection)
					{
						throw std::runtime_error("Connection deleted");
					}

					_logger->log(LogLevel::Debug, "test_connect", "Connect to scene OK");
					_logger->log(LogLevel::Debug, "test_connect", "External addresses: " + connection->metadata("externalAddrs"));

					execNextTest();
				}
				catch (const std::exception& ex)
				{
					_logger->log(LogLevel::Error, "test_connect", "Failed to get and connect the scene.", ex.what());
				}
			});
		}
		catch (const std::exception& ex)
		{
			_logger->log(ex);
		}
	}

	void Tester::test_echo()
	{
		_logger->log(LogLevel::Info, "test_echo", "ECHO");

		try
		{
			auto scene = _sceneMain.lock();
			if (!scene)
			{
				_logger->log(LogLevel::Error, "StormancerWrapper", "scene deleted");
				return;
			}

			_logger->log(LogLevel::Debug, "test_echo", "Sending message...");
			scene->send("message", [](obytestream* stream) {
				Serializer serializer;
				serializer.serialize(stream, "echo");
			});
		}
		catch (std::exception& ex)
		{
			_logger->log(LogLevel::Error, "test_echo", "Can't send data to the scene.", ex.what());
		}
	}

	void Tester::test_rpc_server()
	{
		_logger->log(LogLevel::Info, "test_rpc_server", "RPC SERVER");

		auto scene = _sceneMain.lock();
		if (!scene)
		{
			_logger->log(LogLevel::Error, "StormancerWrapper", "scene deleted");
			return;
		}

		// get the RPC service
		auto rpcService = scene->dependencyResolver()->resolve<RpcService>();

		//int i = 123;
		//rpcService->rpc<void>("rpc2").then([]() { std::cout << "void 0" << std::endl; } );
		//rpcService->rpc<void>("rpc2", i).then([]() { std::cout << "void 1" << std::endl; } );
		//rpcService->rpc<void>("rpc2", i, i).then([]() { std::cout << "void 2" << std::endl; } );
		//rpcService->rpc<void>("rpc2", i, i, i).then([]() { std::cout << "void 3" << std::endl; } );
		//rpcService->rpc<int>("rpc2").then([](int j) { std::cout << "int 0" << j << std::endl; } );
		//rpcService->rpc<int>("rpc2", i).then([](int j) { std::cout << "int 1" << j << std::endl; } );
		//rpcService->rpc<int>("rpc2", i, i).then([](int j) { std::cout << "int 2" << j << std::endl; } );
		//rpcService->rpc<int>("rpc2", i, i, i).then([](int j) { std::cout << "int 3" << j << std::endl; } );

		//rpcService->rpcWriter("rpc2", [](bytestream* stream) {
		//	msgpack(stream, 123);
		//});

		//rpcService->rpcWriter<int>("rpc2", [](bytestream* stream) {
		//	(*stream) << 123;
		//}, [](Packetisp_ptr p) {
		//	int i;
		//	*p->stream >> i;
		//	return i;
		//});

		// We do an RPC on the server (by sending the string "stormancer") and get back a string response (that should be "stormancer" too)
		rpcService->rpc<std::string>("rpc", "stormancer")
			.then([this](pplx::task<std::string> t)
		{
			try
			{
				std::string response = t.get();

				_logger->log(LogLevel::Debug, "test_rpc_server", "rpc response received", response.c_str());

				if (response == "stormancer")
				{
					_logger->log(LogLevel::Debug, "test_rpc_server", "RPC SERVER OK");
				}
				else
				{
					_logger->log(LogLevel::Error, "test_rpc_server", "RPC SERVER FAILED", "Bad RPC response");
				}
				execNextTest();
			}
			catch (const std::exception& ex)
			{
				_logger->log(LogLevel::Error, "test_rpc_server", "RPC SERVER failed", ex.what());
			}
		});

		// Test disconnecting while doing an RPC ! =)
		//client->disconnect().then([](pplx::task<void> t) {
		//	try
		//	{
		//		t.get();
		//		logger->log(LogLevel::Info, "test_rpc_server", "Disconnect OK");
		//	}
		//	catch (std::exception ex)
		//	{
		//		logger->log(LogLevel::Error, "test_rpc_server", "Disconnect failed", ex.what());
		//	}
		//});
	}

	void Tester::test_rpc_server_cancel()
	{
		_logger->log(LogLevel::Info, "test_rpc_server_cancel", "RPC SERVER CANCEL");

		auto scene = _sceneMain.lock();
		if (!scene)
		{
			_logger->log(LogLevel::Error, "StormancerWrapper", "scene deleted");
			return;
		}

		auto rpcService = scene->dependencyResolver()->resolve<RpcService>();

		auto observable = rpcService->rpc_observable("rpcCancel", [](obytestream* stream) {
			Serializer serializer;
			serializer.serialize(stream, "stormancer2");
		});

		auto onNext = [this](Packetisp_ptr packet) {
			_logger->log(LogLevel::Error, "test_rpc_server_cancel", "rpc response received, but this RPC should be cancelled.");
		};

		auto onError = [this](std::exception_ptr exptr) {
			try {
				std::rethrow_exception(exptr);
			}
			catch (const std::exception& ex) {
				_logger->log(LogLevel::Debug, "test_rpc_server_cancel", "Rpc failed as expected", ex.what());
			}
		};

		auto onComplete = [this]() {
			_logger->log(LogLevel::Error, "test_rpc_server_cancel", "rpc complete received, but this RPC should be cancelled.");
		};

		auto subscription = observable.subscribe(onNext, onError, onComplete);

		subscription.add([=]() {
			_logger->log(LogLevel::Debug, "test_rpc_server_cancel", "RPC subscription unsubscribed");
		});

		taskDelay(30ms).then([this, subscription]() {
			if (subscription.is_subscribed())
			{
				subscription.unsubscribe();
				_logger->log(LogLevel::Debug, "test_rpc_server_cancel", "RPC cancel sent");
			}
			else
			{
				_logger->log(LogLevel::Error, "test_rpc_server_cancel", "subscription is empty");
			}
		});
	}

	void Tester::test_rpc_server_exception()
	{
		_logger->log(LogLevel::Info, "test_rpc_server_exception", "RPC SERVER EXCEPTION");

		auto scene = _sceneMain.lock();
		if (!scene)
		{
			_logger->log(LogLevel::Error, "StormancerWrapper", "scene deleted");
			return;
		}

		auto rpcService = scene->dependencyResolver()->resolve<RpcService>();

		rpcService->rpc<void>("rpcException").then([this](pplx::task<void> t) {
			try
			{
				t.get();
				_logger->log(LogLevel::Error, "test_rpc_server_exception", "Rpc should fail");
			}
			catch (const std::exception& ex)
			{
				_logger->log(LogLevel::Debug, "test_rpc_server_exception", "RPC SERVER EXCEPTION OK", ex.what());
				execNextTest();
			}
		});
	}

	void Tester::test_rpc_server_clientException()
	{
		_logger->log(LogLevel::Info, "test_rpc_server_clientException", "RPC SERVER CLIENT EXCEPTION");

		auto scene = _sceneMain.lock();
		if (!scene)
		{
			_logger->log(LogLevel::Error, "StormancerWrapper", "scene deleted");
			return;
		}

		auto rpcService = scene->dependencyResolver()->resolve<RpcService>();

		rpcService->rpc<void>("rpcClientException").then([this](pplx::task<void> t) {
			try
			{
				t.get();
				_logger->log(LogLevel::Error, "test_rpc_server_clientException", "Rpc should fail");
			}
			catch (const std::exception& ex)
			{
				_logger->log(LogLevel::Debug, "test_rpc_server_clientException", "RPC SERVER CLIENT EXCEPTION OK", ex.what());
				execNextTest();
			}
		});
	}

	void Tester::test_rpc_client()
	{
		_logger->log(LogLevel::Info, "test_rpc_client", "RPC CLIENT");

		auto scene = _sceneMain.lock();
		if (!scene)
		{
			_logger->log(LogLevel::Error, "StormancerWrapper", "scene deleted");
			return;
		}

		scene->send("message", [](obytestream* stream) {
			Serializer serializer;
			serializer.serialize(stream, "rpc");
		});
	}

	void Tester::test_rpc_client_cancel()
	{
		_logger->log(LogLevel::Info, "test_rpc_client_cancel", "RPC CLIENT CANCEL");

		auto scene = _sceneMain.lock();
		if (!scene)
		{
			_logger->log(LogLevel::Error, "StormancerWrapper", "scene deleted");
			return;
		}

		scene->send("message", [](obytestream* stream) {
			Serializer serializer;
			serializer.serialize(stream, "rpcCancel");
		});
	}

	void Tester::test_rpc_client_exception()
	{
		_logger->log(LogLevel::Info, "test_rpc_client_exception", "RPC CLIENT EXCEPTION");

		auto scene = _sceneMain.lock();
		if (!scene)
		{
			_logger->log(LogLevel::Error, "StormancerWrapper", "scene deleted");
			return;
		}

		scene->send("message", [](obytestream* stream) {
			Serializer serializer;
			serializer.serialize(stream, "rpcException");
		});
	}

	void Tester::test_syncClock()
	{
		_logger->log(LogLevel::Info, "test_syncclock", "SYNC CLOCK");

		_stop = false;
		pplx::create_task([this]()
		{
			while (!_stop && !_client->lastPing())
			{
				taskDelay(100ms).wait();
			}
			if (!_stop && _client->lastPing() > 0)
			{
				int64 clock = _client->clock();
				if (clock)
				{
					std::string clockStr = to_string(clock / 1000.0);
					_logger->log(LogLevel::Debug, "test_syncclock", "clock", clockStr.c_str());
					_logger->log(LogLevel::Debug, "test_syncclock", "SyncClock OK");
					execNextTest();
				}
			}
		});
	}

	void Tester::test_disconnect()
	{
		_logger->log(LogLevel::Info, "test_disconnect", "DISCONNECT");

		_client->disconnect()
			.then([this](pplx::task<void> t)
		{
			try
			{
				t.get();
				_logger->log(LogLevel::Debug, "test_disconnect", "Disconnect client OK");
				execNextTest();
			}
			catch (const std::exception& ex)
			{
				_logger->log(LogLevel::Error, "test_disconnect", "Failed to disconnect client", ex.what());
			}
		});
	}

	void Tester::test_clean()
	{
		_logger->log(LogLevel::Info, "test_clean", "CLEAN");

		_stop = true;

		_client = nullptr;
		_config = nullptr;

		_logger->log(LogLevel::Debug, "test_clean", "scene and client deleted");

		_testsPassed = true;

		execNextTest();
	}

	void Tester::run_all_tests()
	{
		run_all_tests_nonblocking();

		_stop = true;
	}

	void Tester::run_all_tests_nonblocking()
	{
		_testsDone = false;
		_testsPassed = false;

		// Some platforms require a Client to be created before using pplx::task
		_config = Configuration::create(_endpoint, _accountId, _applicationName);
		_config->logger = _logger;
		//_config->synchronisedClock = false;
		_client = Client::create(_config);

		_tests.push_back([this]() { test_connect(); });
		_tests.push_back([this]() { test_echo(); });
		_tests.push_back([this]() { test_rpc_server(); });
		_tests.push_back([this]() { test_rpc_server_cancel(); });
		_tests.push_back([this]() { test_rpc_server_exception(); });
		_tests.push_back([this]() { test_rpc_server_clientException(); });
		_tests.push_back([this]() { test_rpc_client(); });
		_tests.push_back([this]() { test_rpc_client_cancel(); });
		_tests.push_back([this]() { test_rpc_client_exception(); });
		_tests.push_back([this]() { test_syncClock(); });
		_tests.push_back([this]() { test_disconnect(); });
		_tests.push_back([this]() { test_clean(); });

		execNextTest();
	}

	bool Tester::tests_done()
	{
		return _testsDone;
	}

	bool Tester::tests_passed()
	{
		return _testsPassed;
	}
}
