//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include <interfaces/ispx_http_error_handler.h>
#include <interface_helpers.h>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

    /// <summary>
    /// Get the default error handler for HTTP/WS requests. This treats all non-zero send request error codes as
    /// failures, and all non 2XX HTTP status codes as failures. In the case of failures, an HttpException will
    /// be thrown. The HTTP exception message will include the details of the failure (where available), the HTTP
    /// method, the endpoint where the request is being sent, some useful well known headers e.g. X-ConnectionId,
    /// as well as the first 4k characters of the response from the service for supported text like content types
    /// (where available)
    /// </summary>
    /// <returns>The pointer to the singleton instance to use</returns>
    ISpxHttpErrorHandler::Ptr GetDefaultHttpErrorHandler();

    class DefaultHttpErrorHandler : public ISpxHttpErrorHandler
    {
    public:
        DefaultHttpErrorHandler() = default;
        virtual ~DefaultHttpErrorHandler() = default;
        DefaultHttpErrorHandler(const DefaultHttpErrorHandler&) = delete;
        DefaultHttpErrorHandler(DefaultHttpErrorHandler&&) = delete;

        SPX_INTERFACE_MAP_BEGIN()
            SPX_INTERFACE_MAP_ENTRY(ISpxHttpErrorHandler)
        SPX_INTERFACE_MAP_END()

        virtual bool IsSuccess(const ISpxHttpResponse* response) const override;
        virtual void HandleSendResult(HttpMethod method, const IHttpEndpointInfo* request, int32_t errorCode, int32_t additionalErrorCode, const char *errorText) const override;
        virtual void HandleSendError(HttpMethod method, const IHttpEndpointInfo* request, const std::string& error) const override;
        virtual std::string GenerateSendErrorMessage(HttpMethod method, const IHttpEndpointInfo* request, const std::string& error) const override;
        virtual void HandleResponse(HttpMethod method, const IHttpEndpointInfo* request, const ISpxHttpResponse* response) const override;
        virtual std::string GenerateResponseErrorMessage(HttpMethod method, const IHttpEndpointInfo* request, const ISpxHttpResponse* response) const override;
    };

}}}}
