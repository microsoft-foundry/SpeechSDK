// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.
#pragma once

#include <chrono>
#include <memory>
#include "azure_c_shared_utility_httpapi_wrapper.h"

namespace HttpAdapter
{
    class IHttpAdapter
    {
    public:
        virtual ~IHttpAdapter() {};
        virtual void Initialize() = 0;
        virtual void Uninitialize() = 0;
        virtual int32_t OpenHttpConnection(
            const char* hostName,
            int port,
            bool secure,
            const char* proxyHost,
            int proxyPort,
            const char* proxyUserName,
            const char* proxyPassword) = 0;
        virtual void CloseHttpConnection() = 0;
        virtual int32_t ExecuteRequest(
            HTTPAPI_REQUEST_TYPE requestType,
            const char* relativePath,
            HTTP_HEADERS_HANDLE httpHeadersHandle,
            const unsigned char* content,
            size_t contentLength,
            unsigned int* statusCode,
            HTTP_HEADERS_HANDLE responseHeadersHandle,
            BUFFER_HANDLE responseContent) = 0;
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
            BUFFER_HANDLE responseContent) = 0;
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
            void* context) = 0;
        virtual int32_t SetOption(const char* option_name, const void* value) = 0;
        virtual int32_t GetLastError(int* errorCode) = 0;
    };
}

std::shared_ptr<HttpAdapter::IHttpAdapter> GetHttpAdapter();
