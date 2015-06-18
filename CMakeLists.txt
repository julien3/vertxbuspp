cmake_minimum_required (VERSION 2.8.4)

set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

project (vertxbuspp_test C CXX)

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

include_directories ("asio/include")
include_directories ("json")
include_directories ("websocketpp")
add_executable(vertxbuspp_test 	main.cpp
								VertxBus.cpp VertxBus.h
								json/jsoncpp.cpp)

if (NOT MSVC)
	if (APPLE)
		target_link_libraries ( vertxbuspp_test ${CMAKE_THREAD_LIBS_INIT} )
	else (APPLE)
		target_link_libraries ( vertxbuspp_test ${CMAKE_THREAD_LIBS_INIT} ${LIBUUID_LIBRARIES} )
	endif (APPLE)
endif (NOT MSVC)
