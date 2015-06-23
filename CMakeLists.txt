cmake_minimum_required (VERSION 2.8.4)

project (vertxbuspp_examples C CXX)

set (SSL_EXAMPLE OFF CACHE BOOL "Add SSL example (requires OpenSSL)")
set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

add_subdirectory(examples/send)
add_subdirectory(examples/send_reply_cascade)
add_subdirectory(examples/publish)

if (SSL_EXAMPLE)
	add_subdirectory(examples/ssl_tls)
endif (SSL_EXAMPLE)
