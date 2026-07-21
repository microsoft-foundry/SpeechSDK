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
#include "i_http_adapter.h"

namespace HttpAdapter
{
    enum class WinRtHttpAdapterState
    {
        Uninitialized,
        Initialized,
        Open,
        Closing
    };

    class WinRtHttpAdapter : public IHttpAdapter, public std::enable_shared_from_this<WinRtHttpAdapter>
    {
    public:
        WinRtHttpAdapter();
        virtual ~WinRtHttpAdapter();
        virtual void Initialize();
        virtual void Uninitialize();
        virtual int32_t OpenHttpConnection(
            const char* hostName,
            int port,
            bool secure,
            const char* proxyHost,
            int proxyPort,
            const char* proxyUserName,
            const char* proxyPassword);
        virtual void CloseHttpConnection();
        virtual int32_t ExecuteRequest(
            HTTPAPI_REQUEST_TYPE requestType,
            const char* relativePath,
            HTTP_HEADERS_HANDLE httpHeadersHandle,
            const unsigned char* content,
            size_t contentLength,
            unsigned int* statusCode,
            HTTP_HEADERS_HANDLE responseHeadersHandle,
            BUFFER_HANDLE responseContent);
        virtual int32_t ExecuteRequestWithReasonPhrase(
            HTTPAPI_REQUEST_TYPE requestType,
            const char* relativePath,
            HTTP_HEADERS_HANDLE httpHeadersHandle,
            const unsigned char* content,
            size_t contentLength,
            unsigned int* statusCode,
            char* reasonPhrase,
            const size_t maxReasonPhraseSize,
            HTTP_HEADERS_HANDLE responseHeadersHandle,
            BUFFER_HANDLE responseContent);
        virtual int32_t ExecuteRequestWithStreaming(
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
            void* context);
        virtual int32_t SetOption(const char* option_name, const void* value);
        virtual int32_t GetLastError(int* errorCode);

    private:
        bool AddRequestHeader(Windows::Web::Http::HttpRequestMessage^ request, std::wstring key, std::wstring value, size_t contentLength);
        void CloseInternal();
        void UninitializeInternal();
        static std::string ConvertPlatformStringToStdString(Platform::String^ str);

    private:
        static std::atomic<size_t> s_connectionNumber;
        static constexpr unsigned int c_chunkSize = 2048;
        Windows::Web::Http::HttpClient^ m_httpClient;
        std::wstring m_host;
        int m_port;
        std::atomic<WinRtHttpAdapterState> m_state;
        Windows::Foundation::IAsyncOperationWithProgress<Windows::Web::Http::HttpResponseMessage^, Windows::Web::Http::HttpProgress>^ m_sendOperation;
        Windows::Foundation::IAsyncOperationWithProgress<Windows::Storage::Streams::IBuffer^, uint64>^ m_readContentOperation;
        Windows::Foundation::IAsyncOperationWithProgress<Windows::Storage::Streams::IInputStream^, uint64>^ m_streamContentOperation;
        Windows::Foundation::IAsyncOperationWithProgress<Windows::Storage::Streams::IBuffer^, unsigned int>^ m_readStreamOperation;
        HANDLE m_sendOperationCompletionEvent;
        std::mutex m_operationMutex;
    };
}
