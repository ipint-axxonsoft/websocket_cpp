#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>
#include <websocket_cpp/ws.hpp>

BOOST_AUTO_TEST_CASE(my_test)
{
    BOOST_CHECK_EQUAL(ws::foo(1, 3), 4);
}