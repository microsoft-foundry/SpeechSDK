//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include <functional>
#include <memory>
#include <interfaces/base.h>
#include <interfaces/ispx_http_response.h>
#include <interfaces/http_method.h>
#include <interfaces/i_http_endpoint_info.h>
#include <interfaces/ispx_http_error_handler.h>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

    /// <summary>
    /// The interface for sending HTTP requests
    /// </summary>
    SPX_INTERFACE(ISpxHttpRequest)
    {
    public:
        /// <summary>
        /// The function to call whenever we receive data from the service. Used for SendRequestStreamResponse
        /// </summary>
        using StreamedResponseDataHandler = std::function<void(const uint8_t* data, const size_t numBytes)>;

        /// <summary>
        /// Sends an HTTP request, optionally with a payload
        /// </summary>
        /// <param name="method">The type of HTTP request to send (e.g. POST)</param>
        /// <param name="endpoint">The endpoint to connect to</param>
        /// <param name="content">(Optional) Pointer to raw byte data to send</param>
        /// <param name="contentSize">(Optional) The number of bytes to send</param>
        /// <paran name="errorHandler">(Optional) The error handler to use to check/process send failures, or errors in the service response</param>
        /// <returns>The HTTP response</returns>
        /// <exception cref="HttpException">If the connection to the host failed, or another error was encountered. By default, exceptions
        /// are NOT thrown for non success HTTP status (e.g. 400, 500)</exception>
        virtual std::unique_ptr<ISpxHttpResponse> SendRequest(
            HttpMethod method,
            const IHttpEndpointInfo& endpoint,
            const uint8_t *content = nullptr,
            size_t contentSize = 0,
            const ISpxHttpErrorHandler::ConstPtr& errorHandler = nullptr) = 0;

        /// <summary>
        /// Sends an HTTP request, and streams the response data. The method will return once the entire response content
        /// has been read, or there is an error
        /// </summary>
        /// <param name="method">The type of HTTP request to send (e.g. POST)</param>
        /// <param name="endpoint">The endpoint to connect to</param>
        /// <param name="onDataCallback"></param>
        /// <param name="content">(Optional) Pointer to raw byte data to send</param>
        /// <param name="contentSize">(Optional) The number of bytes to send</param>
        /// <paran name="errorHandler">(Optional) The error handler to use to check/process send failures, or errors in the service response</param>
        /// <returns>The HTTP response. Returns as soon as the response headers have been received from the endpoint</returns>
        /// <exception cref="HttpException">If the connection to the host failed, or another error was encountered. By default exceptions
        /// are NOT thrown for non success HTTP status (e.g. 400, 500)</exception>
        virtual std::unique_ptr<ISpxHttpResponse> SendRequestStreamResponse(
            HttpMethod method,
            const IHttpEndpointInfo & endpoint,
            StreamedResponseDataHandler&& onDataCallback,
            const uint8_t* content = nullptr,
            size_t contentSize = 0,
            const ISpxHttpErrorHandler::ConstPtr& errorHandler = nullptr) = 0;
    };

}}}}
