#pragma once

#include <windows.h>
#include <bcrypt.h>
#include "stormancer/headers.h"
#include "stormancer/AES/IAES.h"

namespace Stormancer
{
	class KeyStore;
	class AESWindows : public IAES
	{
	public:

#pragma region public_methods

		AESWindows(std::shared_ptr<KeyStore> keyStore);

		~AESWindows();

		

		void encrypt(byte* dataPtr, std::streamsize dataSize, byte* ivPtr, std::streamsize ivSize, obytestream* outputStream, uint64 keyId) override;

		void decrypt(byte* dataPtr, std::streamsize dataSize, byte* ivPtr, std::streamsize ivSize, obytestream* outputStream, uint64 keyId) override;

		void generateRandomIV(byte* ivPtr) override;

		virtual std::streamsize getBlockSize() override;

#pragma endregion

#pragma region private_methods

		void initAES();

		void cleanAES();

#pragma endregion

#pragma region private_members

		std::shared_ptr<KeyStore>  _key;

		BCRYPT_ALG_HANDLE _hAesAlg = NULL;

		std::unordered_map<uint64, BCRYPT_KEY_HANDLE> _keyHandles;
		std::unordered_map<uint64, PBYTE> _keyPointers;
		
		DWORD _cbBlockLen = 0;
		BCRYPT_AUTH_TAG_LENGTHS_STRUCT authTagLengths;
#pragma endregion
	};
}
