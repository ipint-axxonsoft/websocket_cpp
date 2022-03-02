cmake_minimum_required (VERSION 3.1)
project (ws_lib LANGUAGES CXX)

add_library(ws_lib INTERFACE)

target_sources(ws_lib INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include/websocket_cpp/ws.hpp")

target_include_directories(ws_lib INTERFACE "${PROJECT_SOURCE_DIR}/include/")



option(ENABLE_TESTS OFF)

if (ENABLE_TESTS)
	find_package (Boost COMPONENTS unit_test_framework REQUIRED)
	add_executable(test0 tests/test.cpp)
	target_link_libraries(test0 ws_lib ${Boost_LIBRARIES} ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY})
	add_test(NAME First_Test COMMAND test0)	
endif()