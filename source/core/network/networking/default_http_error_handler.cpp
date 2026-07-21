//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"
#include <vector>
#include <mutex>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <interfaces/enum_helpers.h>
#include <string_utils.h>
#include <http_method_helpers.h>
#include "default_http_error_handler.h"
#include "http_platform.h"
#include "http_exception.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

    /// <summary>
    /// List of well known headers in the request that we want to include in the error output
    /// </summary>
    static const std::vector<std::string> WELL_KNOWN_REQUEST_HEADERS =
    {
        "X-ConnectionId",
        "Upgrade"
    };

    /// <summary>
    /// List of well known headers in the request that we want to include in the error output
    /// </summary>
    static const std::vector<std::string> WELL_KNOWN_RESPONSE_HEADERS =
    {
        "apim-request-id",  // For some cognitive services endpoints
        "X-MSEdge-Ref",     // Anything Bing related will include this
        "X-RequestId",      // For the conversation translator endpoint
        "Content-Type"
    };

    /// <summary>
    /// List of supported content types. Must all be lowercase
    /// </summary>
    static const std::vector<std::string> SUPPORTED_CONTENT_TYPES
    {
        "application/json",
        "application/xml",
        "text/html",
        "text/xml",
        "application/xhtml+xml",
        "text/plain",
    };

    /// <summary>
    /// The maximum number of bytes to read from the response
    /// </summary>
    constexpr size_t MAX_RESPONSE_BYTES_TO_READ = 4096;

    static std::once_flag initOnce;

    ISpxHttpErrorHandler::Ptr GetDefaultHttpErrorHandler()
    {
        static ISpxHttpErrorHandler::Ptr instance;
        std::call_once(initOnce, []()
        {
            instance = std::make_shared<DefaultHttpErrorHandler>();
        });

        return instance;
    }

    bool DefaultHttpErrorHandler::IsSuccess(const ISpxHttpResponse* response) const
    {
        if (response == nullptr)
        {
            return false;
        }

        auto statusCode = response->GetStatusCode();
        return statusCode >= 200 && statusCode < 300;
    }

    void DefaultHttpErrorHandler::HandleSendResult(HttpMethod method, const IHttpEndpointInfo* request, int32_t errorCode, int32_t additionalErrorCode, const char *errorText) const
    {
        if (errorCode == HttpException::RESULT_NO_ERROR)
        {
            return;
        }

        {
            std::ostringstream oss;
            oss << errorText
                << " [0x" << std::hex << errorCode << std::dec;

            if (additionalErrorCode)
            {
                oss << " | " << std::dec << additionalErrorCode;
            }

            oss << "]";

            std::string errorMsg = GenerateSendErrorMessage(method, request, oss.str());
            SPX_TRACE_ERROR("%s", errorMsg.c_str());

            throw HttpException(errorCode, std::move(errorMsg));
        }
    }

    void DefaultHttpErrorHandler::HandleSendError(HttpMethod method, const IHttpEndpointInfo* request, const std::string& error) const
    {
        if (error.empty())
        {
            return;
        }

        {
            std::string errorMsg = GenerateSendErrorMessage(method, request, error);
            SPX_TRACE_ERROR("%s", errorMsg.c_str());

            throw HttpException(-1, std::move(errorMsg));
        }
    }

    std::string DefaultHttpErrorHandler::GenerateSendErrorMessage(HttpMethod method, const IHttpEndpointInfo* request, const std::string& error) const
    {
        std::ostringstream oss;
        oss << "Failed with error: "
            << error;

        if (request)
        {
            oss << std::endl;
            if (request->Scheme() == UriScheme::HTTPS || request->Scheme() == UriScheme::HTTP)
            {
                oss << EnumHelpers::ToString(method) << " ";
            }

            oss << request->EndpointUrl();

            const auto& requestHeaders = request->Headers();

            for (const auto& header : WELL_KNOWN_REQUEST_HEADERS)
            {
                auto found = requestHeaders.find(header);
                if (found != requestHeaders.end())
                {
                    oss << std::endl
                        << found->first << ": " << found->second;
                }
            }
        }

        return oss.str();
    }

    void DefaultHttpErrorHandler::HandleResponse(HttpMethod method, const IHttpEndpointInfo* request, const ISpxHttpResponse* response) const
    {
        if (IsSuccess(response))
        {
            return;
        }

        unsigned int statusCode = 0;
        if (response)
        {
            statusCode = response->GetStatusCode();
        }

        {
            std::string errorMessage = GenerateResponseErrorMessage(method, request, response);
            SPX_TRACE_ERROR("%s", errorMessage.c_str());

            throw HttpException(static_cast<HttpStatusCode>(statusCode), std::move(errorMessage));
        }
    }

    std::string DefaultHttpErrorHandler::GenerateResponseErrorMessage(HttpMethod method, const IHttpEndpointInfo* request, const ISpxHttpResponse* response) const
    {
        if (IsSuccess(response))
        {
            return {};
        }
        else if (response == nullptr)
        {
            return "Null response";
        }

        std::ostringstream oss;
        oss << "Failed with HTTP " << response->GetStatusCode() << " " << response->GetReasonPhrase() << std::endl;

        if (request)
        {
            if (request->Scheme() == UriScheme::HTTPS || request->Scheme() == UriScheme::HTTP)
            {
                oss << EnumHelpers::ToString(method) << " ";
            }

            oss << request->EndpointUrl();

            const auto& requestHeaders = request->Headers();
            for (const auto& header : WELL_KNOWN_REQUEST_HEADERS)
            {
                auto found = requestHeaders.find(header);
                if (found != requestHeaders.end())
                {
                    oss << std::endl
                        << found->first << ": " << found->second;
                }
            }
        }

        for (const auto& header : WELL_KNOWN_RESPONSE_HEADERS)
        {
            auto value = PAL::StringUtils::Trim(response->GetHeader(header));
            if (!value.empty())
            {
                oss << std::endl
                    << header << ": " << value;
            }
        }

        auto contentType = PAL::StringUtils::Trim(
            PAL::StringUtils::ToLower(
                response->GetHeader("Content-Type")));

        if (!contentType.empty())
        {
            bool supportedContentType = std::any_of(
                SUPPORTED_CONTENT_TYPES.begin(),
                SUPPORTED_CONTENT_TYPES.end(),
                [&contentType](const std::string& supported)
            {
                return contentType.find_first_of(supported, 0) != std::string::npos;
            });

            if (supportedContentType)
            {
                std::string body = response->ReadContentAsString(MAX_RESPONSE_BYTES_TO_READ);
                if (!body.empty())
                {
                    oss << std::endl << body;
                }
            }
        }

        return oss.str();
    }

}}}}
