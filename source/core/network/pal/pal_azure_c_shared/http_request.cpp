//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"
#include <sstream>
#include "azure_c_shared_utility_urlencode_wrapper.h"
#include "azure_c_shared_utility_uws_client_wrapper.h"
#include "azure_c_shared_utility_error_converter.h"
#include "default_http_error_handler.h"
#include "http_utils.h"
#include "http_request.h"
#include "http_response.h"
#include "http_exception.h"
#include "i_http_adapter.h"
#include "http_platform_impl.h"

#ifdef SPEECHSDK_USE_OPENSSL
    #include <azure_c_shared_utility/shared_util_options.h>
#endif

/// <summary>
/// Gets the last error code. You should call this if your HTTAPI_Execute... method returned an
/// error
/// </summary>
/// <param name="handle">The HTTP handle to get the last error from</param>
/// <param name="errorCode">The error code to set</param>
/// <returns>HTTPAPI_OK on success, or an error code otherwise</returns>
AZAC_EXTERN_C HTTPAPI_RESULT HTTPAPI_GetLastError(HTTP_HANDLE handle, int* errorCode);

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

    static void AddHeader(HTTP_HEADERS_HANDLE hHeaders, const char* name, const char* value)
    {
        if (HTTPHeaders_AddHeaderNameValuePair(hHeaders, name, value) != HTTP_HEADERS_OK)
        {
            ThrowRuntimeError("Could not add HTTP request header", 1);
        }
    }

    static HTTPAPI_REQUEST_TYPE ToRequestType(HttpMethod method)
    {
        switch (method)
        {
        default:
        case HttpMethod::Get:
            return HTTPAPI_REQUEST_TYPE::HTTPAPI_REQUEST_GET;
        case HttpMethod::Delete: return HTTPAPI_REQUEST_TYPE::HTTPAPI_REQUEST_DELETE;
        case HttpMethod::Patch:  return HTTPAPI_REQUEST_TYPE::HTTPAPI_REQUEST_PATCH;
        case HttpMethod::Post:   return HTTPAPI_REQUEST_TYPE::HTTPAPI_REQUEST_POST;
        case HttpMethod::Put:    return HTTPAPI_REQUEST_TYPE::HTTPAPI_REQUEST_PUT;
        }
    }

    CSpxHttpRequest::CSpxHttpRequest() :
        m_requestHeaders(nullptr)
    {
    }

    CSpxHttpRequest::~CSpxHttpRequest()
    {
        ClearHeaders();
    }

    std::unique_ptr<ISpxHttpResponse> CSpxHttpRequest::SendRequest(HttpMethod method, const IHttpEndpointInfo& endpoint, const uint8_t* content, size_t contentSize, const ISpxHttpErrorHandler::ConstPtr& errorHandler)
    {
        auto responsePtr = CreateAndConfigureRequest(endpoint, contentSize, errorHandler);
        auto response = static_cast<CSpxHttpResponse*>(responsePtr.get());
        response->m_requestMethod = method;

        std::string httpPath = endpoint.Path() + endpoint.QueryString();

        int32_t result = response->m_httpAdapter->ExecuteRequestWithReasonPhrase(
            ToRequestType(method),
            httpPath.c_str(),
            m_requestHeaders,
            (const unsigned char*)content,
            contentSize,
            &response->m_statusCode,
            response->m_reasonPhrase,
            CSpxHttpResponse::REASON_PHRASE_MAX_LENGTH,
            response->m_responseHeaders,
            response->m_buffer);

        int innerErrorCode;
        if (response->m_httpAdapter->GetLastError(&innerErrorCode) != HTTPAPI_OK)
        {
            innerErrorCode = 0;
        }

        auto errorString = GetAzureCSharedErrorMessage(result);

        response->m_errorHandler->HandleSendResult(method, &endpoint, (int32_t)result, innerErrorCode, errorString.c_str());
        return responsePtr;
    }

    std::unique_ptr<ISpxHttpResponse> CSpxHttpRequest::SendRequestStreamResponse(
        HttpMethod method, const IHttpEndpointInfo& endpoint, StreamedResponseDataHandler&& onDataCallback, const uint8_t* content, size_t contentSize, const ISpxHttpErrorHandler::ConstPtr& errorHandler)
    {
        auto responsePtr = CreateAndConfigureRequest(endpoint, contentSize, errorHandler);
        auto response = static_cast<CSpxHttpResponse*>(responsePtr.get());
        response->m_requestMethod = method;

        std::string httpPath = endpoint.Path() + endpoint.QueryString();

        response->m_chunkCallback = std::move(onDataCallback);
        response->m_callbackExPtr = nullptr;

        int32_t result = response->m_httpAdapter->ExecuteRequestWithStreaming(
            ToRequestType(method),
            httpPath.c_str(),
            m_requestHeaders,
            (unsigned char*)content,
            contentSize,
            &response->m_statusCode,
            response->m_reasonPhrase,
            CSpxHttpResponse::REASON_PHRASE_MAX_LENGTH,
            response->m_responseHeaders,
            [](void* context, const unsigned char* buffer, size_t size)
            {
                auto response = static_cast<CSpxHttpResponse*>(context);
                try
                {
                    if (response)
                    {
                        response->m_chunkCallback(reinterpret_cast<const uint8_t*>(buffer), size);
                    }
                }
                catch (const std::exception& ex)
                {
                    SPX_TRACE_ERROR("Exception in HTTP response chunk received callback. Details: %s", ex.what());

                    if (!response->m_callbackExPtr)
                    {
                        response->m_callbackExPtr = std::current_exception();
                    }
                    else
                    {
                        SPX_TRACE_WARNING("There was another unhandled exception in HTTP response chunk received callback. Current exception will be ignored");
                    }
                }
                catch (...)
                {
                    SPX_TRACE_ERROR("Error in chunk received callback");

                    if (!response->m_callbackExPtr)
                    {
                        response->m_callbackExPtr = std::make_exception_ptr(ExceptionWithCallStack("Unknown error in HTTP response chunk received callback"));
                    }
                    else
                    {
                        SPX_TRACE_WARNING("There was another unhandled exception in HTTP response chunk received callback. Current exception will be ignored");
                    }
                }
            },
            response);

        int innerErrorCode;
        if (response->m_httpAdapter->GetLastError(&innerErrorCode) != HTTPAPI_OK)
        {
            innerErrorCode = 0;
        }

        auto errorString = GetAzureCSharedErrorMessage(result);

        response->m_errorHandler->HandleSendResult(method, &endpoint, (int32_t)result, innerErrorCode, errorString.c_str());

        if (response->m_callbackExPtr)
        {
            std::rethrow_exception(response->m_callbackExPtr);
        }

        return responsePtr;
    }

    std::unique_ptr<ISpxHttpResponse> CSpxHttpRequest::CreateAndConfigureRequest(const IHttpEndpointInfo& endpoint, const size_t contentSize, const ISpxHttpErrorHandler::ConstPtr& errorHandler)
    {
        ClearHeaders();

        auto response = std::make_unique<CSpxHttpResponse>();
        response->m_request = endpoint.Clone();
        response->m_errorHandler = errorHandler ? errorHandler : GetDefaultHttpErrorHandler();

        const auto host = endpoint.Host();
        const auto port = endpoint.Port();
        const auto proxy = endpoint.Proxy();

        m_requestHeaders = HTTPHeaders_Alloc();
        SPX_THROW_HR_IF(SPXERR_OUT_OF_MEMORY, m_requestHeaders == nullptr);

        AddHeader(m_requestHeaders, "Host", endpoint.Host().c_str());

        for (const auto& h : endpoint.Headers())
        {
            AddHeader(m_requestHeaders, h.first.c_str(), h.second.c_str());
        }

        auto contentLengthString = std::to_string(contentSize);
        AddHeader(m_requestHeaders, "Content-Length", contentLengthString.c_str());

        // Create the native HTTP instance
        int32_t result = response->m_httpAdapter->OpenHttpConnection(
            host.c_str(),
            port,
            endpoint.IsSecure(),
            proxy.host.empty() ? nullptr : proxy.host.c_str(),
            proxy.port,
            proxy.username.empty() ? nullptr : proxy.username.c_str(),
            proxy.password.empty() ? nullptr : proxy.password.c_str());

        if (result != HTTPAPI_OK)
        {
            SPX_TRACE_ERROR("Failed to open HTTP connection: %d", result);
            ThrowRuntimeError("Creating the HTTP request failed");
        }

#ifdef SPEECHSDK_USE_OPENSSL
        if (endpoint.IsSecure())
        {
            int tls_version = OPTION_TLS_VERSION_1_2;
            if (response->m_httpAdapter->SetOption(OPTION_TLS_VERSION, &tls_version) != HTTPAPI_OK)
            {
                ThrowRuntimeError("Could not set TLS 1.2 option");
            }

            bool disableDefaultVerifyPaths = endpoint.DisableDefaultVerifyPaths();
            bool disableCrlChecks = endpoint.DisableCrlChecks();
            bool continueOnCrlDownloadFailure = endpoint.ContinueOnCrlDownloadFailure();
            std::string singleCert = endpoint.SingleTrustedCertificate();
            int maxCrlDownloadSize = endpoint.MaxCRLDownloadSizeOption();

            response->m_httpAdapter->SetOption(OPTION_DISABLE_DEFAULT_VERIFY_PATHS, &disableDefaultVerifyPaths);
            if (!singleCert.empty())
            {
                response->m_httpAdapter->SetOption(OPTION_TRUSTED_CERT, singleCert.c_str());
            }

            if (disableCrlChecks)
            {
                response->m_httpAdapter->SetOption(OPTION_DISABLE_CRL_CHECK, &disableCrlChecks);
            }

            if (continueOnCrlDownloadFailure)
            {
                response->m_httpAdapter->SetOption(OPTION_CONTINUE_ON_CRL_DOWNLOAD_FAILURE, &continueOnCrlDownloadFailure);
            }

            if (maxCrlDownloadSize > 0)
            {
                response->m_httpAdapter->SetOption(OPTION_SSL_CRL_MAX_SIZE_IN_KB, &maxCrlDownloadSize);
            }
        }
#endif

        return response;
    }

    void CSpxHttpRequest::ClearHeaders()
    {
        if (m_requestHeaders)
        {
            HTTPHeaders_Free(m_requestHeaders);
            m_requestHeaders = nullptr;
        }
    }

    std::function<void()> CSpxHttpRequest::Init()
    {
        auto platformInstance = HttpPlatformImpl::Instance();
        return platformInstance->Init();
    }

} } } }
