#pragma once

#include <functional>

namespace ws
{
	class DataMaskingHelper
	{
		const uint8_t* data_;
		size_t data_len_ = 0;
		unsigned key_;
	public:
		DataMaskingHelper(const uint8_t* data, size_t len, unsigned key)
			: data_(data), data_len_(len), key_(key)
		{
		}

		size_t Mask(char* out) {
			auto* ptr = data_;
			for (size_t i = 0; i < data_len_; ++i)
			{
				out[i] = ptr[i] ^ *((char*)(&key_) + i % 4 );
			}

			return data_len_;
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
		enum { BUFFER_SIZE = 2048 };
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

			wrapBuffer[1] |= datalen; // set PAYLOAD LEN

			uint32_t* maskingKey = (uint32_t*)(wrapBuffer + 2);
			uint32_t someRandomMaskingKey = 0x19; // random 32-bit MASKING KEY, need to be generated randomly
			*maskingKey = someRandomMaskingKey;

			resultWrapDataLen = 6;

			wrapCb_(wrapBuffer, resultWrapDataLen);
		}
	};

	class Server : public IWebSocket
	{
	public:
		Server(DataReadyCallback_t cb, DataWrapCallback_t wrapCb) : IWebSocket(cb, wrapCb) {}

		// Inherited via IWebSocket
		virtual void SubmitChunk(const char* data, size_t dataLen) override
		{
		}

		virtual void WrapData(const char* data, size_t datalen) override
		{
		}
	};
}