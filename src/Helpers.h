#pragma once
#include "headers.h"
#include "basic_bytebuf.h"
#include "basic_bytestream.h"

namespace Stormancer
{
#pragma region flux

	// vector flux operators
	
	template<typename T>
	vector<T>& operator<<(vector<T>& v, const T& data)
	{
		v.push_back(data);
		return v;
	}

	template<typename T>
	vector<T>& operator<<(vector<T>& v, const T&& data)
	{
		v.push_back(data);
		return v;
	}

	template<typename T>
	vector<T>& operator>>(vector<T>& v, T& data)
	{
		data = v.pop_back();
		return v;
	}

	template<typename T>
	bytestream& operator<<(bytestream& bs, T& data)
	{
#ifdef _IS_BIG_ENDIAN
		T tmp(data);
		reverseByteOrder(&tmp);
		bs.write((char*)&tmp, sizeof(T));
#else
		bs.write((char*)&data, sizeof(T));
#endif
		return bs;
	}

	template<typename T>
	bytestream& operator<<(bytestream& bs, T&& data)
	{
		return (bs << T(data));
	}

	/// Write a c-string in a byte stream.
	/// \param bs The byte stream we want to write in.
	/// \param data The c-string to write.
	///	\return The byte stream.
	STORMANCER_DLL_API bytestream& operator<<(bytestream& bs, const char* data);

	/// Write a constant std::string in a byte stream.
	/// \param bs The byte stream we want to write in.
	/// \param data The std::string to write.
	/// \return The byte stream.
	STORMANCER_DLL_API bytestream& operator<<(bytestream& bs, const string& data);

	/// Write a c-string in a byte stream.
	/// \param bs The byte stream we want to write in.
	/// \param data The c-string to write.
	/// \return The byte stream.
	STORMANCER_DLL_API bytestream& operator<<(bytestream& bs, string& data);

	/// Write a wide c-string in a byte stream.
	/// \param bs The byte stream we want to write in.
	/// \param data The wide c-string to write.
	/// \return The byte stream.
	STORMANCER_DLL_API bytestream& operator<<(bytestream& bs, const wchar_t* data);

	/// Write a constant std::wstring in a byte stream.
	/// \param bs The byte stream we want to write in.
	/// \param data The std::wstring to write.
	/// \return The byte stream.
	STORMANCER_DLL_API bytestream& operator<<(bytestream& bs, const wstring& data);

	/// Write a std::wstring in a byte stream.
	/// \param bs The byte stream we want to write in.
	/// \param data The std::wstring to write.
	/// \return The byte stream.
	STORMANCER_DLL_API bytestream& operator<<(bytestream& bs, wstring& data);

	/// Template for reading any data type from the byte stream.
	/// \param bs The byte stream we want to read.
	/// \param data A ref to a variable for puting the read data.
	template<typename T>
	bytestream& operator>>(bytestream& bs, T& data)
	{
		char* tmp = (char*)&data;
		bs.read(tmp, sizeof(T));
#ifdef _IS_BIG_ENDIAN
		reverseByteOrder(&data);
#endif
		return bs;
	}

	/// Read a std::string from a byte stream.
	/// \param bs The byte stream we want to read.
	/// \param data A ref to a std::string where we get the data.
	STORMANCER_DLL_API bytestream& operator>>(bytestream& bs, string& data);

	/// Read a std::wstring from a byte stream.
	/// \param bs The byte stream we want to read.
	/// \param data A ref to a std::wstring where we get the data.
	STORMANCER_DLL_API bytestream& operator>>(bytestream& bs, wstring& data);

	template<typename T>
	T* reverseByteOrder(T* data, size_t n = -1)
	{
		char* tmp = (char*)data;
		std::reverse(tmp, tmp + (n >= 0 ? n : sizeof(T)));
		return data;
	}

#pragma endregion

	namespace Helpers
	{
#pragma region map

		anyMap stringMapToAnyMap(stringMap& sm);

		template<typename TKey, typename TValue>
		vector<TKey> mapKeys(map<TKey, TValue>& map)
		{
			vector<TKey> vec;
			for (auto it = map.begin(); it != map.end(); ++it)
			{
				vec.push_back(it->first);
			}
			return vec;
		}

		template<typename TKey, typename TValue>
		vector<TKey*> mapKeysPtr(map<TKey, TValue>& map)
		{
			vector<TKey*> vec;
			for (auto it = map.begin(); it != map.end(); ++it)
			{
				vec.push_back(&it->first);
			}
			return vec;
		}

		template<typename TKey, typename TValue>
		vector<TValue> mapValues(map<TKey, TValue>& map)
		{
			vector<TValue> vec;
			for (auto it = map.begin(); it != map.end(); ++it)
			{
				vec.push_back(it->second);
			}
			return vec;
		}

		template<typename TKey, typename TValue>
		vector<TValue*> mapValuesPtr(map<TKey, TValue>& map)
		{
			vector<TValue*> vec;
			for (auto it = map.begin(); it != map.end(); ++it)
			{
				vec.push_back(&it->second);
			}
			return vec;
		}

		/// Returns a boolean indicating if the map contains the key.
		template<typename TKey, typename TValue>
		bool mapContains(map<TKey, TValue>& map, TKey& key)
		{
			return (map.find(key) != map.end()) ? true : false;
		}

#pragma endregion

#pragma region string
		
		/// Join a string vector by using a glue string.
		/// \param vector The vector containing the strings to join.
		/// \param glue A glue string. Default is an empty string.
		STORMANCER_DLL_API wstring vectorJoin(vector<wstring>& vector, wstring glue = L"");

		/// Split a string to a vector by using a separator string.
		/// \param str The string to split.
		/// \param separator the separator to detect in the string.
		STORMANCER_DLL_API vector<wstring> stringSplit(const wstring& str, const wstring separator);

		/// Trim a specific character from a string.
		/// \param str The string to trim.
		/// \param ch the character to remove from the string. Default is space.
		STORMANCER_DLL_API wstring stringTrim(wstring& str, wchar_t ch = ' ');

		/// Convert any type of data to a std::wstring.
		/// \param data Data to convert.
		/// \return The Converter std::wstring.
		template<typename T>
		wstring to_wstring(T data)
		{
			return to_wstring(to_string(data));
		}

		/// Convert a c-string to a std::wstring.
		/// \param str A c-string.
		STORMANCER_DLL_API wstring to_wstring(const char* str);

		/// Convert a std::string to a std::wstring.
		/// \param str A std::string.
		STORMANCER_DLL_API wstring to_wstring(string str);

		/// Convert any type of data to a std::string.
		template<typename T>
		string to_string(T& data)
		{
			return std::to_string(data);
		}

		/// Convert a std::wstring to a std::string.
		STORMANCER_DLL_API string to_string(wstring& str);

		/// Convert a vector of bytes to a std::string.
		STORMANCER_DLL_API string to_string(vector<byte>& v);

		/// Convert one data type to another data type by using the constructor.
		template<typename T1, typename T2>
		T2 convert(T1& data)
		{
			return T2(data);
		}

		/// Convert a std::string to a vector of bytes.
		template<>
		vector<byte> convert<string, vector<byte>>(string& str);

#pragma endregion

#pragma region task

		pplx::task<void> taskCompleted();

		template<typename T>
		pplx::task<T> taskCompleted(T result)
		{
			task_completion_event<T> tce;
			tce.set(result);
			return create_task(tce);
		}

		template<typename T>
		pplx::task<T> taskFromException(exception& ex)
		{
			task_completion_event<T> tce;
			tce.set_exception(ex);
			return create_task(tce);
		}

		pplx::task<void> taskIf(bool condition, function<pplx::task<void>()> action);

#pragma endregion

#pragma region stream

		bytestream* convertRakNetPacketToStream(RakNet::Packet* packet);

		void deleteStringBuf(stringbuf* sb);

		template<typename T, typename U>
		void streamCopy(T* fromStream, U* toStream)
		{
			uint32 n = static_cast<uint32>(fromStream->rdbuf()->in_avail());
			char* c = new char[n];
			fromStream->readsome(c, n);
			toStream->write(c, n);
			delete[] c;
		}

#pragma endregion

#pragma region time

		time_t STORMANCER_DLL_API nowTime_t();
		wstring STORMANCER_DLL_API time_tToStr(time_t& time, bool local = false);
		wstring STORMANCER_DLL_API time_tToStr(time_t& time, const char* format);
		wstring STORMANCER_DLL_API nowStr(bool local = false);
		wstring STORMANCER_DLL_API nowStr(const char* format);
		wstring STORMANCER_DLL_API nowDateStr(bool local = false);
		wstring STORMANCER_DLL_API nowTimeStr(bool local = false);

#pragma endregion

#pragma region other

		bool ensureSuccessStatusCode(int statusCode);

		template<typename T>
		uint64 ptrToUint64(T* ptr)
		{
			return *static_cast<uint64*>(static_cast<void*>(ptr));
		}

#pragma endregion
	};
};