// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.
#pragma once

#include <atomic>
#include <chrono>
#include "i_web_socket_adapter.h"

namespace WebSocketAdapter
{
    enum class UwsWebSocketState
    {
        Uninitialized,
        Initialized,
        Open,
        Closing
    };

    class UwsWebSocket : public IWebSocketAdapter
    {
    public:
        UwsWebSocket();
        virtual ~UwsWebSocket();
        virtual void Initialize(const WebSocketConfiguration& configuration, void* callback_context);
        virtual void Initialize(const WebSocketConfiguration& configuration, const ProxyConfiguration& proxy_configuration, void* callback_context);
        virtual void Uninitialize();
        virtual int Open(
            ON_WS_OPEN_COMPLETE on_ws_open_complete,
            ON_WS_FRAME_RECEIVED on_ws_frame_received,
            ON_WS_PEER_CLOSED on_ws_peer_closed,
            ON_WS_ERROR on_ws_error);
        virtual void Close(const std::chrono::milliseconds& polling_interval_ms, ON_WS_CLOSE_COMPLETE on_io_close_complete);
        virtual int Send(const unsigned char* buffer, size_t size, WebSocketMessageType message_type, ON_WS_SEND_FRAME_COMPLETE on_send_complete, void* callback_context);
        virtual void DoWork();
        virtual int SetOption(const char* optionName, const void* value);
        virtual int SetRequestHeader(const char* name, const char* value);
        virtual int GetHttpStatus();

    private:
        static void OnWebSocketOpened(void* context, WS_OPEN_RESULT_DETAILED open_result_detailed);
        static void OnWebSocketPeerClosed(void* context, uint16_t* close_code, const unsigned char* extra_data, size_t extra_data_length);
        static void OnWebSocketError(void* context, WS_ERROR error_code);
        static void OnWebSocketClosed(void* context);
        void PumpWebSocketInBackground(UWS_CLIENT_HANDLE handle);

    private:
        UWS_CLIENT_HANDLE m_webSocketHandle;
        std::atomic<ON_WS_OPEN_COMPLETE> m_onOpenComplete;
        std::atomic<ON_WS_PEER_CLOSED> m_onPeerClosed;
        std::atomic<ON_WS_CLOSE_COMPLETE> m_onCloseComplete;
        std::atomic<ON_WS_ERROR> m_onError;
        std::atomic<void*> m_callbackContext;
        std::atomic<UwsWebSocketState> m_state;
        bool m_useProxy;
        std::recursive_mutex m_doWorkMtx;
    };
}
