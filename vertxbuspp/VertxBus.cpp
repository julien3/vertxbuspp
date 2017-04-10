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

#ifdef _WIN32
#pragma comment(lib, "rpcrt4.lib")
#include <Rpc.h>
#else
#include <uuid/uuid.h>
#endif

bool startsWith(const std::string& str, const std::string& start) {
  return str.length() >= start.length() && str.compare(0, start.length(), start) == 0;
}

bool endsWith(const std::string& str, const std::string& end) {
  return str.length() >= end.length() && str.compare(str.length() - end.length(), end.length(), end) == 0;
}

VertxBus::VertxBusReplier::VertxBusReplier() 
    : vertx_bus_(NULL) {}

VertxBus::VertxBusReplier::VertxBusReplier(VertxBus* vb, const std::string& address)
    : vertx_bus_(vb),
      address_(address) {}

void VertxBus::VertxBusReplier::operator() (const Json::Value& reply, const ReplyHandler& reply_handler, const FailureHandler& failure_handler) const {
  if (!address_.empty()) vertx_bus_->Send(address_, reply, reply_handler, failure_handler);
}

VertxBus::VertxBus()
    :
#ifdef VERTXBUSPP_TLS
      client_is_tls_(false),
#endif
      asio_thread_(NULL),
      ping_thread_(NULL),
      state_(kClosed),
      do_ping_(false) {
  // Initialize the Asio transport policy
  client_.init_asio();
  
  // set up access channels to log nothing
  client_.clear_access_channels(websocketpp::log::alevel::all);
  client_.set_access_channels(websocketpp::log::alevel::connect);
  client_.set_access_channels(websocketpp::log::alevel::disconnect);
  client_.set_access_channels(websocketpp::log::alevel::app);
  
  // set up error channel to log nothing
  client_.clear_error_channels(websocketpp::log::elevel::all);
  
  // Bind the handlers we are using
  client_.set_open_handler(websocketpp::lib::bind(&VertxBus::OnOpen, this, websocketpp::lib::placeholders::_1));
  client_.set_close_handler(websocketpp::lib::bind(&VertxBus::OnClose, this, websocketpp::lib::placeholders::_1));
  client_.set_fail_handler(websocketpp::lib::bind(&VertxBus::OnFail, this, websocketpp::lib::placeholders::_1));
  client_.set_message_handler(websocketpp::lib::bind(&VertxBus::OnMessagePlain, this, websocketpp::lib::placeholders::_1, websocketpp::lib::placeholders::_2));

#ifdef VERTXBUSPP_TLS
  // Initialize the Asio transport policy
  client_tls_.init_asio();
  
  // set up access channels to log nothing
  client_tls_.clear_access_channels(websocketpp::log::alevel::all);
  client_tls_.set_access_channels(websocketpp::log::alevel::connect);
  client_tls_.set_access_channels(websocketpp::log::alevel::disconnect);
  client_tls_.set_access_channels(websocketpp::log::alevel::app);
  
  // set up error channel to log nothing
  client_tls_.clear_error_channels(websocketpp::log::elevel::all);
  
  // Bind the handlers we are using
  client_tls_.set_open_handler(websocketpp::lib::bind(&VertxBus::OnOpen, this, websocketpp::lib::placeholders::_1));
  client_tls_.set_close_handler(websocketpp::lib::bind(&VertxBus::OnClose, this, websocketpp::lib::placeholders::_1));
  client_tls_.set_fail_handler(websocketpp::lib::bind(&VertxBus::OnFail, this, websocketpp::lib::placeholders::_1));
  client_tls_.set_message_handler(websocketpp::lib::bind(&VertxBus::OnMessageTls, this, websocketpp::lib::placeholders::_1, websocketpp::lib::placeholders::_2));
  
  client_tls_.set_tls_init_handler(websocketpp::lib::bind(&VertxBus::OnTlsInit, this, websocketpp::lib::placeholders::_1));
#endif
}

VertxBus::~VertxBus() {
  if (state_ == kOpen) Close();
  WaitClosed();
}

bool VertxBus::Connect(const std::string& url, 
                       const std::function<void()>& on_open, 
                       const std::function<void(const std::error_code&)>& on_close, 
                       const std::function<void(const std::error_code&, const Json::Value&)>& on_fail, 
                       const VertxBusOptions& options) {
  if (state_ != kClosed) {
#ifdef VERTXBUSPP_TLS
    if (client_is_tls_) {
      client_tls_.get_alog().write(websocketpp::log::alevel::app, "Connection Error: not in CLOSE state");
    } else
#endif
    {
      client_.get_alog().write(websocketpp::log::alevel::app, "Connection Error: not in CLOSE state");
    }
        
    return false;
  }

  if (asio_thread_) {
    asio_thread_->join();
  }

#ifdef VERTXBUSPP_TLS
  if (client_is_tls_) {
    client_tls_.reset();
  } else
#endif
  {
    client_.reset();
  }

  on_open_ = on_open;
  on_close_ = on_close;
  on_fail_ = on_fail;
  options_ = options;
  state_ = kConnecting;

  std::string final_url = url; 
  std::transform(final_url.begin(), final_url.end(), final_url.begin(), ::tolower);
  if (!endsWith(final_url, "/websocket")) final_url += "/websocket";

#ifdef VERTXBUSPP_TLS
  client_is_tls_ = startsWith(final_url, "https") || startsWith(final_url, "wss");
#endif

  websocketpp::lib::error_code ec;

#ifdef VERTXBUSPP_TLS
  if (client_is_tls_) {
    client_tls::connection_ptr con = client_tls_.get_connection(final_url, ec);
    
    if (ec) {
      client_tls_.get_alog().write(websocketpp::log::alevel::app, "Connection Error: " + ec.message());
      return false;
    }

    // Grab a handle for this connection so we can talk to it in a thread
    // safe manor after the event loop starts.
    hdl_ = con->get_handle();

    // Queue the connection. No DNS queries or network connections will be
    // made until the io_service event loop is run.
    client_tls_.connect(con);
  } else
#endif
  {
    client::connection_ptr con = client_.get_connection(final_url, ec);
    
    if (ec) {
      client_.get_alog().write(websocketpp::log::alevel::app, "Connection Error: " + ec.message());
      return false;
    }

    // Grab a handle for this connection so we can talk to it in a thread
    // safe manor after the event loop starts.
    hdl_ = con->get_handle();

    // Queue the connection. No DNS queries or network connections will be
    // made until the io_service event loop is run.
    client_.connect(con);
  }

  // Create a thread to run the ASIO io_service event loop
#ifdef VERTXBUSPP_TLS
  if (client_is_tls_) {
    client_tls_.start_perpetual();
    asio_thread_.reset(new websocketpp::lib::thread(&client_tls::run, &client_tls_));
  } else
#endif
  {
    client_.start_perpetual();
    asio_thread_.reset(new websocketpp::lib::thread(&client::run, &client_));
  }

  return true;
}

bool VertxBus::RegisterHandler(const std::string& address, const Json::Value& headers, const ReplyHandler& reply_handler, const FailureHandler& failure_handler) {
  if (state_ != kOpen || !reply_handler) return false;
  
  hm_lock_.lock();
  std::list<VertxReplyHandlers>& handlers = handler_map_[address];
  bool do_reg = handlers.empty();
  handlers.push_back({ reply_handler, failure_handler });
  if (do_reg) {
    hm_lock_.unlock();
    websocketpp::lib::error_code ec;
    Json::FastWriter writer;
    Json::Value val;
    val["type"] = "register";
    val["address"] = address;
    val["headers"] = MergeHeaders(headers);

#ifdef VERTXBUSPP_TLS
    if (client_is_tls_) {
      client_tls_.send(hdl_, writer.write(val), websocketpp::frame::opcode::text, ec);
    } else
#endif
    {
      client_.send(hdl_, writer.write(val), websocketpp::frame::opcode::text, ec);
    }

    return !ec;
  }  
  hm_lock_.unlock();

  return true;
}

#ifdef VERTXBUSPP_NO_RTTI
bool VertxBus::UnregisterHandler(const std::string& address, const Json::Value& headers) {
#else
bool VertxBus::UnregisterHandler(const std::string& address, const Json::Value& headers, const ReplyHandler& reply_handler) {
#endif
  if (state_ != kOpen) return false;
  
  hm_lock_.lock();
  std::list<VertxReplyHandlers>& handlers = handler_map_[address];
  if (!handlers.empty()) {

#ifdef VERTXBUSPP_NO_RTTI
    handlers.clear();
#else
    if (!reply_handler) {
      handlers.clear();
    } else {
      void(*const* rh_ptr)(const Json::Value&, const VertxBusReplier&) = reply_handler.target<void(*)(const Json::Value&, const VertxBusReplier&)>();

      if (rh_ptr) {
        bool bfound = false;
        
        for (std::list<VertxReplyHandlers>::iterator it = handlers.begin(); !bfound && it != handlers.end(); it++) {
          void(*const* it_ptr)(const Json::Value&, const VertxBusReplier&) = (*it).reply_handler.target<void(*)(const Json::Value&, const VertxBusReplier&)>();
          
          if (it_ptr && *it_ptr == *rh_ptr) {
              handlers.erase(it);
              bfound = true;
          }
        }
      }
    }
#endif

    if (handlers.empty()) {
      hm_lock_.unlock();

      websocketpp::lib::error_code ec;
      Json::FastWriter writer;
      Json::Value val;
      val["type"] = "unregister";
      val["address"] = address;
      val["headers"] = MergeHeaders(headers);

#ifdef VERTXBUSPP_TLS
      if (client_is_tls_) {
        client_tls_.send(hdl_, writer.write(val), websocketpp::frame::opcode::text, ec);
      } else
#endif
      {
        client_.send(hdl_, writer.write(val), websocketpp::frame::opcode::text, ec);
      }

    return !ec;
    }
  }
  hm_lock_.unlock();

  return true;
}

bool VertxBus::Close() {
  if (state_ != kOpen) return false;
  
  websocketpp::lib::error_code ec;

#ifdef VERTXBUSPP_TLS
  if (client_is_tls_) {
    client_tls_.close(hdl_, websocketpp::close::status::normal, "", ec);
  } else
#endif
  {
    client_.close(hdl_, websocketpp::close::status::normal, "", ec);
  }

  if (!ec) state_ = kClosing;
  return !ec;
}

void VertxBus::WaitClosed() {
  if (asio_thread_) {
    asio_thread_->join();
    asio_thread_ = NULL;
  }
}

void VertxBus::OnOpen(websocketpp::connection_hdl hdl) {
  SendPing();
  state_ = kOpen;

  StartPingInterval();

  if (on_open_) on_open_();
}

void VertxBus::OnClose(websocketpp::connection_hdl hdl) {
  state_ = kClosed;

  StopPingInterval();

  reply_handlers_.clear();
  handler_map_.clear();

#ifdef VERTXBUSPP_TLS
  if (client_is_tls_) {
    client_tls_.stop_perpetual();
  } else
#endif
  {
    client_.stop_perpetual();
  }

  if (on_close_) {
#ifdef VERTXBUSPP_TLS
    if (client_is_tls_) {
      on_close_(client_tls_.get_con_from_hdl(hdl).get()->get_ec());
    } else
#endif
    {
      on_close_(client_.get_con_from_hdl(hdl).get()->get_ec());
    }
  }
}

void VertxBus::OnFail(websocketpp::connection_hdl hdl) {
  state_ = kClosed;

#ifdef VERTXBUSPP_TLS
  if (client_is_tls_) {
    client_tls_.stop_perpetual();
  } else
#endif

  {
    client_.stop_perpetual();
  }

  if (on_fail_) {
#ifdef VERTXBUSPP_TLS
    if (client_is_tls_) {
      on_fail_(client_tls_.get_con_from_hdl(hdl).get()->get_ec(), Json::Value());
    } else
#endif
    {
      on_fail_(client_.get_con_from_hdl(hdl).get()->get_ec(), Json::Value());
    }
  }
}

void VertxBus::OnMessagePlain(websocketpp::connection_hdl hdl, message_ptr msg) {
  OnMessage(hdl, msg.get()->get_payload());
}

#ifdef VERTXBUSPP_TLS
void VertxBus::OnMessageTls(websocketpp::connection_hdl hdl, message_ptr_tls msg) {
  OnMessage(hdl, msg.get()->get_payload());
}
#endif

void VertxBus::OnMessage(websocketpp::connection_hdl hdl, const std::string& payload) {
  Json::Reader reader;
  Json::Value json;
  
  if (reader.parse(payload, json)) {
    Json::Value replyAddress = json.get("replyAddress", Json::Value());
    Json::Value address = json.get("address", Json::Value());
  
    if (replyAddress.empty()) replyAddress = "";
  
    hm_lock_.lock();
    const std::list<VertxReplyHandlers>& handlers = handler_map_[address.asString()];
    if (!handlers.empty()) {
      std::list<VertxReplyHandlers> copy = handlers;
  
      hm_lock_.unlock();
  
      for (const VertxReplyHandlers& rh : copy) {
        if (json.isMember("type") && json["type"].isString() && json["type"].asString() == "err") {
          if (rh.failure_handler) {

            Json::Value failure;
            failure["failureCode"] = json.get("failureCode", -1);
            failure["failureType"] = json.get("failureType", "UNKNOWN");
            failure["message"] = json.get("message", "");
            rh.failure_handler(failure);
          }
        } else {
            rh.reply_handler(json, VertxBusReplier(this, replyAddress.asString()));
        }
      }
    } else {
      hm_lock_.unlock();
  
      std::string addr = address.asString();
  
      rh_lock_.lock();
      if (reply_handlers_.count(addr) > 0) {
        VertxReplyHandlers rep_handlers = reply_handlers_[addr];
        reply_handlers_.erase(addr);
        rh_lock_.unlock();
  
        if (json.isMember("type") && json["type"].isString() && json["type"].asString() == "err") {
          if (rep_handlers.failure_handler) {
            Json::Value failure;
            failure["failureCode"] = json.get("failureCode", -1);
            failure["failureType"] = json.get("failureType", "UNKNOWN");
            failure["message"] = json.get("message", "");
            rep_handlers.failure_handler(failure);
          }
        } else {
          rep_handlers.reply_handler(json, VertxBusReplier(this, replyAddress.asString()));
        }
      } else {
        rh_lock_.unlock();
  
        if (json.isMember("type") && json["type"].isString() && json["type"].asString() == "err") {
            if (on_fail_) {
                on_fail_(std::make_error_code(std::errc::connection_refused), json);
            }
        } else {
          std::string err_msg = "No handler found for message: ";
          Json::StyledWriter writer;
          err_msg += writer.write(json);

#ifdef VERTXBUSPP_TLS
          if (client_is_tls_) {
            client_tls_.get_alog().write(websocketpp::log::alevel::app, err_msg);
          } else
#endif
          {
            client_.get_alog().write(websocketpp::log::alevel::app, err_msg);
          }   
        }
      }
    }
  }
}

#ifdef VERTXBUSPP_TLS
VertxBus::context_ptr_tls VertxBus::OnTlsInit(websocketpp::connection_hdl hdl) {
  context_ptr_tls ctx = websocketpp::lib::make_shared<asio::ssl::context>(asio::ssl::context::tlsv1);
  
  try {
    ctx->set_options(asio::ssl::context::default_workarounds |
                     asio::ssl::context::no_sslv2 |
                     asio::ssl::context::no_sslv3 |
                     asio::ssl::context::single_dh_use);
  } catch (std::exception& e) {
    std::cerr << e.what() << std::endl;
  }
  
  return ctx;
}
#endif

bool VertxBus::SendOrPub(const std::string& send_or_pub, const std::string& address, const Json::Value& message, const Json::Value& headers, const ReplyHandler& reply_handler, const FailureHandler& failure_handler) {
  if (state_ != kOpen) return false;
  
  Json::Value envelope;
  envelope["type"] = send_or_pub;
  envelope["address"] = address;
  envelope["headers"] = MergeHeaders(headers);
  envelope["body"] = message;
  
  if (reply_handler) {
    std::string reply_address;
    MakeUUID(reply_address);
    envelope["replyAddress"] = reply_address;
    rh_lock_.lock();
    reply_handlers_[reply_address] = { reply_handler, failure_handler };
    rh_lock_.unlock();
  }
  
  Json::FastWriter writer;
  websocketpp::lib::error_code ec;

#ifdef VERTXBUSPP_TLS
  if (client_is_tls_) {
    client_tls_.send(hdl_, writer.write(envelope), websocketpp::frame::opcode::text, ec);
  } else
#endif

  {
    client_.send(hdl_, writer.write(envelope), websocketpp::frame::opcode::text, ec);
  }

  return !ec;
}

bool VertxBus::SendPing() {
  if (state_ != kOpen) return false;

  websocketpp::lib::error_code ec;

#ifdef VERTXBUSPP_TLS
  if (client_is_tls_) {
    client_tls_.send(hdl_, "{\"type\": \"ping\"}", websocketpp::frame::opcode::text, ec);
  } else
#endif
  {
    client_.send(hdl_, "{\"type\": \"ping\"}", websocketpp::frame::opcode::text, ec);
  }

  return !ec;
}

void VertxBus::StartPingInterval() {
  {
    websocketpp::lib::lock_guard<websocketpp::lib::mutex> lock(ping_lock_);
    if (do_ping_) return;
    do_ping_ = true;
  }
  
  ping_thread_ = new websocketpp::lib::thread([=]() {
                                                  websocketpp::lib::unique_lock<websocketpp::lib::mutex> lock(ping_lock_);
                                                  while (do_ping_) {
                                                      auto wait_res = ping_cvwait_.wait_for(lock, std::chrono::milliseconds(options_.ping_interval));
                                                      if (wait_res == std::cv_status::timeout) SendPing();
                                                  }
                                                });
}

void VertxBus::StopPingInterval() {
  {
    websocketpp::lib::lock_guard<websocketpp::lib::mutex> lock(ping_lock_);
    if (!do_ping_) return;
    do_ping_ = false;
    ping_cvwait_.notify_one();
  }
  
  if (ping_thread_) {
    ping_thread_->join();
    delete ping_thread_;
    ping_thread_ = NULL;
  }
}

void VertxBus::MakeUUID(std::string& uuid) {
#ifdef WIN32
  UUID uuid_gen;
  UuidCreate(&uuid_gen);
  
  RPC_CSTR str;
  UuidToStringA(&uuid_gen, &str);
  
  uuid = reinterpret_cast<char*> (str);
  
  RpcStringFreeA(&str);
#else
  uuid_t uuid_gen;
  uuid_generate_random(uuid_gen);
  char str[37];
  uuid_unparse_lower(uuid_gen, str);
  uuid = str;
#endif
}

Json::Value VertxBus::MergeHeaders(Json::Value headers) {
  if (default_headers_.isObject()) {
    if (!headers.isObject()) return default_headers_;
  
    for (const auto& header_name : default_headers_.getMemberNames()) {
      if (!headers.isMember(header_name)) {
        headers[header_name] = default_headers_[header_name];
      }
    }
  }

  return headers.isObject() ? headers : Json::Value();
}
