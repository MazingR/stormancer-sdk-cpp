#include "stdafx.h"
#include "AES/AESDecryptStream.h"

namespace Stormancer
{
	AESDecryptStream::AESDecryptStream(const std::vector<byte>& key, byte* dataPtr, std::streamsize dataSize)
		: obytestream(dataPtr, dataSize)
		, _aes(IAES::createAES(key))
	{
	}

	AESDecryptStream::~AESDecryptStream()
	{
	}

	std::vector<byte> AESDecryptStream::bytes()
	{
		if (_aes)
		{
			obytestream stream;
			decrypt(&stream);
			return stream.bytes();
		}
		return std::vector<byte>();
	}

	void AESDecryptStream::decrypt(obytestream* stream)
	{
		if (_aes)
		{
			byte* dataPtr = ptr();
			std::streamsize dataSize = rdbuf()->in_avail();
			if (dataPtr != nullptr && dataSize > 0)
			{
				ibytestream ibs(dataPtr, dataSize);
				uint8 ivSize;
				ibs >> ivSize;

				byte* ivPtr = nullptr;
				if (ivSize > 0)
				{
					ivPtr = new byte[ivSize];
					ibs.read(ivPtr, ivSize);
				}

				uint32 encryptedSize;
				ibs >> encryptedSize;
				std::streamsize dataLeft = ibs.rdbuf()->in_avail();
				if (dataLeft >= encryptedSize)
				{
					byte* encryptedPtr = dataPtr + sizeof(ivSize) + ivSize + sizeof(encryptedSize);

					if (ivSize == _aes->getBlockSize())
					{
						_aes->decrypt(encryptedPtr, encryptedSize, ivPtr, ivSize, stream);
					}
				}

				if (ivPtr)
				{
					delete[] ivPtr;
				}
			}
		}
	}
}
