# Introduction
vertxbuspp is a C++11 [Vert.x 3.x][] event bus client using websocket only transport 
and consisting of two files (VertxBus header and source) plus headers only dependencies 
([Websocketpp][], [ASIO][] and [JsonCPP][]).

[Vert.x 3.x]: http://vert-x3.github.io/
[Websocketpp]: http://www.zaphoyd.com/websocketpp/
[ASIO]: https://think-async.com/
[JsonCPP]: https://github.com/open-source-parsers/jsoncpp

# Using vertxbuspp
* Copy the content of vertxbuspp: `VertxBus.h`, `VertxBus.cpp` and the folders `asio`, `websocketpp` and `json` to your project.
* Add `asio/include`, `websocket` and `json` to your include dirs and compile `jsoncpp.cpp` and `VertxBus.cpp`. 
* Check the tests to open a connection and communicate with your Vert.x server.
* You can build the tests with cmake plus your favourite compiler and test them with `vertx run Server.java`.

# Notes
ASIO and JsonCPP have been patched to ensure a compatibility with Cygwin.
The tests have been tested on Windows (Visual Studio 2013, Cygwin/G++), Linux and MacOS X.
