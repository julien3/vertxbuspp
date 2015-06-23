cmake_minimum_required (VERSION 2.8.4)

project (vbpp_ssl_tls C CXX)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVERTXBUSPP_TLS")

if (MSVC)
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_SCL_SECURE_NO_WARNINGS")
else (MSVC)
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++11")
	
	if (CYGWIN)
		set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_GNU_SOURCE")
	endif (CYGWIN)
	
	find_package (Threads REQUIRED)

	if (NOT APPLE)
		find_package (LibUuid REQUIRED)
		include_directories (${LIBUUID_INCLUDE_DIRS})

	endif (NOT APPLE)
endif (MSVC)

find_package (OpenSSL REQUIRED)
include_directories (${OPENSSL_INCLUDE_DIR})

include_directories ("../../vertxbuspp")
include_directories ("../../vertxbuspp/asio/include")
include_directories ("../../vertxbuspp/json")
include_directories ("../../vertxbuspp/websocketpp")
add_executable(vbpp_ssl_tls	main.cpp
				../../vertxbuspp/VertxBus.cpp ../../vertxbuspp/VertxBus.h
				../../vertxbuspp/json/jsoncpp.cpp)

if (NOT MSVC)
	if (APPLE)
		target_link_libraries ( vbpp_ssl_tls ${CMAKE_THREAD_LIBS_INIT} ${OPENSSL_LIBRARIES} )

	else (APPLE)

		target_link_libraries ( vbpp_ssl_tls ${CMAKE_THREAD_LIBS_INIT} ${LIBUUID_LIBRARIES} ${OPENSSL_LIBRARIES} )

	endif (APPLE)
else (NOT MSVC)
	target_link_libraries ( vbpp_ssl_tls ${OPENSSL_LIBRARIES} )

endif (NOT MSVC)
