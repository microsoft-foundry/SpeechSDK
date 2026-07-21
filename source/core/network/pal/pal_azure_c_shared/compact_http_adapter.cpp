// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.
#include "stdafx.h"
#include "compact_http_adapter.h"
#include <future>
#include <stdexcept>
#include <thread>
#include <http_exception.h>

/// <summary>
/// Gets the last error code. You should call this if your HTTAPI_Execute... method returned an
/// error
/// </summary>
/// <param name="handle">The HTTP handle to get the last error from</param>
/// <param name="errorCode">The error code to set</param>
/// <returns>HTTPAPI_OK on success, or an error code otherwise</returns>
AZAC_EXTERN_C HTTPAPI_RESULT HTTPAPI_GetLastError(HTTP_HANDLE handle, int* errorCode);

using namespace HttpAdapter;

std::shared_ptr<IHttpAdapter> GetHttpAdapter()
{
    return std::make_shared<CompactHttpAdapter>();
}

CompactHttpAdapter::CompactHttpAdapter() :
    m_handle(nullptr),
    m_state(CompactHttpAdapterState::Uninitialized)
{
    SPX_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
}

CompactHttpAdapter::~CompactHttpAdapter()
{
    SPX_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
}

void CompactHttpAdapter::Initialize()
{
    SPX_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    if (m_state != CompactHttpAdapterState::Uninitialized)
    {
        SPX_TRACE_ERROR("Invalid state: %d", static_cast<int>(m_state.load()));
        throw std::runtime_error("Invalid state");
    }
    HTTPAPI_RESULT result = HTTPAPI_Init();
    if (result != HTTPAPI_OK)
    {
        SPX_TRACE_ERROR("Failed to initialize http api: %d", result);
        throw std::runtime_error("HTTP initialization failed");
    }
    m_state = CompactHttpAdapterState::Initialized;
}

void CompactHttpAdapter::Uninitialize()
{
    SPX_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    if (m_state != CompactHttpAdapterState::Initialized)
    {
        SPX_TRACE_ERROR("Invalid state: %d", static_cast<int>(m_state.load()));
        throw std:: runtime_error("Invalid state");
    }
    HTTPAPI_Deinit();
    m_state = CompactHttpAdapterState::Uninitialized;
}

int32_t CompactHttpAdapter::OpenHttpConnection(
    const char* hostName,
    int port,
    bool secure,
    const char* proxyHost,
    int proxyPort,
    const char* proxyUserName,
    const char* proxyPassword)
{
    SPX_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    int32_t result = HTTPAPI_OK;
    if (m_state != CompactHttpAdapterState::Initialized)
    {
        SPX_TRACE_ERROR("Invalid state: %d", static_cast<int>(m_state.load()));
        result = HTTPAPI_ERROR;
    }
    else
    {
        m_handle = HTTPAPI_CreateConnection_Advanced(
            hostName,
            port,
            secure,
            proxyHost,
            proxyPort,
            proxyUserName,
            proxyPassword);
        if (m_handle == nullptr)
        {
            SPX_TRACE_ERROR("Failed to create http connection");
            result = HTTPAPI_ERROR;
        }
        else
        {
            m_state = CompactHttpAdapterState::Open;
        }
    }
    return result;
}

void CompactHttpAdapter::CloseHttpConnection()
{
    SPX_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    if (m_state != CompactHttpAdapterState::Open)
    {
        SPX_TRACE_ERROR("Invalid state: %d", static_cast<int>(m_state.load()));
        throw std::runtime_error("Invalid state");
    }
    HTTPAPI_CloseConnection(m_handle);
    m_handle = nullptr;
    m_state = CompactHttpAdapterState::Initialized;
}

int32_t CompactHttpAdapter::ExecuteRequest(
    HTTPAPI_REQUEST_TYPE requestType,
    const char* relativePath,
    HTTP_HEADERS_HANDLE httpHeadersHandle,
    const unsigned char* content,
    size_t contentLength,
    unsigned int* statusCode,
    HTTP_HEADERS_HANDLE responseHeadersHandle,
    BUFFER_HANDLE responseContent)
{
    int32_t result = HTTPAPI_OK;
    if (m_state != CompactHttpAdapterState::Open)
    {
        SPX_TRACE_ERROR("Invalid state: %d", static_cast<int>(m_state.load()));
        result = HTTPAPI_ERROR;
    }
    else
    {
        result = HTTPAPI_ExecuteRequest(
            m_handle,
            requestType,
            relativePath,
            httpHeadersHandle,
            (const unsigned char*)content,
            contentLength,
            statusCode,
            responseHeadersHandle,
            responseContent);
    }
    return result;
}

int32_t CompactHttpAdapter::ExecuteRequestWithReasonPhrase(
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
    int32_t result = HTTPAPI_OK;
    if (m_state != CompactHttpAdapterState::Open)
    {
        SPX_TRACE_ERROR("Invalid state: %d", static_cast<int>(m_state.load()));
        result = HTTPAPI_ERROR;
    }
    else
    {
        result = HTTPAPI_ExecuteRequest_With_Reason_Phrase(
            m_handle,
            requestType,
            relativePath,
            httpHeadersHandle,
            (const unsigned char*)content,
            contentLength,
            statusCode,
            reasonPhrase,
            maxReasonPhraseSize,
            responseHeadersHandle,
            responseContent);
    }
    return result;
}

int32_t CompactHttpAdapter::ExecuteRequestWithStreaming(
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
    int32_t result = HTTPAPI_OK;
    if (m_state != CompactHttpAdapterState::Open)
    {
        SPX_TRACE_ERROR("Invalid state: %d", static_cast<int>(m_state.load()));
        result = HTTPAPI_ERROR;
    }
    else
    {
        result = HTTPAPI_ExecuteRequest_With_Streaming(
            m_handle,
            requestType,
            relativePath,
            httpHeadersHandle,
            (const unsigned char*)content,
            contentLength,
            statusCode,
            reasonPhrase,
            maxReasonPhraseSize,
            responseHeadersHandle,
            onChunkReceived,
            context);
    }
    return result;
}

int32_t CompactHttpAdapter::SetOption(const char* optionName, const void* value)
{
    SPX_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    int32_t result = HTTPAPI_SetOption(m_handle, optionName, value);
    return result;
}

int32_t CompactHttpAdapter::GetLastError(int* errorCode)
{
    return HTTPAPI_GetLastError(m_handle, errorCode);
}
