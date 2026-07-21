// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.
#include "stdafx.h"
#include <stdlib.h>
//#include "uhttp.h"
#include <memory>
#include <string>
#include <wrl.h>
#include <MemoryBuffer.h>
#include <queue>
#include <locale>
#include <codecvt>
#include <list>
#include <mutex>
#include <atomic>
#include <ppltasks.h>
#include <stdexcept>
#include "winrt_web_socket.h"

using namespace std;
using namespace Microsoft::WRL;
using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Storage::Streams;
using namespace Windows::Networking::Sockets;
using namespace Windows::Web;
using namespace concurrency;
using namespace Windows::Security::Credentials;
using namespace WebSocketAdapter;

std::shared_ptr<IWebSocketAdapter> GetWebSocketAdapter()
{
    return std::make_shared<WinRtWebSocket>();
}

inline static wstring to_wstring(const char* str)
{
    return wstring{ str, str + strlen(str) };
}

WinRtWebSocket::WinRtWebSocket() :
    m_webSocket(nullptr),
    m_dataWriterStoreOperation(nullptr),
    m_state(WinRtWebSocketState::Uninitialized),
    m_statusCode(0),
    m_useProxy(false),
    m_hHeaders(nullptr),
    m_closeCompletedEvent(NULL),
    m_dataWriterCompletedEvent(NULL)
{
    SPX_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    m_hHeaders = HTTPHeaders_Alloc();
    if (m_hHeaders == nullptr)
    {
        SPX_TRACE_ERROR("Header allocation failed");
        throw runtime_error("Http header allocation failed");
    }
    m_closeCompletedEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (m_closeCompletedEvent == NULL)
    {
        SPX_TRACE_ERROR("Event creation failed");
        throw runtime_error("Event creation failed");
    }
    m_dataWriterCompletedEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (m_dataWriterCompletedEvent == NULL)
    {
        SPX_TRACE_ERROR("Event creation failed");
        throw runtime_error("Event creation failed");
    }
}

WinRtWebSocket::~WinRtWebSocket()
{
    SPX_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    HTTPHeaders_Free(m_hHeaders);
    if (m_closeCompletedEvent != NULL)
    {
        CloseHandle(m_closeCompletedEvent);
        m_closeCompletedEvent = NULL;
    }
    if (m_dataWriterCompletedEvent != NULL)
    {
        CloseHandle(m_dataWriterCompletedEvent);
        m_dataWriterCompletedEvent = NULL;
    }
}

void WinRtWebSocket::Initialize(const WebSocketConfiguration& configuration, void* callback_context)
{
    SPX_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    if (configuration.host.empty() || configuration.relative_path.empty())
    {
        SPX_TRACE_ERROR("Host or relative path is empty");
        throw runtime_error("Invalid configuration");
    }
    if (m_state != WinRtWebSocketState::Uninitialized)
    {
        SPX_TRACE_ERROR("Invalid state: %d", m_state.load());
        throw runtime_error("Invalid state");
    }
    HRESULT hr = RoInitialize(RO_INIT_MULTITHREADED);
    if (hr != S_OK && hr != S_FALSE)
    {
        SPX_TRACE_ERROR("RoInitialize failed 0x%x", hr);
        throw runtime_error("RoInitialize failed");
    }
    m_callbackContext = callback_context;
    string hostNameStringLowerCase;
    hostNameStringLowerCase.resize(configuration.host.length());
#pragma warning(push)
#pragma warning(disable : 4242)
#pragma warning(disable : 4244)
    transform(configuration.host.begin(), configuration.host.end(), hostNameStringLowerCase.begin(), ::tolower);
#pragma warning(pop)
    if ((strncmp(hostNameStringLowerCase.c_str(), "ws://", 5) != 0) && (strncmp(hostNameStringLowerCase.c_str(), "wss://", 6) != 0))
    {
        if (configuration.use_ssl)
        {
            m_uri = L"wss://";
        }
        else
        {
            m_uri = L"ws://";
        }
    }
    m_uri += to_wstring(configuration.host.c_str());
    m_uri += L":";
    m_uri += to_wstring(configuration.port);
    m_uri += to_wstring(configuration.relative_path.c_str());
    m_sendTask = task_from_result();
    m_state = WinRtWebSocketState::Initialized;
}

void WinRtWebSocket::Initialize(const WebSocketConfiguration& configuration, const ProxyConfiguration& proxy_configuration, void* callback_context)
{
    SPX_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    if (m_state != WinRtWebSocketState::Uninitialized)
    {
        SPX_TRACE_ERROR("Invalid state: %d", m_state.load());
        throw runtime_error("Invalid state");
    }
    m_useProxy = true;
    m_proxyConfiguration = proxy_configuration;
    Initialize(configuration, callback_context);
}

void WinRtWebSocket::Uninitialize()
{
    SPX_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    if (m_state.load() == WinRtWebSocketState::Open)
    {
        Close(0ms, nullptr);
    }
    if (m_state == WinRtWebSocketState::Initialized)
    {
        RoUninitialize();
    }
    m_useProxy = false;
    m_callbackContext = nullptr;
    m_state = WinRtWebSocketState::Uninitialized;
}

int WinRtWebSocket::Open(
    ON_WS_OPEN_COMPLETE on_ws_open_complete,
    ON_WS_FRAME_RECEIVED on_ws_frame_received,
    ON_WS_PEER_CLOSED on_ws_peer_closed,
    ON_WS_ERROR on_ws_error)
{
    SPX_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    if (m_state != WinRtWebSocketState::Initialized)
    {
        SPX_TRACE_ERROR("Invalid state: %d", m_state.load());
        return WS_OPEN_ERROR_UNDERLYING_IO_OPEN_FAILED;
    }
    WS_OPEN_RESULT_DETAILED result = { WS_OPEN_OK, 0, nullptr, 0 };
    try
    {
        m_sendTask = task_from_result();
        m_onFrameReceived.store(on_ws_frame_received);
        m_onOpenComplete.store(on_ws_open_complete);
        m_onError.store(on_ws_error);
        m_onPeerClosed.store(on_ws_peer_closed);
        m_webSocket = ref new MessageWebSocket();
        if (m_useProxy)
        {
            if (!m_proxyConfiguration.username.empty())
            {
                String^ userName = ref new String(to_wstring(m_proxyConfiguration.username.c_str()).c_str());
                m_webSocket->Control->ProxyCredential->UserName = userName;
            }
            if (!m_proxyConfiguration.password.empty())
            {
                String^ password = ref new String(to_wstring(m_proxyConfiguration.password.c_str()).c_str());
                m_webSocket->Control->ProxyCredential->Password = password;
            }
        }
        m_error = false;
        // When capturing state for the websocket callback lambdas, we pass a weak reference to wsio_instance
        // to avoid strong reference cycles.
        weak_ptr<WinRtWebSocket> weakRef = shared_from_this();
        m_webSocket->Closed += ref new TypedEventHandler<Windows::Networking::Sockets::IWebSocket^, WebSocketClosedEventArgs^>(
            [weakRef](Windows::Networking::Sockets::IWebSocket^ sender, WebSocketClosedEventArgs^ args)
            {
                std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
                std::string str = converter.to_bytes(args->Reason->Begin());
                const unsigned char* reason = reinterpret_cast<const unsigned char*>(str.c_str());
                uint16_t code = static_cast<uint16_t>(args->Code);
                // Memory safety: Only update the instance if it still exists (i.e. the weak_ptr was successfully upgraded to a shared_ptr)
                if (auto webSocket = weakRef.lock())
                {
                    if (webSocket->m_state != WinRtWebSocketState::Closing)
                    {
                        webSocket->OnWebSocketPeerClosed(&code, reason, str.length());
                    }
                    else
                    {
                        webSocket->OnWebSocketClosed();
                    }
                }
            });
    
        m_webSocket->MessageReceived += ref new TypedEventHandler<MessageWebSocket^, MessageWebSocketMessageReceivedEventArgs^>(
            [weakRef](MessageWebSocket^ sender, MessageWebSocketMessageReceivedEventArgs^ args)
            {
                // Memory safety: Only update the instance if it still exists (i.e. the weak_ptr was successfully upgraded to a shared_ptr)
                if (auto webSocket = weakRef.lock())
                {
                    webSocket->OnMessageReceived(sender, args);
                }
            });
        size_t count = 0;
        if (m_hHeaders)
        {
            HTTPHeaders_GetHeaderCount(m_hHeaders, &count);
        }
        for (size_t i = 0; i < count; ++i)
        {
            char* temp = nullptr;
            if (HTTPHeaders_GetHeader(m_hHeaders, i, &temp) == HTTP_HEADERS_OK && temp)
            {
                wstring header = to_wstring(temp);
                free(temp);
                //the header retrieved is of the format 'key: value'
                auto pos = header.find(L": ");
                if (pos != wstring::npos)
                {
                    wstring key = header.substr(0, pos);
                    wstring val = header.substr(pos + 2);
                    m_webSocket->SetRequestHeader(StringReference(key.c_str()), StringReference(val.c_str()));
                }
            }
        }
        m_connectAction = m_webSocket->ConnectAsync(ref new Uri(StringReference(m_uri.c_str())));
    }
    catch (Exception^ e)
    {
        SPX_TRACE_ERROR("Exception: %ls", e->Message->Data());
        result = { WS_OPEN_ERROR_UNDERLYING_IO_OPEN_FAILED, 0, nullptr, 0 };
    }
    return result.result;
}

void WinRtWebSocket::Close(const std::chrono::milliseconds& polling_interval_ms, ON_WS_CLOSE_COMPLETE on_io_close_complete)
{
    SPX_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    (void)polling_interval_ms;
    if (m_state != WinRtWebSocketState::Open)
    {
        SPX_TRACE_ERROR("Invalid state: %d", m_state.load());
        throw runtime_error("Invalid state");
    }
    m_state = WinRtWebSocketState::Closing;
    m_onCloseComplete = on_io_close_complete;
    CloseInternal();
    DWORD waitResult = WaitForSingleObject(m_closeCompletedEvent, c_maxCloseWaitTimeMs);
    if (waitResult == WAIT_TIMEOUT)
    {
        SPX_TRACE_ERROR("Timeout while closing websocet");
    }
}

void WinRtWebSocket::CloseInternal()
{
    SPX_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    try
    {
        unique_lock<mutex> lock{ m_lock };
        if (m_connectAction)
        {
            m_connectAction->Cancel();
        }
        if (m_dataWriterStoreOperation)
        {
            m_dataWriterStoreOperation->Cancel();
        }
        if (!m_sendTask.is_done())
        {
            m_sendTask.wait();
        }
        if (m_webSocket)
        {
            m_webSocket->Close(1000, L"Closed due to user request.");
        }
        while (!m_readQueue.empty())
        {
            m_readQueue.pop();
        }
    }
    catch (Exception^ e)
    {
        SPX_TRACE_ERROR("Exception: %ls", e->Message->Data());
        m_error = true;
    }
    m_dataWriterStoreOperation = nullptr;
    m_connectAction = nullptr;
    m_webSocket = nullptr;
}

DataWriterStoreOperation^ WinRtWebSocket::SendPacket(MessageWebSocket^ webSocket, Platform::Array<BYTE>^ buffer, WebSocketMessageType msgType)
{
    auto control = webSocket->Control;
    control->MessageType = (msgType == WebSocketMessageType::Text) ? SocketMessageType::Utf8 : SocketMessageType::Binary;
    auto dataWriter = ref new DataWriter(webSocket->OutputStream);
    dataWriter->WriteBytes(buffer);
    m_dataWriterStoreOperation = dataWriter->StoreAsync();
    m_dataWriterStoreOperation->Completed = ref new AsyncOperationCompletedHandler<unsigned int>([&](IAsyncOperation<unsigned int>^ /*operation*/, AsyncStatus /*status*/)
        {
            SetEvent(m_dataWriterCompletedEvent);
        });
    WaitForSingleObject(m_dataWriterCompletedEvent, INFINITE);
    dataWriter->DetachStream();
    return m_dataWriterStoreOperation;
}

int WinRtWebSocket::Send(const unsigned char* buffer, size_t size, WebSocketMessageType message_type, ON_WS_SEND_FRAME_COMPLETE on_send_complete, void* callback_context)
{
    if (m_state != WinRtWebSocketState::Open)
    {
        SPX_TRACE_ERROR("Invalid state: %d", m_state.load());
        return WS_SEND_FRAME_ERROR;
    }
    if (!buffer || !size || !on_send_complete)
    {
        SPX_TRACE_ERROR("Invalid parameter");
        return WS_SEND_FRAME_ERROR;
    }
    auto bufferCopy = ref new Platform::Array<BYTE>((BYTE*)buffer, static_cast<unsigned int>(size));
    WeakReference weakRef(m_webSocket);
    auto sendRoutine = [=]()
    {
        auto webSocket = weakRef.Resolve<MessageWebSocket>();
        if (!webSocket)
        {
            return;
        }
        try
        {
            try
            {
                DataWriterStoreOperation^ operation = SendPacket(webSocket, bufferCopy, message_type);
                unsigned int bytesWritten = operation->GetResults();
                if (bytesWritten != static_cast<unsigned int>(size))
                {
                    // Should this be an error?
                }
                on_send_complete(callback_context, WS_SEND_FRAME_OK);
            }
            catch (Exception^ e)
            {
                //LogError("send packet operation returned error with hr: %#x, message: %ws", e->HResult, e->Message->Data());
                on_send_complete(callback_context, WS_SEND_FRAME_ERROR);
            }
        }
        catch (Exception^ ex)
        {
            SPX_TRACE_ERROR("Exception: %ls", ex->Message->Data());
            on_send_complete(callback_context, WS_SEND_FRAME_ERROR);
        }
    };
    unique_lock<mutex> lock{ m_lock };
    if (m_sendTask.is_done())
    {
        m_sendTask = create_task(sendRoutine);
    }
    else
    {
        m_sendTask = m_sendTask.then(sendRoutine);
    }
    lock.unlock();
    m_sendTask.wait();
    return WS_SEND_FRAME_OK;
}

void WinRtWebSocket::DoWork()
{
    int webErrorStatus = 0;
    try
    {
        if (m_connectAction)
        {
            WS_OPEN_RESULT_DETAILED result = { WS_OPEN_OK, 0, nullptr, 0 };
            auto action = m_connectAction;
            auto status = action->Status;
            auto errorCode = action->ErrorCode;
            int httpError = errorCode.Value & 0xfff;
            if (status != AsyncStatus::Started)
            {
                switch (status)
                {
                case AsyncStatus::Completed:
                    result = { WS_OPEN_OK, 0, nullptr, 0 };
                    break;

                case AsyncStatus::Canceled:
                    result = { WS_OPEN_ERROR_UNDERLYING_IO_OPEN_CANCELLED, httpError, nullptr, 0 };
                    break;

                default:
                    if ((httpError == 401) || (httpError == 400))
                    {
                        result = { WS_OPEN_ERROR_BAD_RESPONSE_STATUS, httpError, nullptr, 0 };
                    }
                    else if (httpError == 301 || httpError == 307 || httpError == 308)
                    {
                        SPX_TRACE_INFO("Redirect response", __FUNCTION__);
                        result = { WS_OPEN_ERROR_BAD_RESPONSE_STATUS, httpError, nullptr, 0 };
                    }
                    else
                    {
                        result = { WS_OPEN_ERROR_UNDERLYING_IO_OPEN_FAILED, httpError, nullptr, 0 };
                    }
                }
                m_connectAction = nullptr;
                if (status != AsyncStatus::Completed)
                {
                    CloseInternal();
                }
                OnWebSocketOpened(result);
            }
        }
        else
        {
            unique_lock<mutex> lock{ m_lock };
            auto t1 = GetTickCount64();
            while ((!m_readQueue.empty()) && ((GetTickCount64() - t1) < 100))
            {
                auto args = m_readQueue.front();
                m_readQueue.pop();
                lock.unlock();
                unsigned char msgType = (args->MessageType == SocketMessageType::Utf8) ? WS_FRAME_TYPE_TEXT : WS_FRAME_TYPE_BINARY;
                auto reader = args->GetDataReader();
                if (reader)
                {
                    reader->UnicodeEncoding = UnicodeEncoding::Utf8;
                    auto readBuffer = reader->ReadBuffer(reader->UnconsumedBufferLength);
                    auto memBuffer = Buffer::CreateMemoryBufferOverIBuffer(readBuffer);
                    auto memReference = memBuffer->CreateReference();
                    ComPtr<IUnknown> memBufferUnknown = reinterpret_cast<IUnknown*>(memReference);
                    ComPtr<IMemoryBufferByteAccess> memBytes;
                    memBufferUnknown.As(&memBytes);
                    BYTE* buffer = nullptr;
                    uint32 bufSize = 0;
                    (void)memBytes->GetBuffer(&buffer, &bufSize);
                    ON_WS_FRAME_RECEIVED onFrameReceived = m_onFrameReceived;
                    if (onFrameReceived)
                    {
                        onFrameReceived(m_callbackContext, msgType, buffer, bufSize);
                    }
                }
                lock.lock();
            }
        }
    }
    catch (Exception^ e)
    {
        SPX_TRACE_ERROR("Exception: %ls", e->Message->Data());
        m_error = true;
        webErrorStatus = static_cast<int>(WebSocketError::GetStatus(e->HResult));
        m_statusCode = (webErrorStatus >= 100) ? webErrorStatus : 0;
    }
    if (m_error)
    {
        CloseInternal();
        m_error = false;
        OnWebSocketError(WS_ERROR_UNDERLYING_IO_ERROR);
    }
}

int WinRtWebSocket::SetOption(const char* option_name, const void* value)
{
    SPX_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    // Not currently supported.
    (void)option_name;
    (void)value;
    return WS_ERROR_UNDERLYING_IO_ERROR;
}

int WinRtWebSocket::SetRequestHeader(const char* name, const char* value)
{
    return HTTPHeaders_AddHeaderNameValuePair(m_hHeaders, name, value);
}

int WinRtWebSocket::GetHttpStatus()
{
    return m_statusCode;
}

void WinRtWebSocket::OnWebSocketOpened(WS_OPEN_RESULT_DETAILED open_result_detailed)
{
    SPX_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    m_state = WinRtWebSocketState::Open;
    ON_WS_OPEN_COMPLETE onOpenComplete = m_onOpenComplete.load();
    if (onOpenComplete != nullptr)
    {
        onOpenComplete(m_callbackContext.load(), open_result_detailed);
    }
}

void WinRtWebSocket::OnWebSocketPeerClosed(uint16_t* closeCode, const unsigned char* extraData, size_t extra_data_length)
{
    SPX_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    m_state = WinRtWebSocketState::Initialized;
    ON_WS_PEER_CLOSED onPeerClosed = m_onPeerClosed;
    if (onPeerClosed)
    {
        onPeerClosed(m_callbackContext.load(), closeCode, extraData, extra_data_length);
    }
}

void WinRtWebSocket::OnWebSocketError(WS_ERROR errorCode)
{
    SPX_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    ON_WS_ERROR onError = m_onError;
    if (onError)
    {
        onError(m_callbackContext.load(), errorCode);
    }
}

void WinRtWebSocket::OnWebSocketClosed()
{
    SPX_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    m_state = WinRtWebSocketState::Initialized;
    ON_WS_CLOSE_COMPLETE onCloseComplete = m_onCloseComplete;
    if (onCloseComplete)
    {
        onCloseComplete(m_callbackContext.load());
    }
    SetEvent(m_closeCompletedEvent);
}

void WinRtWebSocket::OnMessageReceived(MessageWebSocket^ sender, MessageWebSocketMessageReceivedEventArgs^ args)
{
    SPX_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    lock_guard<mutex> lock{m_lock};
    m_readQueue.push(args);
}
