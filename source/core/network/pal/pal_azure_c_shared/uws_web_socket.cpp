// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.
#include "stdafx.h"
#include "uws_web_socket.h"
#include <future>
#include <stdexcept>
#include <thread>
#include "azure_c_shared_utility_http_proxy_io_wrapper.h"
#include "azure_c_shared_utility_platform_wrapper.h"
#include "azure_c_shared_utility_tlsio_wrapper.h"
#include "azure_c_shared_utility_uws_frame_encoder_wrapper.h"
#include "azure_c_shared_utility_xlogging_wrapper.h"

using namespace WebSocketAdapter;

std::shared_ptr<IWebSocketAdapter> GetWebSocketAdapter()
{
    return std::make_shared<UwsWebSocket>();
}

UwsWebSocket::UwsWebSocket() :
    m_webSocketHandle(nullptr),
    m_onOpenComplete(nullptr),
    m_onPeerClosed(nullptr),
    m_onCloseComplete(nullptr),
    m_onError(nullptr),
    m_callbackContext(nullptr),
    m_state(UwsWebSocketState::Uninitialized),
    m_useProxy(false)
{
    SPX_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
}

UwsWebSocket::~UwsWebSocket()
{
    SPX_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    uws_client_destroy(m_webSocketHandle);
    m_webSocketHandle = nullptr;
}

void UwsWebSocket::Initialize(const WebSocketConfiguration& configuration, void* callback_context)
{
    SPX_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    if (m_state != UwsWebSocketState::Uninitialized)
    {
        SPX_TRACE_ERROR("Invalid state: %d", static_cast<int>(m_state.load()));
        throw std::runtime_error("Invalid state");
    }
    m_callbackContext = callback_context;
    WS_PROTOCOL wsProto;
    wsProto.protocol = configuration.protocol_name.c_str();
    m_webSocketHandle = uws_client_create(
        configuration.host.c_str(),
        configuration.port,
        configuration.relative_path.c_str(),
        configuration.use_ssl,
        configuration.protocol_count > 0 ? &wsProto : nullptr,
        configuration.protocol_count);
    if (!m_webSocketHandle)
    {
        SPX_TRACE_ERROR("Failed to create web socket");
        throw std::runtime_error("Failed to create the web socket");
    }
    m_state = UwsWebSocketState::Initialized;
}

void UwsWebSocket::Initialize(const WebSocketConfiguration& configuration, const ProxyConfiguration& proxy_configuration, void* callback_context)
{
    SPX_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    if (m_state != UwsWebSocketState::Uninitialized)
    {
        SPX_TRACE_ERROR("Invalid state: %d", static_cast<int>(m_state.load()));
        throw std::runtime_error("Invalid state");
    }
    m_callbackContext = callback_context;
    m_useProxy = true;
    HTTP_PROXY_IO_CONFIG proxyConfig;
    proxyConfig.hostname = configuration.host.c_str();
    proxyConfig.port = configuration.port;
    proxyConfig.proxy_hostname = proxy_configuration.host.c_str();
    proxyConfig.proxy_port = proxy_configuration.port;
    proxyConfig.username = proxy_configuration.username.c_str();
    proxyConfig.password = proxy_configuration.password.c_str();
    void* underlyingIOParameters = &proxyConfig;
    const IO_INTERFACE_DESCRIPTION* underlyingIOInterface = http_proxy_io_get_interface_description();
    if (underlyingIOInterface == nullptr)
    {
        SPX_TRACE_ERROR("NULL proxy interface description");
        throw std::runtime_error("NULL proxy interface description");
    }
    else
    {
        TLSIO_CONFIG tlsioConfig;
        if (configuration.use_ssl)
        {
            tlsioConfig.hostname = proxyConfig.hostname;
            tlsioConfig.port = proxyConfig.port;
            tlsioConfig.underlying_io_interface = underlyingIOInterface;
            tlsioConfig.underlying_io_parameters = underlyingIOParameters;
            underlyingIOParameters = &tlsioConfig;
            underlyingIOInterface = platform_get_default_tlsio();
            if (underlyingIOInterface == nullptr)
            {
                SPX_TRACE_ERROR("NULL TLSIO interface description");
                throw std::runtime_error("NULL TLSIO interface description");
            }
        }
        WS_PROTOCOL wsProto;
        wsProto.protocol = configuration.protocol_name.c_str();
        m_webSocketHandle = uws_client_create_with_io(
            underlyingIOInterface,
            underlyingIOParameters,
            configuration.host.c_str(),
            configuration.port,
            configuration.relative_path.c_str(),
            configuration.protocol_count > 0 ? &wsProto : nullptr,
            configuration.protocol_count);
        if (!m_webSocketHandle)
        {
            SPX_TRACE_ERROR("Failed to create web socket");
            throw std::runtime_error("Failed to create the web socket");
        }
    }
    m_state = UwsWebSocketState::Initialized;
}

void UwsWebSocket::Uninitialize()
{
    SPX_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    if (m_state != UwsWebSocketState::Initialized)
    {
        SPX_TRACE_ERROR("Invalid state: %d", static_cast<int>(m_state.load()));
        throw std:: runtime_error("Invalid state");
    }
    if (m_webSocketHandle)
    {
        uws_client_destroy(m_webSocketHandle);
        m_webSocketHandle = nullptr;
    }
    m_state = UwsWebSocketState::Uninitialized;
}

int UwsWebSocket::Open(
    ON_WS_OPEN_COMPLETE on_ws_open_complete,
    ON_WS_FRAME_RECEIVED on_ws_frame_received,
    ON_WS_PEER_CLOSED on_ws_peer_closed,
    ON_WS_ERROR on_ws_error)
{
    SPX_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    if (m_state != UwsWebSocketState::Initialized)
    {
        SPX_TRACE_ERROR("Invalid state: %d", static_cast<int>(m_state.load()));
        throw std::runtime_error("Invalid state");
    }
    m_onOpenComplete = on_ws_open_complete;
    m_onPeerClosed = on_ws_peer_closed;
    m_onError = on_ws_error;
    int result = uws_client_open_async(m_webSocketHandle, OnWebSocketOpened, this, on_ws_frame_received, m_callbackContext, OnWebSocketPeerClosed, this, OnWebSocketError, this);
    return result;
    // State will get updated in the callback (which is issued before this function returns).
}

void UwsWebSocket::Close(const std::chrono::milliseconds& pollingIntervalMs, ON_WS_CLOSE_COMPLETE on_ws_close_complete)
{
    SPX_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    if (m_state != UwsWebSocketState::Open)
    {
        SPX_TRACE_ERROR("Invalid state: %d", static_cast<int>(m_state.load()));
        throw std::runtime_error("Invalid state");
    }
    m_state = UwsWebSocketState::Closing;
    using namespace std;
    static const chrono::milliseconds SLEEP_INTERVAL = 10ms;
    static const int MAX_WAIT_RETRIES = 100;
    m_onCloseComplete = on_ws_close_complete;
    int result = uws_client_close_handshake_async(m_webSocketHandle, 1000 /* Disconnect reason normal */, "" /* not used */, OnWebSocketClosed, this);
    if (result == 0)
    {
        // first wait for the poll period to give the background worker a chance to terminate
        std::this_thread::sleep_for(pollingIntervalMs);
        // waits for close.
        int retries = 0;
        while ((m_state == UwsWebSocketState::Closing) && (retries++ < MAX_WAIT_RETRIES))
        {
            PumpWebSocketInBackground(m_webSocketHandle);
            std::this_thread::sleep_for(SLEEP_INTERVAL);
        }
    }
    if (m_state == UwsWebSocketState::Closing)
    {
        // force to close.
        (void)uws_client_close_async(m_webSocketHandle, OnWebSocketClosed, this);

        // wait for force close.
        while (m_state == UwsWebSocketState::Closing)
        {
            PumpWebSocketInBackground(m_webSocketHandle);
            std::this_thread::sleep_for(SLEEP_INTERVAL);
        }
    }
}

int UwsWebSocket::Send(const unsigned char* buffer, size_t size, WebSocketMessageType message_type, ON_WS_SEND_FRAME_COMPLETE on_send_complete, void* callback_context)
{
    return uws_client_send_frame_async(m_webSocketHandle, message_type == WebSocketMessageType::Text ? 1 : 2, buffer, size, true, on_send_complete, callback_context);
}

void UwsWebSocket::DoWork()
{
    std::lock_guard<std::recursive_mutex> lock(m_doWorkMtx);

    uws_client_dowork(m_webSocketHandle);
}

int UwsWebSocket::SetOption(const char* optionName, const void* value)
{
    SPX_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    return uws_client_set_option(m_webSocketHandle, optionName, value);
}

int UwsWebSocket::SetRequestHeader(const char* option, const char* value)
{
    SPX_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    return uws_client_set_request_header(m_webSocketHandle, option, value);
}

int UwsWebSocket::GetHttpStatus()
{
    SPX_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    throw std::runtime_error("Not supported");
}

void UwsWebSocket::PumpWebSocketInBackground(UWS_CLIENT_HANDLE handle)
{
    UNUSED(handle);
    // Helper method to pump the Azure C shared library web socket code on a background thread. This
    // prevents situations where disconnecting can trigger event callbacks on the same thread as
    // disconnect was called on. This is problematic if you are using the thread service ExecuteSync
    // in other code.
    std::lock_guard<std::recursive_mutex> lock(m_doWorkMtx);

    auto ignored = std::async(std::launch::async, [this]()
    {
            uws_client_dowork(m_webSocketHandle);
    });

    // NOTE: the destructor of the async future will block here until the call to uws_client_dowork completes
}

void UwsWebSocket::OnWebSocketOpened(void* context, WS_OPEN_RESULT_DETAILED open_result_detailed)
{
    SPX_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    auto webSocket = static_cast<UwsWebSocket*>(context);
    webSocket->m_state = UwsWebSocketState::Open;
    ON_WS_OPEN_COMPLETE onOpenComplete = webSocket->m_onOpenComplete;
    if (onOpenComplete != nullptr)
    {
        onOpenComplete(webSocket->m_callbackContext, open_result_detailed);
    }
}

void UwsWebSocket::OnWebSocketPeerClosed(void* context, uint16_t* close_code, const unsigned char* extra_data, size_t extra_data_length)
{
    SPX_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    auto webSocket = static_cast<UwsWebSocket*>(context);
    webSocket->m_state = UwsWebSocketState::Initialized;
    ON_WS_PEER_CLOSED onPeerClosed = webSocket->m_onPeerClosed;
    if (onPeerClosed)
    {
        onPeerClosed(webSocket->m_callbackContext, close_code, extra_data, extra_data_length);
    }
}

void UwsWebSocket::OnWebSocketError(void* context, WS_ERROR error_code)
{
    SPX_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    auto webSocket = static_cast<UwsWebSocket*>(context);
    webSocket->m_state = UwsWebSocketState::Initialized;
    ON_WS_ERROR onError = webSocket->m_onError;
    if (onError)
    {
        onError(webSocket->m_callbackContext, error_code);
    }
}

void UwsWebSocket::OnWebSocketClosed(void* context)
{
    SPX_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    auto webSocket = static_cast<UwsWebSocket*>(context);
    webSocket->m_state = UwsWebSocketState::Initialized;
    ON_WS_CLOSE_COMPLETE onCloseComplete = webSocket->m_onCloseComplete;
    if (onCloseComplete)
    {
        onCloseComplete(webSocket->m_callbackContext);
    }
}
