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

#ifndef __VERTXBUS_H__
#define __VERTXBUS_H__

#define ASIO_STANDALONE

#ifdef VERTXBUSPP_NO_RTTI
#define ASIO_NO_TYPEID
#endif

#define _WEBSOCKETPP_CPP11_INTERNAL_

#ifdef _MSC_VER
#pragma warning(disable : 4503)
#endif

#include <atomic>
#include <list>

#ifdef VERTXBUSPP_TLS
#ifdef OPENSSL_IS_BORINGSSL
extern "C" {
  #ifndef SSL_R_SHORT_READ
  #define SSL_R_SHORT_READ SSL_R_UNEXPECTED_RECORD
  #endif
  
  inline void CONF_modules_unload(int p) {}
}
#endif
#include <websocketpp/config/asio_client.hpp>
#else // VERTXBUSPP_TLS
#include <websocketpp/config/asio_no_tls_client.hpp>
#endif // VERTXBUSPP_TLS
#include <websocketpp/client.hpp>
#include <websocketpp/common/thread.hpp>
#include <json/json.h>

struct VertxBusOptions {
  unsigned int ping_interval;
};

static const VertxBusOptions DefaultVertxBusOptions = { 5000 };

enum VertxBusState {
  kConnecting = 0,
  kOpen,
  kClosing,
  kClosed
};

class VertxBus {
 public:
  class VertxBusReplier;
  typedef std::function<void(const Json::Value&, const VertxBusReplier&)> ReplyHandler;
  typedef std::function<void(const Json::Value&)> FailureHandler;
  
  class VertxBusReplier {
   public:
    VertxBusReplier();
    VertxBusReplier(VertxBus* vb, const std::string& address);
  
    void operator() (const Json::Value& reply, const ReplyHandler& reply_handler = ReplyHandler(), const FailureHandler& failure_handler = FailureHandler()) const;
  
   private:
    VertxBus*   vertx_bus_;
    std::string address_;
  };
  
  VertxBus();
  ~VertxBus();
  
  bool Connect(const std::string& url, 
               const std::function<void()>& on_open = std::function<void()>(), 
               const std::function<void(const std::error_code&)>& on_close = std::function<void(const std::error_code&)>(),
               const std::function<void(const std::error_code&, const Json::Value&)>& on_fail = std::function<void(const std::error_code&, const Json::Value&)>(),
               const VertxBusOptions& options = DefaultVertxBusOptions);
  
  inline bool Send(const std::string& address, const Json::Value& message, const ReplyHandler& reply_handler = ReplyHandler(), const FailureHandler& failure_handler = FailureHandler()) {
    return Send(address, message, Json::Value(), reply_handler, failure_handler);
  }
  
  inline bool Send(const std::string& address, const Json::Value& message, const Json::Value& headers, const ReplyHandler& reply_handler = ReplyHandler(), const FailureHandler& failure_handler = FailureHandler()) {
    return SendOrPub("send", address, message, headers, reply_handler, failure_handler);
  }
  
  inline bool Publish(const std::string& address, const Json::Value& message) {
    return Publish(address, message, Json::Value());
  }
  
  inline bool Publish(const std::string& address, const Json::Value& message, const Json::Value& headers) {
    return SendOrPub("publish", address, message, headers);
  }
  
  inline bool RegisterHandler(const std::string& address, const ReplyHandler& reply_handler, const FailureHandler& failure_handler = FailureHandler()) {
    return RegisterHandler(address, Json::Value(), reply_handler, failure_handler);
  }
  
  bool RegisterHandler(const std::string& address, const Json::Value& headers, const ReplyHandler& reply_handler, const FailureHandler& failure_handler = FailureHandler());

#ifdef VERTXBUSPP_NO_RTTI
  inline bool UnregisterHandler(const std::string& address) {
    return unregisterHandler(address, Json::Value());
  }

  bool UnregisterHandler(const std::string& address, const Json::Value& headers);
#else
  inline bool UnregisterHandler(const std::string& address, const ReplyHandler& reply_handler = ReplyHandler()) {
    return UnregisterHandler(address, Json::Value(), reply_handler);
  }
  
  bool UnregisterHandler(const std::string& address, const Json::Value& headers, const ReplyHandler& reply_handler = ReplyHandler());
#endif

  bool Close();
  void WaitClosed();
  
  inline VertxBusState ReadyState() const { return state_; }
  
  inline const Json::Value& GetDefaultHeaders() const { return default_headers_; }
  inline void SetDefaultHeaders(const Json::Value& default_headers) { default_headers_ = default_headers; }

 private:
#ifdef VERTXBUSPP_TLS
  typedef websocketpp::client<websocketpp::config::asio_tls_client> client_tls;
  typedef websocketpp::config::asio_tls_client::message_type::ptr message_ptr_tls;
  typedef websocketpp::lib::shared_ptr<asio::ssl::context> context_ptr_tls;
#endif

  typedef websocketpp::client<websocketpp::config::asio_client> client;
  typedef websocketpp::config::asio_client::message_type::ptr message_ptr;

  struct VertxReplyHandlers {
    ReplyHandler    reply_handler;
    FailureHandler  failure_handler;
  };

  void OnOpen(websocketpp::connection_hdl hdl);
  void OnClose(websocketpp::connection_hdl hdl);
  void OnFail(websocketpp::connection_hdl hdl);
  void OnMessagePlain(websocketpp::connection_hdl hdl, message_ptr msg);

#ifdef VERTXBUSPP_TLS
  void OnMessageTls(websocketpp::connection_hdl hdl, message_ptr_tls msg);
#endif

  void OnMessage(websocketpp::connection_hdl hdl, const std::string& payload);

#ifdef VERTXBUSPP_TLS
  context_ptr_tls OnTlsInit(websocketpp::connection_hdl hdl);
#endif

  bool SendOrPub(const std::string& send_or_pub, const std::string& address, const Json::Value& message, const Json::Value& headers, const ReplyHandler& reply_handler = ReplyHandler(), const FailureHandler& failure_handler = FailureHandler());
  bool SendPing();
  void StartPingInterval();
  void StopPingInterval();

  void MakeUUID(std::string& uuid);

  Json::Value MergeHeaders(Json::Value headers);

  // Members
  client client_;

#ifdef VERTXBUSPP_TLS
  client_tls client_tls_;
#endif

  websocketpp::lib::shared_ptr<websocketpp::lib::thread>  asio_thread_;
  websocketpp::lib::thread*                               ping_thread_;
  websocketpp::connection_hdl                             hdl_;

  std::atomic<VertxBusState>            state_;
  bool                                  do_ping_;
  websocketpp::lib::mutex               ping_lock_;
  websocketpp::lib::condition_variable  ping_cvwait_;
  VertxBusOptions                       options_;

  std::map<std::string, VertxReplyHandlers>             reply_handlers_;
  std::map<std::string, std::list<VertxReplyHandlers>>  handler_map_;

  websocketpp::lib::mutex rh_lock_;
  websocketpp::lib::mutex hm_lock_;

  std::function<void()>                                           on_open_;
  std::function<void(const std::error_code&)>                     on_close_;
  std::function<void(const std::error_code&, const Json::Value&)> on_fail_;

#ifdef VERTXBUSPP_TLS
  bool client_is_tls_;
#endif

  Json::Value default_headers_;
};

#endif
