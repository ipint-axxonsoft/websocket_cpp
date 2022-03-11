#pragma once

#include <functional>

namespace ws
{
	class DataMaskingHelper
	{
		const uint8_t* data_;
		size_t data_len_ = 0;
		uint32_t key_;
	public:
		DataMaskingHelper(const uint8_t* data, size_t len, uint32_t key)
			: data_(data), data_len_(len), key_(key)
		{
		}

		size_t Mask(uint8_t* out) {
			auto* ptr = data_;
			for (size_t i = 0; i < data_len_; ++i)
			{
				out[i] = ptr[i] ^ *((char*)(&key_) + i % 4 );
			}

			return data_len_;
		}
	};

	class DataDemaskingHelper : private DataMaskingHelper
	{
	public:
		DataDemaskingHelper(const uint8_t* data, size_t len, uint32_t key)
			: DataMaskingHelper(data, len, key)
		{
		}

		size_t Demask(uint8_t* out) {
			return Mask(out);
		}
	};

	class IWebSocket
	{
	public:
		using DataReadyCallback_t = std::function<void(const char* data, size_t dataLen)>;
		using DataWrapCallback_t = std::function<void(const char* data, size_t dataLen)>;

	protected:
		DataReadyCallback_t cb_;
		DataWrapCallback_t wrapCb_;

	public:
		IWebSocket(DataReadyCallback_t cb, DataWrapCallback_t wrapCb)
			: cb_(cb), wrapCb_(wrapCb)
		{
		}

		virtual void SubmitChunk(const char* data, size_t dataLen) = 0;
		virtual void WrapData(const char* data, size_t datalen) = 0;
	};

	class Client : public IWebSocket
	{
		enum { BUFFER_SIZE = 65536 };
		char wrapBuffer[BUFFER_SIZE];
	public:

		Client(DataReadyCallback_t cb, DataWrapCallback_t wrapCb) : IWebSocket(cb, wrapCb)
		{
			memset(wrapBuffer, 0, BUFFER_SIZE);
		}

		// Inherited via IWebSocket
		virtual void SubmitChunk(const char* data, size_t dataLen) override
		{
		}

		virtual void WrapData(const char* data, size_t datalen) override
		{
			size_t resultWrapDataLen = 0;

			wrapBuffer[0] |= 1U << 7; // set FIN bit
			
			wrapBuffer[0] |= 0x2; // set OP CODE. 0x2 is binary format
			
			wrapBuffer[1] |= 1U << 7; // set MASK flag

			resultWrapDataLen += 1;

			if (datalen < 126) // max payload len for 2 ^ 7 - 2
			{
				wrapBuffer[1] |= datalen; // set PAYLOAD LEN
				resultWrapDataLen += 2;
			}
			else if (datalen < 65536) // max payload len for 2 ^ 16
			{
				wrapBuffer[1] |= 126;
				*(uint16_t*)(wrapBuffer + 2) |= datalen; // next 2 bytes reserved for the payload size 
				resultWrapDataLen += 3;
			}
			else
			{
				throw std::runtime_error("Not implemented yet 7+16+64 payload length scheme");
			}

			uint32_t* maskingKey = (uint32_t*)(wrapBuffer + resultWrapDataLen);
			uint32_t someRandomMaskingKey = 0xff; // random 32-bit MASKING KEY, need to be generated randomly
			*maskingKey = someRandomMaskingKey;

			resultWrapDataLen += 4;

			// MASKING data
			unsigned key = 0xff; // TODO, a key should be randomly generated
			DataMaskingHelper((const uint8_t*)data, datalen, someRandomMaskingKey).Mask((uint8_t*)(wrapBuffer + resultWrapDataLen));
			resultWrapDataLen += datalen;

			wrapCb_(wrapBuffer, resultWrapDataLen);
		}
	};

	class Server : public IWebSocket
	{
		enum { BUFFER_SIZE = 65536 };
		char dataBuffer_[BUFFER_SIZE];
		size_t bufferLen = 0;

		uint64_t inPayloadLen = 0;

	public:
		Server(DataReadyCallback_t cb, DataWrapCallback_t wrapCb) : IWebSocket(cb, wrapCb) {}

		// Inherited via IWebSocket
		virtual void SubmitChunk(const char* data, size_t dataLen) override
		{
			if (std::bitset<8>(data[0]).test(7) == false)
				throw std::runtime_error("FIN != 0 not supported yet!");
			
			if (std::bitset<4>(data[0]) != 2)
				throw std::runtime_error("Opcodes except 2 is not supported yet!");

			if (std::bitset<8>(data[1]).test(7) != 1)
				throw std::runtime_error("Client frame always should be masked!");

			size_t headerPointer = 0;

			inPayloadLen = std::bitset<7>(*(data + 1)).to_ulong();
			if (inPayloadLen < 126)
			{
				headerPointer = 2;
			}
			else if (inPayloadLen == 126)
			{
				inPayloadLen = *(uint16_t*)(data + 2);
				headerPointer = 4;
			}
			else
			{
				throw std::runtime_error("Payload scheme 7 + 16 + 64 is not supported yet!");
			}

			uint32_t maskingKey = 0;
			maskingKey = *(uint32_t*)(data + headerPointer);
			headerPointer += 4;

			bufferLen = DataDemaskingHelper((const uint8_t*)(data + headerPointer), inPayloadLen, maskingKey).Demask((uint8_t*)dataBuffer_);

			cb_(dataBuffer_, bufferLen);
		}

		virtual void WrapData(const char* data, size_t datalen) override
		{
		}

		uint64_t recPayloadLen() const
		{
			return inPayloadLen;
		}
	};
}