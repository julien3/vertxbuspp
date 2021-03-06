// Copyright 2015-2017 Julien Lehuraux

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

// http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "VertxBus.h"

int main(int argc, char* argv[]) {
    int iter_counter = 0;
    VertxBus vertxeb;
    vertxeb.Connect("http://localhost:8080/eventbus", 
    [&] {
      // OnOpen
      vertxeb.RegisterHandler("test.pub", VertxBus::ReplyHandler([&](const Json::Value& jmsg, const VertxBus::VertxBusReplier& vbr) {
        Json::StyledWriter writer;
        std::cout << "Received:\n" << writer.write(jmsg["body"]) << std::endl;

        if(++iter_counter == 5) vertxeb.Close();
      }));
    }, 
    [&](const std::error_code& ec) {
      // OnClose
      std::cout << "Connection closed." << std::endl;
    },
    [&](const std::error_code& ec, const Json::Value& jmsg_fail) {
      // OnFail
      std::cerr << "Connection failed: " << ec.message() << std::endl;

      if(!jmsg_fail.empty()) {
        Json::StyledWriter writer;
        std::cerr << writer.write(jmsg_fail) << std::endl;
      }
    });

    vertxeb.WaitClosed();

    return 0;
}
