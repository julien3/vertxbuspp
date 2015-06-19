cmake_minimum_required (VERSION 2.8.4)

project (vbpp_publish C CXX)

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

include_directories ("../../vertxbuspp")
include_directories ("../../vertxbuspp/asio/include")
include_directories ("../../vertxbuspp/json")
include_directories ("../../vertxbuspp/websocketpp")
add_executable(vbpp_publish	main.cpp
				../../vertxbuspp/VertxBus.cpp ../../vertxbuspp/VertxBus.h
				../../vertxbuspp/json/jsoncpp.cpp)

if (NOT MSVC)
	if (APPLE)
		target_link_libraries ( vbpp_publish ${CMAKE_THREAD_LIBS_INIT} )

	else (APPLE)

		target_link_libraries ( vbpp_publish ${CMAKE_THREAD_LIBS_INIT} ${LIBUUID_LIBRARIES} )

	endif (APPLE)
endif (NOT MSVC)
