# Introduction
vertxbuspp is a C++11 [Vert.x 3.x][] event bus client using websocket only transport 
and consisting of two files (VertxBus header and source) plus lightweight main dependencies 
([Websocketpp][], [ASIO][] and [JsonCPP][]).

[Vert.x 3.x]: http://vert-x3.github.io/
[Websocketpp]: http://www.zaphoyd.com/websocketpp/
[ASIO]: https://think-async.com/
[JsonCPP]: https://github.com/open-source-parsers/jsoncpp

# Using vertxbuspp
* Copy the content of vertxbuspp: `VertxBus.h`, `VertxBus.cpp` and the folders `asio`, `websocketpp` and `json` to your project.
* Add `asio/include`, `websocket` and `json` to your include dirs and compile `jsoncpp.cpp` and `VertxBus.cpp`. 
* Check the examples to open a connection and communicate with your Vert.x server.
* You can build the examples with cmake plus your favourite compiler and test them with `vertx run Server.java`.

# Using vertxbuspp with TLS support
* Link with `libssl/OpenSSL`.
* Add `VERTXBUSPP_TLS` to your preprocessor definitions.
* The example `ssl_tls` can be added to the cmake build with `SSL_EXAMPLE=ON` (on Windows, don't forget to copy your OpenSSJL DLLs to avoid an Ordinal 313 error).

# Dependencies
* Windows / Visual Studio : `rpcrt4.lib` (already pragma'ed).
* Windows / Cygwin : `libuuid`, `pthread`
* Linux : `libuuid`, `pthread`
* MacOSX : `pthread`
* Add `OpenSSL` to your dependencies if using TLS support.

# Notes
ASIO and JsonCPP have been patched to ensure a compatibility with Cygwin.  
The examples have been tested on Windows (Visual Studio 2013, Cygwin), Linux and MacOSX.
