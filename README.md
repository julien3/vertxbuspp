# Introduction
vertxbuspp is a C++11 [Vert.x 3.x][] event bus client using websocket only transport 
and consisting of two files (VertxBus header and source) plus headers only dependencies 
([Websocketpp][], [ASIO][] and [JsonCPP][]).

[Vert.x 3.x]: http://vert-x3.github.io/
[Websocketpp]: http://www.zaphoyd.com/websocketpp/
[ASIO]: https://think-async.com/
[JsonCPP]: https://github.com/open-source-parsers/jsoncpp

# Using vertxbuspp
Copy `VertxBus.h`, `VertxBus.cpp` and the folders `asio`, `websocketpp` and `json` to your project.
Add `asio/include`, `websocket` and `json` to your include dirs. Check the example 
`main.cpp` to open a connection and communicate with your Vert.x server.
You can build the example with cmake plus your favourite compiler and test it with `vertx run vertx_server_test/Server.java`.

# Notes
ASIO and JsonCPP have been patched to ensure a compatibility with Cygwin.
The example has been tested on Windows (Visual Studio 2013, Cygwin/G++), Linux and MacOS X.
