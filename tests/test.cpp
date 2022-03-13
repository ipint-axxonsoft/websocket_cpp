#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>

#include <bitset>

#include <websocket_cpp/ws.hpp>

BOOST_AUTO_TEST_CASE(DataMaskingHelper_test0)
{
    const size_t DATA_LEN = 5; // 4 chars + trailing zero
    const char data[DATA_LEN] = "Test";
    char outBuffer[DATA_LEN];
    int key = 0xaabbccdd;
    auto resLen = ws::DataMaskingHelper((uint8_t*)data, DATA_LEN, key).Mask((uint8_t*)outBuffer);

    const char maskedData[] = { (const char)0x89, (const char)0xa9, (const char)0xc8, (const char)0xde, (const char)0xDD }; // manually masked data for test :)

    BOOST_CHECK(resLen == DATA_LEN); // masking should not affect the original size

    BOOST_CHECK(memcmp(outBuffer, maskedData, DATA_LEN) == 0);

    char demaskedBuffer[DATA_LEN];
    auto demaskResLen = ws::DataDemaskingHelper((uint8_t*)outBuffer, DATA_LEN, key).Demask((uint8_t*)demaskedBuffer);
    BOOST_CHECK(demaskResLen == DATA_LEN); // de-masking should not affect the original size
    BOOST_CHECK(strcmp(data, demaskedBuffer) == 0); // after reversed mask operation the result buffer should be match the orig. input
}

BOOST_AUTO_TEST_CASE(DataMaskingHelper_test1)
{
    const size_t DATA_LEN = 5; // 4 chars + trailing zero
    const char data[DATA_LEN] = "Test";
    char outBuffer[DATA_LEN];
    int key = 0xff;
    auto resLen = ws::DataMaskingHelper((uint8_t*)data, DATA_LEN, key).Mask((uint8_t*)outBuffer);
    char demaskedBuffer[DATA_LEN];
    auto demaskResLen = ws::DataDemaskingHelper((uint8_t*)outBuffer, DATA_LEN, key).Demask((uint8_t*)demaskedBuffer);

    BOOST_CHECK(demaskResLen == DATA_LEN);
    BOOST_CHECK(strcmp(data, demaskedBuffer) == 0);
}

BOOST_AUTO_TEST_CASE(Client_test0)
{
    const size_t DATA_BUFFER_LEN = 2048;
    char wrapDataBuffer[DATA_BUFFER_LEN];
    char plainDataBuffer[DATA_BUFFER_LEN];

    memset(wrapDataBuffer, 0, DATA_BUFFER_LEN);
    memset(plainDataBuffer, 0, DATA_BUFFER_LEN);
    size_t wrapDataLen = 0;

    auto dataReadyCb = [plainDataBuffer](const char* data, size_t len) {};
    auto wrapCb = [&wrapDataBuffer, &wrapDataLen](const char* wrappedData, size_t len) {
        memcpy(wrapDataBuffer, wrappedData, len);
		wrapDataLen = len;
    };
    
    ws::Client c(dataReadyCb, wrapCb);

    char plainText[] = "Hello, World!"; // it's a short text, for the payload length should be sufficient 7bit scheme
    const size_t PLAIN_TEXT_LEN = strlen(plainText) + 1; // +1 for trailing 0
    c.WrapData(plainText, PLAIN_TEXT_LEN);

    BOOST_CHECK(wrapDataLen == 20);

    BOOST_CHECK(std::bitset<8>(wrapDataBuffer[0]).test(7) == 1); // check FIN bit is 1
    
    BOOST_CHECK(std::bitset<4>(wrapDataBuffer[0]) == 0x2); // check OP code, we use only binary
    
    BOOST_CHECK(std::bitset<8>(wrapDataBuffer[1]).test(7) == 1); // check MASK code, for client it's always true
    
    BOOST_CHECK(std::bitset<7>(wrapDataBuffer[1]) == PLAIN_TEXT_LEN); // check payload len
    
    BOOST_CHECK(*((uint32_t*)(wrapDataBuffer + 2)) == c.usedMaskingKey()); // just check some MASKING KEY is set, should be 32 bit value
}

BOOST_AUTO_TEST_CASE(Client_test1)
{
    const size_t DATA_BUFFER_LEN = 65536;
    char wrapDataBuffer[DATA_BUFFER_LEN];
    char plainDataBuffer[DATA_BUFFER_LEN];

    memset(wrapDataBuffer, 0, DATA_BUFFER_LEN);
    memset(plainDataBuffer, 0, DATA_BUFFER_LEN);
    size_t wrapDataLen = 0;

    auto dataReadyCb = [plainDataBuffer](const char* data, size_t len) {};
    auto wrapCb = [&wrapDataBuffer, &wrapDataLen](const char* wrappedData, size_t len) {
        memcpy(wrapDataBuffer, wrappedData, len);
        wrapDataLen = len;
    };

   ws::Client c(dataReadyCb, wrapCb);

    const size_t DATA_LEN = 50000;
    char plainText[DATA_LEN]; // some large data to check 7+16 bits payload scheme
    memset(plainText, 97, DATA_LEN);
    c.WrapData(plainText, DATA_LEN);
    BOOST_CHECK(std::bitset<8>(wrapDataBuffer[0]).test(7) == 1); // check FIN bit is 1

    BOOST_CHECK(std::bitset<4>(wrapDataBuffer[0]) == 0x2); // check OP code, we use only binary

    BOOST_CHECK(std::bitset<8>(wrapDataBuffer[1]).test(7) == 1); // check MASK code, for client it's always true

    // check 7 + 16 payload length scheme
    BOOST_CHECK(std::bitset<7>(wrapDataBuffer[1]) == 126);
    BOOST_CHECK(*(uint16_t*)(wrapDataBuffer + 2) == DATA_LEN);

    BOOST_CHECK(*(uint32_t*)(wrapDataBuffer + 4) != 0); // just check some MASKING KEY is set, should be 32 bit value
}

BOOST_AUTO_TEST_CASE(Server_test0)
{
    const char wrappedData[] = { (const char)0x82, (const char)0x8e, (const char)0xff, (const char)0x00, (const char)0x00, (const char)0x00, (const char)0xb7, (const char)0x65, (const char)0x6c, (const char)0x6c, (const char)0x90, (const char)0x2c, (const char)0x20, (const char)0x57, (const char)0x90, (const char)0x72, (const char)0x6c, (const char)0x64, (const char)0xde, (const char)0x00 };
    const size_t WRAPPED_DATA_LEN = 20;

    char plainDataBuffer[255];
    memset(plainDataBuffer, 0, 255);
    size_t plainDataLen = 0;

    auto dataReadyCb = [&plainDataBuffer, &plainDataLen](const char* data, size_t len) { memcpy(plainDataBuffer, data, len), plainDataLen = len; };
    auto wrapCb = [](const char* data, size_t len) {};

    ws::Server s(dataReadyCb, wrapCb);

    s.SubmitChunk(wrappedData, WRAPPED_DATA_LEN);
    
    BOOST_CHECK(s.recMaskingKey() == 0xff);

    BOOST_CHECK(s.recPayloadLen() == 14); // Hello, World! + trailing zero = 14
    BOOST_CHECK(plainDataLen == 14); // just check some MASKING KEY is set, should be 32 bit value

    BOOST_CHECK(strcmp(plainDataBuffer, "Hello, World!") == 0); // just check some MASKING KEY is set, should be 32 bit value
}