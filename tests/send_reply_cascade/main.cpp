// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#include "VertxBus.h"

int main(int argc, char* argv[])
{
    Json::Value jval;
    Json::StyledWriter writer;
    VertxBus vertxeb;
    vertxeb.connect("http://localhost:8080/eventbus", 
    [&] {
        // OnOpen
        jval["message"] = "foo";
        vertxeb.send("test.echo", jval, [&](const Json::Value& jmsg, const VertxBus::VertxBusReplier& vbr) {
            std::cout << "Received:\n" << writer.write(jmsg) << std::endl;

            jval["message"] = "bar";
            vbr(jval, [&](const Json::Value& jmsg2, const VertxBus::VertxBusReplier& vbr2) {
                std::cout << "Received:\n" << writer.write(jmsg2) << std::endl;

                jval["message"] = "foobar";
                vbr2(jval, [&](const Json::Value& jmsg3, const VertxBus::VertxBusReplier& vbr3) {
                    std::cout << "Received:\n" << writer.write(jmsg3) << std::endl;

                    jval["message"] = "barfoo";
                    vbr3(jval, [&](const Json::Value& jmsg4, const VertxBus::VertxBusReplier& vbr4) {
                        std::cout << "Received:\n" << writer.write(jmsg4) << std::endl;

                        jval.removeMember("message");
                        vbr4(jval, [&](const Json::Value& jmsg5, const VertxBus::VertxBusReplier& vbr5) {
                            std::cout << "Received:\n" << writer.write(jmsg5) << std::endl;

                            vertxeb.close();
                        });
                    });
                });
            });
        });
    }, 
    [&] {
        // OnClose
        std::cout << "Connection closed." << std::endl;
    },
    [&] (const std::error_code& ec, const Json::Value& jmsg_fail) {
        // OnFail
        std::cerr << "Connection failed: " << ec.message() << std::endl;

        if (!jmsg_fail.empty())
        {
            Json::StyledWriter writer;
            std::cerr << writer.write(jmsg_fail) << std::endl;
        }
    });

    vertxeb.waitClosed();

    return 0;
}
