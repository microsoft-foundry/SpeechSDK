// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.
#include "stdafx.h"
#include <stdlib.h>
#include "azure_c_shared_utility_httpapi_wrapper.h"
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
#include <set>
#include "winrt_http_adapter.h"
#include "http_exception.h"

using namespace std;
using namespace Microsoft::WRL;
using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Storage::Streams;
using namespace Windows::Networking::Sockets;
using namespace concurrency;
using namespace Windows::Security::Credentials;
using namespace HttpAdapter;
using namespace Windows::Web::Http;
using namespace Windows::Web::Http::Headers;
using namespace Windows::Web::Http::Filters;
using namespace Windows::Security::Cryptography;
using namespace Windows::Storage::Streams;

std::atomic<size_t> WinRtHttpAdapter::s_connectionNumber = 0;

std::shared_ptr<IHttpAdapter> GetHttpAdapter()
{
    return std::make_shared<WinRtHttpAdapter>();
}

inline static wstring to_wstring(const char* str)
{
    return wstring{ str, str + strlen(str) };
}

WinRtHttpAdapter::WinRtHttpAdapter() :
    m_httpClient(nullptr),
    m_host(L""),
    m_port(0),
    m_state(WinRtHttpAdapterState::Uninitialized),
    m_sendOperation(nullptr),
    m_readContentOperation(nullptr),
    m_sendOperationCompletionEvent(NULL)
{
    SPX_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    m_sendOperationCompletionEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (m_sendOperationCompletionEvent == NULL)
    {
        SPX_TRACE_ERROR("EEvent creation failed");
        throw runtime_error("Event creation failed");
    }
}

WinRtHttpAdapter::~WinRtHttpAdapter()
{
    SPX_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    CloseInternal();
    UninitializeInternal();
}

void WinRtHttpAdapter::Initialize()
{
    SPX_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    if (m_state != WinRtHttpAdapterState::Uninitialized)
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
    m_state = WinRtHttpAdapterState::Initialized;
}

void WinRtHttpAdapter::Uninitialize()
{
    SPX_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    if (m_state != WinRtHttpAdapterState::Initialized)
    {
        SPX_TRACE_ERROR("Invalid state: %d", m_state.load());
        throw runtime_error("Invalid state");
    }
    UninitializeInternal();
}

void WinRtHttpAdapter::UninitializeInternal()
{
    SPX_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    if (m_state == WinRtHttpAdapterState::Uninitialized)
    {
        return;
    }
    if (m_sendOperationCompletionEvent != NULL)
    {
        CloseHandle(m_sendOperationCompletionEvent);
        m_sendOperationCompletionEvent = NULL;
    }
    RoUninitialize();
    m_state = WinRtHttpAdapterState::Uninitialized;
}

int32_t WinRtHttpAdapter::OpenHttpConnection(
    const char* hostName,
    int port,
    bool secure,
    const char* proxyHost,
    int proxyPort,
    const char* proxyUserName,
    const char* proxyPassword)
{
    SPX_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    (void)proxyHost;
    (void)proxyPort;
    (void)proxyUserName;
    (void)proxyPassword;
    SPX_TRACE_INFO(R"(Using WinRtHttpAdapter: 0x%x)", this);
    if (m_state != WinRtHttpAdapterState::Initialized)
    {
        SPX_TRACE_ERROR("Invalid state: %d", m_state.load());
        return HTTPAPI_ERROR;
    }
    if (hostName == nullptr)
    {
        SPX_TRACE_ERROR("Hostname == nullptr");
        return HTTPAPI_INVALID_ARG;
    }
    else if (to_wstring(hostName).empty())
    {
        SPX_TRACE_ERROR("Hostname is empty");
        return HTTPAPI_INVALID_ARG;
    }
    m_httpClient = ref new HttpClient();
    wstring uriString = L"";
    string hostNameString(hostName);
    string hostNameStringLowerCase;
    hostNameStringLowerCase.resize(hostNameString.length());
#pragma warning(push)
#pragma warning(disable : 4242)
#pragma warning(disable : 4244)
    transform(hostNameString.begin(), hostNameString.end(), hostNameStringLowerCase.begin(), ::tolower);
#pragma warning(pop)
    if ((strncmp(hostNameStringLowerCase.c_str(), "http", 4) != 0) && (strncmp(hostNameStringLowerCase.c_str(), "https", 5) != 0))
    {
        if (secure)
        {
            uriString = L"https://";
        }
        else
        {
            uriString = L"http://";
        }
    }
    uriString += to_wstring(hostName);
    uriString += L":";
    uriString += to_wstring(port);
    m_host = uriString;
    m_state = WinRtHttpAdapterState::Open;
    return HTTPAPI_OK;
}

void WinRtHttpAdapter::CloseHttpConnection()
{
    SPX_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    if (m_state != WinRtHttpAdapterState::Open)
    {
        SPX_TRACE_ERROR("Invalid state: %d", m_state.load());
        throw runtime_error("Invalid state");
    }
    CloseInternal();
}

void WinRtHttpAdapter::CloseInternal()
{
    SPX_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    lock_guard<mutex> lock(m_operationMutex);
    if (m_sendOperation)
    {
        m_sendOperation->Cancel();
        m_sendOperation = nullptr;
    }
    if (m_readContentOperation)
    {
        m_readContentOperation->Cancel();
        m_readContentOperation = nullptr;
    }
    if (m_streamContentOperation)
    {
        m_streamContentOperation->Cancel();
        m_streamContentOperation = nullptr;
    }
    if (m_httpClient)
    {
        delete m_httpClient;
        m_httpClient = nullptr;
    }
    m_host = L"";
    m_state = WinRtHttpAdapterState::Initialized;
}

bool WinRtHttpAdapter::AddRequestHeader(HttpRequestMessage^ request, wstring key, wstring value, size_t contentLength)
{
    SPX_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    set<wstring> contentHeaderKeys{
        L"Content-Type",
        L"Content-Disposition",
        L"Content-Encoding",
        L"Content-Language",
        L"Content-Length",
        L"Content-Location",
        L"Content-MD5",
        L"Content-Range",
        L"Expires",
    };
    if ((contentLength > 0) && (contentHeaderKeys.find(key) != contentHeaderKeys.end()))
    {
        return request->Content->Headers->TryAppendWithoutValidation(StringReference(key.c_str()), StringReference(value.c_str()));
    }
    return request->Headers->TryAppendWithoutValidation(StringReference(key.c_str()), StringReference(value.c_str()));
}

#pragma warning(push)
#pragma warning(disable : 4244) // 'argument': conversion from 'wchar_t' to 'const _Elem', possible loss of data
// Make sure your input string contains "standard" (8-bit ASCII) characters only
std::string WinRtHttpAdapter::ConvertPlatformStringToStdString(Platform::String^ str)
{
    std::wstring wstr(str->Data());
    return std::string(wstr.begin(), wstr.end());
}
#pragma warning(pop)

int32_t WinRtHttpAdapter::ExecuteRequest(
    HTTPAPI_REQUEST_TYPE requestType,
    const char* relativePath,
    HTTP_HEADERS_HANDLE httpHeadersHandle,
    const unsigned char* content,
    size_t contentLength,
    unsigned int* statusCode,
    HTTP_HEADERS_HANDLE responseHeadersHandle,
    BUFFER_HANDLE responseContent)
{
    *statusCode = 0;
    if (m_state != WinRtHttpAdapterState::Open)
    {
        SPX_TRACE_ERROR("Invalid state: %d", m_state.load());
        return HTTPAPI_ERROR;
    }
    AsyncStatus operationStatus = AsyncStatus::Error;
    try
    {
        HttpMethod^ httpMethod;
        switch (requestType)
        {
        case HTTPAPI_REQUEST_GET:
            httpMethod = HttpMethod::Get;
            break;

        case HTTPAPI_REQUEST_POST:
            httpMethod = HttpMethod::Post;
            break;

        case HTTPAPI_REQUEST_PUT:
            httpMethod = HttpMethod::Put;
            break;

        case HTTPAPI_REQUEST_DELETE:
            httpMethod = HttpMethod::Delete;
            break;

        case HTTPAPI_REQUEST_PATCH:
            httpMethod = HttpMethod::Patch;
            break;

        default:
            return HTTPAPI_INVALID_ARG;
        }
        wstring uri = m_host;
        if (relativePath)
        {
            uri += to_wstring(relativePath);
            // WinRT implementation caches connetions to the same address for a process, and header updates might not get correctly taken into account
            // (e.g. changing subscription key value). Add an increasing number as a quesry string parameter to force a reconnection.
            if (strchr(relativePath, '?') == NULL)
            {
                uri += L"?c=";
            }
            else
            {
                uri += L"&c=";
            }
            uri += to_wstring(s_connectionNumber++);
        }
        Uri^ u = ref new Uri(StringReference(uri.c_str()));
        SPX_TRACE_INFO(R"(WinRtHttpAdapter: 0x%x connecting to %ls)", this, uri.c_str());
        auto requestMessage = ref new HttpRequestMessage(httpMethod, u);
        //Content
        if (content && contentLength > 0)
        {
            auto contentBuffer = CryptographicBuffer::CreateFromByteArray(
                ArrayReference<uint8_t>(const_cast<uint8_t*>(content), static_cast<unsigned int>(contentLength))
            );
            requestMessage->Content = ref new HttpBufferContent(contentBuffer);
        }
        size_t headersCount = 0;
        if (httpHeadersHandle)
        {
            if (HTTPHeaders_GetHeaderCount(httpHeadersHandle, &headersCount) != HTTP_HEADERS_OK)
            {
                SPX_TRACE_ERROR("Failed to get header count");
                return HTTPAPI_QUERY_HEADERS_FAILED;
            }
        }
        for (size_t i = 0; i < headersCount; i++)
        {
            char* temp = nullptr;
            if (HTTPHeaders_GetHeader(httpHeadersHandle, i, &temp) == HTTP_HEADERS_OK && temp)
            {
                wstring header = to_wstring(temp);
                free(temp);
                //the header retrieved is of the format 'key: value'
                auto pos = header.find(L": ");
                if (pos != wstring::npos)
                {
                    wstring key = header.substr(0, pos);
                    wstring value = header.substr(pos + 2);
                    AddRequestHeader(requestMessage, key, value, contentLength);
                }
            }
            else
            {
                SPX_TRACE_ERROR("Failed to get header");
                return HTTPAPI_QUERY_HEADERS_FAILED;
            }
        }
        unique_lock<mutex> lock(m_operationMutex);
        m_sendOperation = m_httpClient->SendRequestAsync(requestMessage, HttpCompletionOption::ResponseContentRead);
        lock.unlock();
        m_sendOperation->Completed = ref new AsyncOperationWithProgressCompletedHandler<HttpResponseMessage^, HttpProgress>([&](IAsyncOperationWithProgress<HttpResponseMessage^, HttpProgress>^ /*asyncInfo*/, AsyncStatus status)
        {
            operationStatus = status;
            if (status == AsyncStatus::Completed)
            {
                auto response = m_sendOperation->GetResults();
                lock.lock();
                m_readContentOperation = response->Content->ReadAsBufferAsync();
                lock.unlock();
                m_readContentOperation->Completed = ref new AsyncOperationWithProgressCompletedHandler<IBuffer^, uint64>([response, &statusCode, this, &operationStatus, &responseHeadersHandle, &responseContent](IAsyncOperationWithProgress<IBuffer^, uint64>^ /*asyncInfo*/, AsyncStatus status)
                {
                    operationStatus = status;
                    if (status == AsyncStatus::Completed)
                    {
                        auto contentBuffer = m_readContentOperation->GetResults();
                        auto memBuffer = Buffer::CreateMemoryBufferOverIBuffer(contentBuffer);
                        auto memReference = memBuffer->CreateReference();
                        ComPtr<IUnknown> memBufferUnknown = reinterpret_cast<IUnknown*>(memReference);
                        ComPtr<IMemoryBufferByteAccess> memBytes;
                        memBufferUnknown.As(&memBytes);
                        BYTE* buffer = nullptr;
                        UINT32 bufSize = 0;
                        (void)memBytes->GetBuffer(&buffer, &bufSize);
                        auto statusCodeFromResponse = static_cast<unsigned int>(response->StatusCode);
                        auto headers = response->Headers;
                        std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
                        auto headerIterator = headers->First();
                        for (unsigned int i = 0; i < headers->Size; i++)
                        {
                            auto hdr = headerIterator->Current;
                            headerIterator->MoveNext();
                            string key = converter.to_bytes(hdr->Key->Begin());
                            string value = converter.to_bytes(hdr->Value->Begin());
                            HTTPHeaders_AddHeaderNameValuePair(responseHeadersHandle, key.c_str(), value.c_str());
                        }

                        // The calling Platform Abstraction Layer (PAL), file http_request.cpp, expects the content
                        // type and content length to be available to read as HTTP headers. This is how the Native
                        // (OpenSSL) implementation exposes these values. WinRT APIs do not expose this information
                        // as HTTP headers, but as strongly typed members on the response class. Write those values
                        // here to the output HTTP headers list, so the behavior is the same regardless of the
                        // networking implementation (WinRT APIs or OpenSSL).
                        Platform::String^ contentType = response->Content->Headers->ContentType->MediaType;
                        if (contentType != nullptr && !contentType->IsEmpty())
                        {
                            HTTPHeaders_AddHeaderNameValuePair(responseHeadersHandle, "Content-Type", ConvertPlatformStringToStdString(contentType).c_str());
                        }

                        Platform::IBox<uint64_t>^ contentLength = response->Content->Headers->ContentLength;
                        if (contentLength != nullptr && contentLength->Value > 0)
                        {
                            // Note that contentLength->Value should equal contentBuffer->Length
                            HTTPHeaders_AddHeaderNameValuePair(responseHeadersHandle, "Content-Length", std::to_string(contentLength->Value).c_str());
                        }

                        *statusCode = statusCodeFromResponse;
                        BUFFER_build(responseContent, buffer, contentBuffer->Length);
                    }
                    else if (status == AsyncStatus::Error)
                    {
                        *statusCode = static_cast<unsigned int>(response->StatusCode);
                    }
                    SetEvent(m_sendOperationCompletionEvent);
                });
            }
            else
            {
                SetEvent(m_sendOperationCompletionEvent);
            }
        });
    }
    catch (Exception^ e)
    {
        SPX_TRACE_ERROR("Exception: %ls", e->Message->Data());
        return HTTPAPI_SEND_REQUEST_FAILED;
    }
    WaitForSingleObject(m_sendOperationCompletionEvent, INFINITE);
    auto readOp = m_readContentOperation;
    auto sendOp = m_sendOperation;
    unique_lock<mutex> lock2(m_operationMutex);
    m_sendOperation = nullptr;
    m_readContentOperation = nullptr;
    lock2.unlock();
    if (readOp != nullptr)
    {
        if (operationStatus == AsyncStatus::Error)
        {
            SPX_TRACE_ERROR("Read operation completed with error");
            return HTTPAPI_ERROR;
        }
        else if (operationStatus == AsyncStatus::Canceled)
        {
            SPX_TRACE_ERROR("Read operation was canceled");
            return HTTPAPI_ERROR;
        }
    }
    else if (sendOp != nullptr)
    {
        if (operationStatus == AsyncStatus::Error)
        {
            SPX_TRACE_ERROR("Send operation completed with error");
            return HTTPAPI_SEND_REQUEST_FAILED;
        }
        else if (operationStatus == AsyncStatus::Canceled)
        {
            SPX_TRACE_ERROR("Send operation canceled");
            return HTTPAPI_SEND_REQUEST_FAILED;
        }
    }
    return HTTPAPI_OK;
}

int32_t WinRtHttpAdapter::ExecuteRequestWithReasonPhrase(
    HTTPAPI_REQUEST_TYPE requestType,
    const char* relativePath,
    HTTP_HEADERS_HANDLE httpHeadersHandle,
    const unsigned char* content,
    size_t contentLength,
    unsigned int* statusCode,
    char* reasonPhrase,
    const size_t maxReasonPhraseSize,
    HTTP_HEADERS_HANDLE responseHeadersHandle,
    BUFFER_HANDLE responseContent)
{
    (void)reasonPhrase;
    (void)maxReasonPhraseSize;
    return ExecuteRequest(
        requestType,
        relativePath,
        httpHeadersHandle,
        content,
        contentLength,
        statusCode,
        responseHeadersHandle,
        responseContent);
}

int32_t WinRtHttpAdapter::ExecuteRequestWithStreaming(
    HTTPAPI_REQUEST_TYPE requestType,
    const char* relativePath,
    HTTP_HEADERS_HANDLE httpHeadersHandle,
    const unsigned char* content,
    size_t contentLength,
    unsigned int* statusCode,
    char* reasonPhrase,
    const size_t maxReasonPhraseSize,
    HTTP_HEADERS_HANDLE responseHeadersHandle,
    ON_CHUNK_RECEIVED onChunkReceived,
    void* context)
{
    AsyncStatus operationStatus = AsyncStatus::Error;
    HttpResponseMessage^ response;
    IInputStream^ inputStream;
    bool done = false;
    *statusCode = 0;
    if (m_state != WinRtHttpAdapterState::Open)
    {
        SPX_TRACE_ERROR("Invalid state: %d", m_state.load());
        return HTTPAPI_ERROR;
    }
    auto buffer = ref new Buffer(c_chunkSize);
    try
    {
        HttpMethod^ httpMethod;
        switch (requestType)
        {
        case HTTPAPI_REQUEST_GET:
            httpMethod = HttpMethod::Get;
            break;

        case HTTPAPI_REQUEST_POST:
            httpMethod = HttpMethod::Post;
            break;

        case HTTPAPI_REQUEST_PUT:
            httpMethod = HttpMethod::Put;
            break;

        case HTTPAPI_REQUEST_DELETE:
            httpMethod = HttpMethod::Delete;
            break;

        case HTTPAPI_REQUEST_PATCH:
            httpMethod = HttpMethod::Patch;
            break;

        default:
            return HTTPAPI_INVALID_ARG;
        }
        wstring uri = m_host;
        if (relativePath)
        {
            uri += to_wstring(relativePath);
            // WinRT implementation caches connetions to the same address for a process, and header updates might not get correctly taken into account
            // (e.g. changing subscription key value). Add an increasing number as a quesry string parameter to force a reconnection.
            if (strchr(relativePath, '?') == NULL)
            {
                uri += L"?c=";
            }
            else
            {
                uri += L"&c=";
            }
            uri += to_wstring(s_connectionNumber++);
        }
        Uri^ u = ref new Uri(StringReference(uri.c_str()));
        auto requestMessage = ref new HttpRequestMessage(httpMethod, u);
        //Content
        if (content && contentLength > 0)
        {
            auto contentBuffer = CryptographicBuffer::CreateFromByteArray(
                ArrayReference<uint8_t>(const_cast<uint8_t*>(content), static_cast<unsigned int>(contentLength))
            );
            requestMessage->Content = ref new HttpBufferContent(contentBuffer);
        }
        size_t headersCount = 0;
        if (httpHeadersHandle)
        {
            if (HTTPHeaders_GetHeaderCount(httpHeadersHandle, &headersCount) != HTTP_HEADERS_OK)
            {
                SPX_TRACE_ERROR("Failed to get header count");
                return HTTPAPI_QUERY_HEADERS_FAILED;
            }
        }
        for (size_t i = 0; i < headersCount; i++)
        {
            char* temp = nullptr;
            if (HTTPHeaders_GetHeader(httpHeadersHandle, i, &temp) == HTTP_HEADERS_OK && temp)
            {
                wstring header = to_wstring(temp);
                free(temp);
                //the header retrieved is of the format 'key: value'
                auto pos = header.find(L": ");
                if (pos != wstring::npos)
                {
                    wstring key = header.substr(0, pos);
                    wstring value = header.substr(pos + 2);
                    AddRequestHeader(requestMessage, key, value, contentLength);
                }
            }
            else
            {
                SPX_TRACE_ERROR("Failed to get header");
                return HTTPAPI_QUERY_HEADERS_FAILED;
            }
        }
        unique_lock<mutex> lock(m_operationMutex);
        m_sendOperation = m_httpClient->SendRequestAsync(requestMessage, HttpCompletionOption::ResponseContentRead);
        lock.unlock();
        m_sendOperation->Completed = ref new AsyncOperationWithProgressCompletedHandler<HttpResponseMessage^, HttpProgress>([&](IAsyncOperationWithProgress<HttpResponseMessage^, HttpProgress>^ /*asyncInfo*/, AsyncStatus status)
            {
                operationStatus = status;
                if (status == AsyncStatus::Completed)
                {
                    response = m_sendOperation->GetResults();
                    lock.lock();
                    m_streamContentOperation = response->Content->ReadAsInputStreamAsync();
                    lock.unlock();
                    m_streamContentOperation->Completed = ref new AsyncOperationWithProgressCompletedHandler<IInputStream^, uint64>([&](IAsyncOperationWithProgress<IInputStream^, uint64>^ asyncInfo, AsyncStatus status)
                        {
                            operationStatus = status;
                            if (status == AsyncStatus::Completed)
                            {
                                auto statusCodeFromResponse = static_cast<unsigned int>(response->StatusCode);
                                auto headers = response->Headers;
                                std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
                                auto headerIterator = headers->First();
                                for (unsigned int i = 0; i < headers->Size; i++)
                                {
                                    auto hdr = headerIterator->Current;
                                    headerIterator->MoveNext();
                                    string key = converter.to_bytes(hdr->Key->Begin());
                                    string value = converter.to_bytes(hdr->Value->Begin());
                                    HTTPHeaders_AddHeaderNameValuePair(responseHeadersHandle, key.c_str(), value.c_str());
                                }
                                *statusCode = statusCodeFromResponse;
                                inputStream = asyncInfo->GetResults();
                                while (!done)
                                {
                                    m_readStreamOperation = inputStream->ReadAsync(buffer, c_chunkSize, InputStreamOptions::Partial);
                                    m_readStreamOperation->Completed = ref new AsyncOperationWithProgressCompletedHandler<IBuffer^, unsigned int>([&](IAsyncOperationWithProgress<IBuffer^, unsigned int>^ asyncInfo, AsyncStatus status)
                                        {
                                            if (status == AsyncStatus::Completed)
                                            {
                                                if (maxReasonPhraseSize > 0)
                                                {
                                                    auto reasonPhrasePlatformString = response->ReasonPhrase;
                                                    if (reasonPhrasePlatformString != nullptr)
                                                    {
                                                        string reasonPhraseString = converter.to_bytes(reasonPhrasePlatformString->Begin());
                                                        strncpy_s(reasonPhrase, maxReasonPhraseSize, reasonPhraseString.c_str(), _TRUNCATE);
                                                    }
                                                }
                                                IBuffer^ resultBuffer = asyncInfo->GetResults();
                                                unsigned int length = resultBuffer->Length;
                                                if (length == 0)
                                                {
                                                    done = true;
                                                    return;
                                                }
                                                auto memBuffer = Buffer::CreateMemoryBufferOverIBuffer(resultBuffer);
                                                auto memReference = memBuffer->CreateReference();
                                                ComPtr<IUnknown> memBufferUnknown = reinterpret_cast<IUnknown*>(memReference);
                                                ComPtr<IMemoryBufferByteAccess> memBytes;
                                                memBufferUnknown.As(&memBytes);
                                                BYTE* byteBuffer = nullptr;
                                                UINT32 bufSize = 0;
                                                (void)memBytes->GetBuffer(&byteBuffer, &bufSize);
                                                onChunkReceived(context, byteBuffer, bufSize);
                                            }
                                            else if (status == AsyncStatus::Error)
                                            {
                                                done = true;
                                                *statusCode = static_cast<unsigned int>(response->StatusCode);
                                            }
                                            else
                                            {
                                                done = true;
                                            }
                                        });
                                }
                                SetEvent(m_sendOperationCompletionEvent);
                            }
                            else
                            {
                                SetEvent(m_sendOperationCompletionEvent);
                            }
                        });
                }
                else
                {
                    SetEvent(m_sendOperationCompletionEvent);
                }
            });
    }
    catch (Exception^ e)
    {
        SPX_TRACE_ERROR("Exception: %ls", e->Message->Data());
        return HTTPAPI_SEND_REQUEST_FAILED;
    }
    WaitForSingleObject(m_sendOperationCompletionEvent, INFINITE);
    auto streamOp = m_streamContentOperation;
    auto sendOp = m_sendOperation;
    unique_lock<mutex> lock2(m_operationMutex);
    m_sendOperation = nullptr;
    m_streamContentOperation = nullptr;
    lock2.unlock();
    if (streamOp != nullptr)
    {
        if (operationStatus == AsyncStatus::Error)
        {
            SPX_TRACE_ERROR("Stream operation completed with error");
            return HTTPAPI_ERROR;
        }
        else if (operationStatus == AsyncStatus::Canceled)
        {
            SPX_TRACE_ERROR("Stream operation canceled");
            return HTTPAPI_ERROR;
        }
    }
    else if (sendOp != nullptr)
    {
        if (operationStatus == AsyncStatus::Error)
        {
            SPX_TRACE_ERROR("Send operation completed with error");
            return HTTPAPI_SEND_REQUEST_FAILED;
        }
        else if (operationStatus == AsyncStatus::Canceled)
        {
            SPX_TRACE_ERROR("Send operation canceled");
            return HTTPAPI_SEND_REQUEST_FAILED;
        }
    }
    return HTTPAPI_OK;
}

int32_t WinRtHttpAdapter::SetOption(const char* option_name, const void* value)
{
    SPX_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    // Not currently supported.
    (void)option_name;
    (void)value;
    return HTTPAPI_ERROR;
}

int32_t WinRtHttpAdapter::GetLastError(int* errorCode)
{
    if (errorCode == NULL)
    {
        return HTTPAPI_INVALID_ARG;
    }
    else
    {
        errorCode = 0;
        return HTTPAPI_OK;
    }
}
