// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.
#pragma once

#include <atomic>
#include <chrono>
#include <string>
#include <wrl.h>
#include <queue>
#include <list>
#include <mutex>
#include <ppltasks.h>
#include "i_web_socket_adapter.h"
#include "azure_c_shared_utility_httpapi_wrapper.h"

namespace WebSocketAdapter
{
    enum class WinRtWebSocketState
    {
        Uninitialized,
        Initialized,
        Open,
        Closing
    };

    class WinRtWebSocket : public IWebSocketAdapter, public std::enable_shared_from_this<WinRtWebSocket>
    {
    public:
        WinRtWebSocket();
        virtual ~WinRtWebSocket();
        virtual void Initialize(const WebSocketConfiguration& configuration, void* callbackContext);
        virtual void Initialize(const WebSocketConfiguration& configuration, const ProxyConfiguration& proxyConfiguration, void* callbackContext);
        virtual void Uninitialize();
        virtual int Open(
            ON_WS_OPEN_COMPLETE on_ws_open_complete,
            ON_WS_FRAME_RECEIVED on_ws_frame_received,
            ON_WS_PEER_CLOSED on_ws_peer_closed,
            ON_WS_ERROR on_ws_error);
        virtual void Close(const std::chrono::milliseconds& pollingIntervalMs, ON_WS_CLOSE_COMPLETE on_io_close_complete);
        virtual int Send(const unsigned char* buffer, size_t size, WebSocketMessageType messageType, ON_WS_SEND_FRAME_COMPLETE on_send_complete, void* callback_context);
        virtual void DoWork();
        virtual int SetOption(const char* option_name, const void* value);
        virtual int SetRequestHeader(const char* name, const char* value);
        virtual int GetHttpStatus();

    private:
        void OnMessageReceived(Windows::Networking::Sockets::MessageWebSocket^ sender, Windows::Networking::Sockets::MessageWebSocketMessageReceivedEventArgs^ args);
        void OnWebSocketOpened(WS_OPEN_RESULT_DETAILED open_result_detailed);
        void OnWebSocketPeerClosed(uint16_t* closeCode, const unsigned char* extraData, size_t extraDataLength);
        void OnWebSocketError(WS_ERROR errorCode);
        void OnWebSocketClosed();
        void CloseInternal();
        Windows::Storage::Streams::DataWriterStoreOperation^ SendPacket(Windows::Networking::Sockets::MessageWebSocket^ webSocket, Platform::Array<BYTE>^ buffer, WebSocketMessageType msgType);

    private:
        std::atomic<ON_WS_OPEN_COMPLETE> m_onOpenComplete;
        std::atomic<ON_WS_FRAME_RECEIVED> m_onFrameReceived;
        std::atomic<ON_WS_PEER_CLOSED> m_onPeerClosed;
        std::atomic<ON_WS_CLOSE_COMPLETE> m_onCloseComplete;
        std::atomic<ON_WS_ERROR> m_onError;
        std::atomic<void*> m_callbackContext;
        Windows::Networking::Sockets::MessageWebSocket^ m_webSocket;
        Windows::Foundation::IAsyncAction^ m_connectAction;
        Windows::Storage::Streams::DataWriterStoreOperation^ m_dataWriterStoreOperation;
        std::mutex m_lock;
        concurrency::task<void> m_sendTask;
        std::queue<Windows::Networking::Sockets::MessageWebSocketMessageReceivedEventArgs^> m_readQueue;
        std::wstring m_uri;
        std::atomic<WinRtWebSocketState> m_state;
        std::atomic_bool m_error;
        int m_statusCode;
        bool m_useProxy;
        ProxyConfiguration m_proxyConfiguration;
        HTTP_HEADERS_HANDLE m_hHeaders;
        HANDLE m_closeCompletedEvent;
        HANDLE m_dataWriterCompletedEvent;
        static constexpr DWORD c_maxCloseWaitTimeMs = 3000;
    };
}
