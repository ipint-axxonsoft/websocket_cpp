#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>

#include <bitset>

#include <websocket_cpp/ws.hpp>

BOOST_AUTO_TEST_CASE(DataMaskingHelper_test0)
{
    const size_t DATA_LEN = 5;
    const char data[DATA_LEN] = "Test";
    char outBuffer[DATA_LEN];
    int key = 33;
    auto resLen = ws::DataMaskingHelper((uint8_t*)data, 5, key).Mask(outBuffer);

    BOOST_CHECK(resLen == DATA_LEN); // just check some MASKING KEY is set, should be 32 bit value
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

    auto c = new ws::Client(dataReadyCb, wrapCb);

    char plainText[] = "Hello, World!";
    size_t plainTextLen = strlen(plainText);
    c->WrapData(plainText, plainTextLen);
    BOOST_CHECK(std::bitset<8>(wrapDataBuffer[0]).test(7) == 1); // check FIN bit is 1
    
    BOOST_CHECK(std::bitset<4>(wrapDataBuffer[0]) == 0x2); // check OP code, we use only binary
    
    BOOST_CHECK(std::bitset<8>(wrapDataBuffer[1]).test(7) == 1); // check MASK code, for client it's always true
    
    BOOST_CHECK(std::bitset<7>(wrapDataBuffer[1]) == plainTextLen); // check payload len
    
    BOOST_CHECK(wrapDataLen == 6);

    BOOST_CHECK(*((uint32_t*)wrapDataBuffer + 2) == 0x19); // just check some MASKING KEY is set, should be 32 bit value

    delete c;
}