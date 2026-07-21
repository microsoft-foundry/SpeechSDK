// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.
#pragma once

#include <atomic>
#include <chrono>
#include "i_http_adapter.h"

namespace HttpAdapter
{
    enum class CompactHttpAdapterState
    {
        Uninitialized,
        Initialized,
        Open,
        Closing
    };

    class CompactHttpAdapter : public IHttpAdapter
    {
    public:
        CompactHttpAdapter();
        virtual ~CompactHttpAdapter();
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
        virtual int32_t SetOption(const char* optionName, const void* value);
        virtual int32_t GetLastError(int* errorCode);

    private:
        HTTP_HANDLE m_handle;
        std::atomic<CompactHttpAdapterState> m_state;
    };
}
