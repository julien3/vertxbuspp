// Copyright 2015 Julien Lehuraux

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
#define _WEBSOCKETPP_CPP11_INTERNAL_

#ifdef _MSC_VER
#pragma warning( disable : 4503 )
#endif

#ifdef VERTXBUSPP_TLS
#include <websocketpp/config/asio_client.hpp>
#else
#include <websocketpp/config/asio_no_tls_client.hpp>
#endif
#include <websocketpp/client.hpp>
#include <websocketpp/common/thread.hpp>

#include <json/json.h>

#include <atomic>
#include <list>

struct VertxBusOptions
{
    unsigned int ping_interval;
};

static const VertxBusOptions DefaultVertxBusOptions = { 5000 };

enum VertxBusState
{
    CONNECTING,
    OPEN,
    CLOSING,
    CLOSED
};

class VertxBus
{
public:
    class VertxBusReplier;
    typedef std::function<void(const Json::Value&, const VertxBusReplier&)> replyHandler;
    typedef std::function<void(const Json::Value&)> failureHandler;

    class VertxBusReplier
    {
    public:
        VertxBusReplier();
        VertxBusReplier(VertxBus* vb, const std::string& address);

        void operator() (const Json::Value& reply, const replyHandler& reply_handler = replyHandler(), const failureHandler& failure_handler = failureHandler()) const;

    private:
        VertxBus* m_vb;
        std::string m_address;
    };

    VertxBus();
    ~VertxBus();

    bool connect(   const std::string& url, 
                    const std::function<void()>& on_open = std::function<void()>(), 
                    const std::function<void()>& on_close = std::function<void()>(), 
                    const std::function<void(const std::error_code&, const Json::Value&)>& on_fail = std::function<void(const std::error_code&, const Json::Value&)>(),
                    const VertxBusOptions& options = DefaultVertxBusOptions);

    inline bool send(const std::string& address, const Json::Value& message, const replyHandler& reply_handler = replyHandler(), const failureHandler& failure_handler = failureHandler())
    {
        return sendOrPub("send", address, message, reply_handler, failure_handler);
    }

    inline bool publish(const std::string& address, const Json::Value& message)
    {
        return sendOrPub("publish", address, message);
    }

    bool registerHandler(const std::string& address, const replyHandler& reply_handler);

    bool unregisterHandler(const std::string& address, const replyHandler& reply_handler = replyHandler());

    bool close();

    void waitClosed();

    inline VertxBusState readyState() const { return m_state; }

private:
#ifdef VERTXBUSPP_TLS
    typedef websocketpp::client<websocketpp::config::asio_tls_client> client_tls;
    typedef websocketpp::config::asio_tls_client::message_type::ptr message_ptr_tls;
    typedef websocketpp::lib::shared_ptr<asio::ssl::context> context_ptr_tls;

    typedef websocketpp::client<websocketpp::config::asio_client> client;
    typedef websocketpp::config::asio_client::message_type::ptr message_ptr;
#else
    typedef websocketpp::client<websocketpp::config::asio_client> client;
    typedef websocketpp::config::asio_client::message_type::ptr message_ptr;
#endif

    struct VertxReplyHandlers
    {
        replyHandler reply_handler;
        failureHandler failure_handler;
    };

    void on_open(websocketpp::connection_hdl);

    void on_close(websocketpp::connection_hdl);

    void on_fail(websocketpp::connection_hdl);

    void on_message_plain(websocketpp::connection_hdl, message_ptr);

#ifdef VERTXBUSPP_TLS
    void on_message_tls(websocketpp::connection_hdl, message_ptr_tls);
#endif

    void on_message(websocketpp::connection_hdl, const std::string&);

#ifdef VERTXBUSPP_TLS
    context_ptr_tls on_tls_init(websocketpp::connection_hdl);
#endif

    bool sendOrPub(const std::string& send_or_pub, const std::string& address, const Json::Value& message, const replyHandler& reply_handler = replyHandler(), const failureHandler& failure_handler = failureHandler());

    bool sendPing();

    void sendPingInterval();

    void makeUUID(std::string& uuid);

    client m_client;
#ifdef VERTXBUSPP_TLS
    client_tls m_client_tls;
#endif

    websocketpp::lib::thread* m_asio_thread;
    websocketpp::lib::thread* m_ping_thread;
    websocketpp::connection_hdl m_hdl;

    std::atomic<VertxBusState> m_state;
    std::atomic<bool> m_do_ping;
    VertxBusOptions m_options;

    std::map<std::string, VertxReplyHandlers> m_reply_handlers;
    std::map<std::string, std::list<replyHandler>> m_handler_map;

    websocketpp::lib::mutex m_rh_lock;
    websocketpp::lib::mutex m_hm_lock;

    std::function<void()> m_on_open;
    std::function<void()> m_on_close;
    std::function<void(const std::error_code&, const Json::Value&)> m_on_fail;

#ifdef VERTXBUSPP_TLS
    bool m_client_is_tls;
#endif
};

#endif
