#pragma once

#include <boost/asio/streambuf.hpp>

#include <functional>
#include <bitset>

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
				out[i] = ptr[i] ^ *((uint8_t*)(&key_) + i % 4); // mask data as said in RFC6455 5.3. Client-to-Server Masking
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
		char wrapBuffer_[BUFFER_SIZE];

		uint32_t usedMaskingKey_ = 0;
	public:

		Client(DataReadyCallback_t cb, DataWrapCallback_t wrapCb) : IWebSocket(cb, wrapCb)
		{
		}

		// Inherited via IWebSocket
		virtual void SubmitChunk(const char* data, size_t dataLen) override
		{
		}

		virtual void WrapData(const char* data, size_t datalen) override
		{
			size_t resultWrapDataLen = 0;

			memset(wrapBuffer_, 0, BUFFER_SIZE);

			wrapBuffer_[0] |= 1U << 7; // set FIN bit
			
			wrapBuffer_[0] |= 0x2; // set OP CODE. 0x2 is binary format
			
			wrapBuffer_[1] |= 1U << 7; // set MASK flag

			resultWrapDataLen += 1;

			if (datalen < 126) // max payload len for 2 ^ 7 - 2
			{
				wrapBuffer_[1] |= datalen; // set PAYLOAD LEN
				resultWrapDataLen += 1;
			}
			else if (datalen < 65536) // max payload len for 2 ^ 16
			{
				wrapBuffer_[1] |= 126;
				*(uint16_t*)(wrapBuffer_ + 2) = datalen; // next 2 bytes reserved for the payload size 
				resultWrapDataLen += 3;
			}
			else
			{
				throw std::runtime_error("Not implemented yet 7+16+64 payload length scheme");
			}

			uint32_t* maskingKey = (uint32_t*)(wrapBuffer_ + resultWrapDataLen);
			usedMaskingKey_ = 0xff; // random 32-bit MASKING KEY, need to be generated randomly
			*maskingKey = usedMaskingKey_;

			resultWrapDataLen += 4;

			// MASKING data
			DataMaskingHelper((const uint8_t*)data, datalen, usedMaskingKey_).Mask((uint8_t*)(wrapBuffer_ + resultWrapDataLen));
			resultWrapDataLen += datalen;

			wrapCb_(wrapBuffer_, resultWrapDataLen);
		}

		uint32_t usedMaskingKey() const
		{
			return usedMaskingKey_;
		}
	};

	class Server : public IWebSocket
	{
		boost::asio::streambuf innerBuffer_;

		enum { BUFFER_SIZE = 65536 };
		char payloadBuffer_[BUFFER_SIZE];

		char wrapBuffer_[BUFFER_SIZE];

		uint64_t payloadLen_ = 0;
		uint32_t maskingKey_ = 0;

	public:
		Server(DataReadyCallback_t cb, DataWrapCallback_t wrapCb) : IWebSocket(cb, wrapCb) {}

		// Inherited via IWebSocket
		virtual void SubmitChunk(const char* data, size_t dataLen) override
		{
			auto bufs = innerBuffer_.prepare(dataLen);
			memcpy(bufs.data(), data, dataLen);
			innerBuffer_.commit(dataLen);

			if (innerBuffer_.size() < 2) // not enough data received
				return;

			const uint8_t* wsHeaderPtr = (const uint8_t*)innerBuffer_.data().data();

			if (std::bitset<8>(wsHeaderPtr[0]).test(7) == false)
				throw std::runtime_error("FIN != 0 not supported yet!");
			
			if (std::bitset<4>(wsHeaderPtr[0]) != 2)
				throw std::runtime_error("Opcodes except 2 is not supported yet!");

			if (std::bitset<8>(wsHeaderPtr[1]).test(7) != 1)
				throw std::runtime_error("Client frame always should be masked!");

			size_t headerPtrOffset = 1;

			payloadLen_ = std::bitset<7>(*(wsHeaderPtr + 1)).to_ulong();
			if (payloadLen_ < 126)
			{
				headerPtrOffset += 1;
			}
			else if (payloadLen_ == 126)
			{
				payloadLen_ = *(uint16_t*)(wsHeaderPtr + 2);
				headerPtrOffset += 3;
			}
			else
			{
				throw std::runtime_error("Payload scheme 7 + 16 + 64 is not supported yet!");
			}

			if (innerBuffer_.size() < headerPtrOffset + 4 + payloadLen_) // not enough data received to read @maskingKey_ and payload
				return;

			maskingKey_ = 0;
			maskingKey_ = *(uint32_t*)(wsHeaderPtr + headerPtrOffset);
			headerPtrOffset += 4;

			auto len = DataDemaskingHelper((const uint8_t*)(wsHeaderPtr + headerPtrOffset), payloadLen_, maskingKey_).Demask((uint8_t*)payloadBuffer_);
			
			innerBuffer_.consume(headerPtrOffset + payloadLen_);

			cb_(payloadBuffer_, len);
		}

		virtual void WrapData(const char* data, size_t datalen) override
		{
			size_t resultWrapDataLen = 0;

			memset(wrapBuffer_, 0, BUFFER_SIZE);

			wrapBuffer_[0] |= 1U << 7; // set FIN bit

			wrapBuffer_[0] |= 0x2; // set OP CODE. 0x2 is binary format

			resultWrapDataLen += 1;

			if (datalen < 126) // max payload len for 2 ^ 7 - 2
			{
				wrapBuffer_[1] |= datalen; // set PAYLOAD LEN
				resultWrapDataLen += 1;
			}
			else if (datalen < 65536) // max payload len for 2 ^ 16
			{
				wrapBuffer_[1] |= 126;
				*(uint16_t*)(wrapBuffer_ + 2) = datalen; // next 2 bytes reserved for the payload size 
				resultWrapDataLen += 3;
			}
			else
			{
				throw std::runtime_error("Not implemented yet 7+16+64 payload length scheme");
			}

			memcpy(wrapBuffer_ + resultWrapDataLen, data, datalen);
			resultWrapDataLen += datalen;

			wrapCb_(wrapBuffer_, resultWrapDataLen);
		}

		uint64_t recPayloadLen() const
		{
			return payloadLen_;
		}

		uint32_t recMaskingKey() const // used for tests, just save and return the last received masking key
		{
			return maskingKey_;
		}
	};
}