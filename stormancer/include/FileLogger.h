#pragma once
#include "headers.h"
#include "ILogger.h"

namespace Stormancer
{
	/// Logger which writing in a file.
	class FileLogger : public ILogger
	{
	private:
		enum class FileLoggerMode
		{
			Deferred = 0,
			Immediate = 1
		};

	public:

		/// Constructor.
		STORMANCER_DLL_API FileLogger(const char* filepath = "", bool immediate = false);

		/// Destructor.
		STORMANCER_DLL_API virtual ~FileLogger();

	public:
	
		/// A basic message log.
		/// \message The message to log.
		void log(const char* message);
		
		/// A detailed message log.
		/// \param level The level.
		/// \param category The category (typically the source).
		/// \param message The message.
		/// \param data Some extra data.
		void log(LogLevel level, const char* category, const char* message, const char* data);
		
		/// Log details about an exception.
		/// \param e The exception.
		void log(const std::exception& ex);

	private:
	
		/// tries to open the log file on disk.
		bool tryOpenFile();

	private:

		std::mutex _mutex;

		/// Name of the log file.
		std::string _fileName;
		
		/// Stream to the log file.
		std::ofstream _myfile;

		/// immediate mode
		bool _immediate = false;
	};
};