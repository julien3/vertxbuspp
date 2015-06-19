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

#include "VertxBus.h"

#ifdef _WIN32
#pragma comment(lib, "rpcrt4.lib")
#include <Rpc.h>
#else
#include <uuid/uuid.h>
#endif

bool endsWith(const std::string& str, const std::string& end)
{
    return str.length() >= end.length() && str.compare(str.length() - end.length(), end.length(), end) == 0;
}

VertxBus::VertxBusReplier::VertxBusReplier() :
m_vb(NULL)
{
}

VertxBus::VertxBusReplier::VertxBusReplier(VertxBus* vb, const std::string& address):
m_vb(vb),
m_address(address)
{
}

void VertxBus::VertxBusReplier::operator() (const Json::Value& reply, const replyHandler& reply_handler, const failureHandler& failure_handler) const
{
    if (!m_address.empty())
        m_vb->send(m_address, reply, reply_handler, failure_handler);
}

VertxBus::VertxBus():
m_asio_thread(NULL),
m_ping_thread(NULL),
m_state(CLOSED),
m_do_ping(false)
{
    // set up access channels to log nothing
    m_client.clear_access_channels(websocketpp::log::alevel::all);
    /*m_client.set_access_channels(websocketpp::log::alevel::connect);
    m_client.set_access_channels(websocketpp::log::alevel::disconnect);
    m_client.set_access_channels(websocketpp::log::alevel::app);*/

    // set up error channel to log nothing
    m_client.clear_error_channels(websocketpp::log::elevel::all);

    // Initialize the Asio transport policy
    m_client.init_asio();

    // Bind the handlers we are using
    m_client.set_open_handler(websocketpp::lib::bind(&VertxBus::on_open, this, websocketpp::lib::placeholders::_1));
    m_client.set_close_handler(websocketpp::lib::bind(&VertxBus::on_close, this, websocketpp::lib::placeholders::_1));
    m_client.set_fail_handler(websocketpp::lib::bind(&VertxBus::on_fail, this, websocketpp::lib::placeholders::_1));
    m_client.set_message_handler(websocketpp::lib::bind(&VertxBus::on_message, this, websocketpp::lib::placeholders::_1, websocketpp::lib::placeholders::_2));
}

VertxBus::~VertxBus()
{
    if (m_state == OPEN)
        close();

    waitClosed();
}

bool VertxBus::connect(const std::string& url, const std::function<void()>& on_open, const std::function<void()>& on_close, const std::function<void(const std::error_code&, const Json::Value&)>& on_fail, const VertxBusOptions& options)
{
    if (m_state != CLOSED)
        return false;

    m_on_open = on_open;
    m_on_close = on_close;
    m_on_fail = on_fail;
    m_options = options;
    m_state = CONNECTING;

    std::string final_url = url;
    if (!endsWith(url, "/websocket"))
        final_url += "/websocket";

    websocketpp::lib::error_code ec;
    client::connection_ptr con = m_client.get_connection(final_url, ec);
    if (ec)
    {
        m_client.get_alog().write(websocketpp::log::alevel::app,
            "Get Connection Error: " + ec.message());
        return false;
    }

    // Grab a handle for this connection so we can talk to it in a thread
    // safe manor after the event loop starts.
    m_hdl = con->get_handle();

    // Queue the connection. No DNS queries or network connections will be
    // made until the io_service event loop is run.
    m_client.connect(con);

    // Create a thread to run the ASIO io_service event loop
    if (m_asio_thread)
    {
        m_asio_thread->join();
        delete m_asio_thread;
    }
    m_asio_thread = new websocketpp::lib::thread(&client::run, &m_client);

    return true;
}

bool VertxBus::registerHandler(const std::string& address, const replyHandler& reply_handler)
{
    if (m_state != OPEN || !reply_handler) return false;

    m_hm_lock.lock();
    std::list<replyHandler>& handlers = m_handler_map[address];
    bool do_reg = handlers.empty();
    handlers.push_back(reply_handler);
    if (do_reg)
    {
        m_hm_lock.unlock();
        websocketpp::lib::error_code ec;
        std::string msg = "{\"type\": \"register\", \"address\": \"";
        msg += address;
        msg += "\"}";
        m_client.send(m_hdl, msg, websocketpp::frame::opcode::text, ec);

        return !ec;
    }
    
    m_hm_lock.unlock();

    return true;
}

bool VertxBus::unregisterHandler(const std::string& address, const replyHandler& reply_handler)
{
    if (m_state != OPEN) return false;

    m_hm_lock.lock();
    std::list<replyHandler>& handlers = m_handler_map[address];
    if (!handlers.empty())
    {
        if (!reply_handler)
            handlers.clear();
        else
        {
            void(*const* rh_ptr)(const Json::Value&, const VertxBusReplier&) = reply_handler.target<void(*)(const Json::Value&, const VertxBusReplier&)>();

            if (rh_ptr)
            {
                bool bfound = false;
                for (std::list<replyHandler>::iterator it = handlers.begin(); !bfound && it != handlers.end(); it++)
                {
                    void(*const* it_ptr)(const Json::Value&, const VertxBusReplier&) = (*it).target<void(*)(const Json::Value&, const VertxBusReplier&)>();
                    if (it_ptr && *it_ptr == *rh_ptr)
                    {
                        handlers.erase(it);
                        bfound = true;
                    }
                }
            }
        }

        if (handlers.empty())
        {
            m_hm_lock.unlock();

            websocketpp::lib::error_code ec;
            std::string msg = "{\"type\": \"unregister\", \"address\": \"";
            msg += address;
            msg += "\"}";
            m_client.send(m_hdl, msg, websocketpp::frame::opcode::text, ec);

            return !ec;
        }
    }

    m_hm_lock.unlock();

    return true;
}

bool VertxBus::close()
{
    if (m_state != OPEN) return false;

    websocketpp::lib::error_code ec;
    m_client.close(m_hdl, websocketpp::close::status::normal, "", ec);

    if (!ec)
        m_state = CLOSING;

    return !ec;
}

void VertxBus::waitClosed()
{
    if (m_asio_thread)
    {
        m_asio_thread->join();
        delete m_asio_thread;
        m_asio_thread = NULL;
    }
}

void VertxBus::on_open(websocketpp::connection_hdl)
{
    sendPing();
    m_state = OPEN;

    m_do_ping = true;
    m_ping_thread = new websocketpp::lib::thread(&VertxBus::sendPingInterval, this);

    if (m_on_open)
        m_on_open();
}

void VertxBus::on_close(websocketpp::connection_hdl)
{
    m_state = CLOSED;

    m_do_ping = false;

    if (m_ping_thread)
    {
        m_ping_thread->join();
        delete m_ping_thread;
        m_ping_thread = NULL;
    }

    if (m_on_close)
        m_on_close();
}

void VertxBus::on_fail(websocketpp::connection_hdl hdl)
{
    if (m_on_fail)
        m_on_fail(m_client.get_con_from_hdl(hdl).get()->get_ec(), Json::Value());
}

void VertxBus::on_message(websocketpp::connection_hdl hdl, message_ptr msg)
{
    Json::Reader reader;
    Json::Value json;

    if (reader.parse(msg.get()->get_payload(), json))
    {
        if (json.isMember("type") && json["type"].isString() && json["type"].asString() == "err")
        {
            if (m_on_fail)
            {
                m_on_fail(std::make_error_code(std::errc::connection_refused), json.get("body", Json::Value()));
            }
            else
            {
                std::string err_msg = "Error received on connection: ";
                err_msg += json.get("body", "").asString();
                m_client.get_alog().write(websocketpp::log::alevel::app, err_msg);
            }

            return;
        }

        Json::Value body = json.get("body", Json::Value());
        Json::Value replyAddress = json.get("replyAddress", Json::Value());
        Json::Value address = json.get("address", Json::Value());

        if (replyAddress.empty())
        {
            replyAddress = "";
        }

        m_hm_lock.lock();
        const std::list<replyHandler>& handlers = m_handler_map[address.asString()];
        if (!handlers.empty())
        {
            std::list<replyHandler> copy = handlers;

            m_hm_lock.unlock();

            for (const replyHandler& rh : copy)
                rh(body, VertxBusReplier(this, replyAddress.asString()));
        }
        else
        {
            m_hm_lock.unlock();

            std::string addr = address.asString();

            m_rh_lock.lock();
            if (m_reply_handlers.count(addr) > 0)
            {
                VertxReplyHandlers rep_handlers = m_reply_handlers[addr];
                m_reply_handlers.erase(addr);
                m_rh_lock.unlock();

                if (json.isMember("body"))
                    rep_handlers.reply_handler(body, VertxBusReplier(this, replyAddress.asString()));
                else if (json.isMember("failureCode"))
                {
                    Json::Value failure;
                    failure["failureCode"] = json.get("failureCode", -1);
                    failure["failureType"] = json.get("failureType", "UNKNOWN");
                    failure["message"] = json.get("message", "");
                    if (rep_handlers.failure_handler)
                        rep_handlers.failure_handler(failure);
                }
            }
            else
            {
                m_rh_lock.unlock();
            }
        }
    }
}

bool VertxBus::sendOrPub(const std::string& send_or_pub, const std::string& address, const Json::Value& message, const replyHandler& reply_handler, const failureHandler& failure_handler)
{
    if (m_state != OPEN) return false;

    Json::Value envelope;
    envelope["type"] = send_or_pub;
    envelope["address"] = address;
    envelope["body"] = message;

    if (reply_handler)
    {
        std::string reply_address;
        makeUUID(reply_address);
        envelope["replyAddress"] = reply_address;
        m_rh_lock.lock();
        m_reply_handlers[reply_address] = { reply_handler, failure_handler };
        m_rh_lock.unlock();
    }

    Json::FastWriter writer;
    websocketpp::lib::error_code ec;
    m_client.send(m_hdl, writer.write(envelope), websocketpp::frame::opcode::text, ec);

    return !ec;
}

bool VertxBus::sendPing()
{
    if (m_state != OPEN) return false;

    websocketpp::lib::error_code ec;
    m_client.send(m_hdl, "{\"type\": \"ping\"}", websocketpp::frame::opcode::text, ec);

    return !ec;
}

void VertxBus::sendPingInterval()
{
    while (m_do_ping)
    {
#ifdef _WEBSOCKETPP_CPP11_THREAD_
        std::this_thread::sleep_for(std::chrono::milliseconds(m_options.ping_interval));
#else
        boost::this_thread::sleep_for(std::chrono::milliseconds(m_options.ping_interval));
#endif
        sendPing();
    }
}

void VertxBus::makeUUID(std::string& uuid)
{
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
