// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.
#pragma once

#include <chrono>
#include <memory>
#include <string>
#include <azure_c_shared_utility_includes.h>
#include <azure_c_shared_utility_macro_utils.h>

namespace WebSocketAdapter
{
    struct WebSocketConfiguration
    {
        std::string host;
        int port;
        std::string protocol_name;
        int protocol_count;
        std::string relative_path;
        std::string trusted_ca;
        int use_ssl;
    };

    struct ProxyConfiguration
    {
        std::string host;
        int port;
        std::string username;
        std::string password;
    };

    enum class WebSocketMessageType
    {
        Binary,
        Text
    };

    class IWebSocketAdapter
    {
    public:
        virtual ~IWebSocketAdapter() {};
        /*
        * Initializes the websocket.
        *
        * @throw std::runtime_error if initialization fails or websocket has already been initialized.
        */
        virtual void Initialize(const WebSocketConfiguration& configuration, void* callback_context) = 0;
        /*
        * Initializes the websocket with proxy.
        *
        * @throw std::runtime_error if initialization fails or websocket has already been initialized.
        */
        virtual void Initialize(const WebSocketConfiguration& configuration, const ProxyConfiguration& proxy_configuration, void* callback_context) = 0;
        /*
        * Uninitializes the websocket. AFter calling this function the websocket has to be initialized before it can be used.
        */
        virtual void Uninitialize() = 0;
        virtual void Open(
            ON_WS_OPEN_COMPLETE on_ws_open_complete,
            ON_WS_FRAME_RECEIVED on_ws_frame_received,
            ON_WS_PEER_CLOSED on_ws_peer_closed,
            ON_WS_ERROR on_ws_error) = 0;
        virtual void Close(const std::chrono::milliseconds& polling_interval_ms, ON_WS_CLOSE_COMPLETE on_io_close_complete) = 0;
        virtual void Send(const unsigned char* buffer, size_t size, WebSocketMessageType message_type, ON_WS_SEND_FRAME_COMPLETE on_send_complete, void* callback_context) = 0;
        virtual void DoWork() = 0;
        virtual void SetOption(const char* option_name, const void* value) = 0;
        virtual void SetRequestHeader(const char* name, const char* value) = 0;
        virtual int GetHttpStatus() = 0;
    };
}

std::shared_ptr<WebSocketAdapter::IWebSocketAdapter> GetWebSocketAdapter();
