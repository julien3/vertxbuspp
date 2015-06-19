cmake_minimum_required (VERSION 2.8.4)

set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

project (vertxbuspp_tests C CXX)

add_subdirectory(tests/send)
add_subdirectory(tests/send_reply_cascade)
add_subdirectory(tests/publish)
