#pragma once
#include "headers.h"
#include "Configuration.h"
#include "ApiClient.h"
#include "SceneEndpoint.h"
#include "Scene.h"
#include "ILogger.h"
#include "SceneDispatcher.h"
#include "RequestProcessor.h"
#include "IConnection.h"
#include "IScheduler.h"
#include "Watch.h"
#include "ITokenHandler.h"

namespace Stormancer
{
	class Client;

	/// Manage the connection to the scenes of an application.
	class Client
	{
		/// The scene need to access some private method of the client.
		friend class Scene;

	private:
		class ConnectionHandler : public IConnectionManager
		{
		public:
			ConnectionHandler();
			~ConnectionHandler();

		public:
			uint64 generateNewConnectionId();
			void newConnection(IConnection* connection);
			IConnection* getConnection(uint64 id);
			void closeConnection(IConnection* connection, std::string reason);
		};

	private:
		struct ClockValue
		{
			double latency;
			double offset;
		};

	private:
		Client(Configuration* config);

	public:
		/// Destructor.
		~Client();

	private:
		/// Copy constructor deleted.
		Client(Client& other) = delete;

		/// Copy operator deleted.
		Client& operator=(Client& other) = delete;

	public:
		/// Factory.
		/// \param config A pointer to a Configuration.
		STORMANCER_DLL_API static Client* createClient(Configuration* config);

		/// Get a public scene.
		/// \param sceneId The scene Id as a string.
		/// \param userData Some custom user data as a string.
		/// \return a task to the connection to the scene.
		STORMANCER_DLL_API pplx::task<Result<Scene*>*> getPublicScene(const char* sceneId, const char* userData = "");

		STORMANCER_DLL_API pplx::task<Result<Scene*>*> getScene(const char* token);

		/// Returns the name of the application.
		STORMANCER_DLL_API std::string applicationName();

		/// Returns the used logger.
		STORMANCER_DLL_API ILogger* logger();

		/// Set the logger
		STORMANCER_DLL_API void setLogger(ILogger* logger);

		/// Disconnect and close all connections.
		/// this method returns nothing. So it's useful for application close.
		STORMANCER_DLL_API pplx::task<Result<>*> disconnect(bool immediate = false);

		STORMANCER_DLL_API int64 clock();

		STORMANCER_DLL_API int64 lastPing();

		STORMANCER_DLL_API DependencyResolver* dependencyResolver() const;

		STORMANCER_DLL_API Action<ConnectionState>& connectionStateChangedAction();

		STORMANCER_DLL_API Action<ConnectionState>::TIterator onConnectionStateChanged(std::function<void(ConnectionState)> callback);

		STORMANCER_DLL_API ConnectionState connectionState() const;

		STORMANCER_DLL_API void SetMedatata(const char* key, const char* value);
	private:

		void initialize();

		/// Connect to a scene
		/// \param scene The scene to connect to.
		/// \param token Application token.
		/// \param localRoutes Local routes declared.
		/// \return A task which complete when the connection is done.
		pplx::task<void> connectToScene(Scene* scene, std::string& token, std::vector<Route_ptr> localRoutes);

		/// Disconnect from a scene.
		/// \param scene The scene.
		/// \param sceneHandle Scene handle.
		/// \return A task which complete when the disconnection is done.
		pplx::task<void> disconnect(Scene* scene, byte sceneHandle, bool immediate = false);

		/// Disconnect from all scenes.
		pplx::task<void> disconnectAllScenes(bool immediate = false);

		/// Get scene informations for connection.
		/// \param sceneId Scene id.
		/// \param sep Scene Endpoint retrieved by ApiClient::getSceneEndpoint
		/// \return A task which complete when the scene informations are retrieved.
		pplx::task<Scene*> getScene(std::string sceneId, SceneEndpoint sep);

		/// Called by the transport when a packet has been received.
		/// \param packet Received packet.
		void transport_packetReceived(Packet_ptr packet);

		/// Send a system request to the server for important operations.
		/// \param id Request id.
		/// \param parameter Data to send with the request.
		/// \return A task which complete when the response is available (As result of the task).
		template<typename T1, typename T2>
		pplx::task<T2> sendSystemRequest(byte id, T1& parameter)
		{
			return _requestProcessor->sendSystemRequest(_serverConnection, id, [this, &parameter](bytestream* stream) {
				// serialize request dto
				msgpack::pack(stream, parameter);
				//auto res = stream->str();
			}).then([this](pplx::task<Packet_ptr> t) {
				Packet_ptr packet;

				try
				{
					packet = t.get();
				}
				catch (const std::exception& ex)
				{
					throw std::runtime_error(std::string() + "System request failed (" + ex.what() + ")");
				}

				// deserialize result dto
				std::string buffer;
				*packet->stream >> buffer;
				msgpack::unpacked result;
				msgpack::unpack(&result, buffer.data(), buffer.size());
				T2 t2;
				result.get().convert(&t2);
				return t2;
			});
		}

		pplx::task<void> updateServerMetadata();

		void startSyncClock();

		void stopSyncClock();

		pplx::task<void> syncClockImpl();

		void dispatchEvent(std::function<void(void)> ev);

	private:
		bool _initialized = false;
		const std::function<void(std::function<void(void)>)> _eventDispatcher;
		ILogger* _logger = nullptr;
		std::string _accountId;
		std::string _applicationName;
		IConnection* _serverConnection = nullptr;
		IScheduler* _scheduler = nullptr;
		ITransport* _transport = nullptr;
		ITokenHandler* _tokenHandler = nullptr;
		ApiClient* _apiClient = nullptr;
		RequestProcessor* _requestProcessor = nullptr;
		SceneDispatcher* _scenesDispatcher = nullptr;
		pplx::cancellation_token_source _cts;
		uint16 _maxPeers = 0;
		stringMap _metadata;
		IPacketDispatcher* _dispatcher = nullptr;
		Watch _watch;
		double _offset = 0;
		int64 _lastPing = 0;
		bool _synchronisedClock = true;
		int64 _synchronisedClockInterval = 5000;
		int64 _pingIntervalAtStart = 100;
		bool lastPingFinished = true;
		std::mutex _syncClockMutex;
		std::deque<ClockValue> _clockValues;
		double _medianLatency = 0;
		double _standardDeviationLatency = 0;
		uint16 _maxClockValues = 24;
		ISubscription* _syncClockSubscription = nullptr;
		std::map<std::string, Scene*> _scenes;
		DependencyResolver* _dependencyResolver = nullptr;
		std::vector<IPlugin*> _plugins;
		std::mutex _connectionMutex;
		pplx::task<void> _connectionTask;
		bool _connectionTaskSet = false;
		Action<ConnectionState> _onConnectionStateChanged;
	};
};