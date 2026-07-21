//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include <interfaces/ispx_http_request.h>
#include <interfaces/ispx_http_error_handler.h>
#include <interface_helpers.h>
#include "azure_c_shared_utility_httpapi_wrapper.h"
#include "interfaces/ISpxNetworkPlatformInit.h"

namespace HttpAdapter
{
    class IHttpAdapter;
}

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

    /* TODO: In an ideal future, the HttpEndpointInfo would be folded into here in a more logical hierarchy */

    /// <summary>
    /// A wrapper around the Azure C shared library code to allow sending HTTP/HTTPS requests
    /// </summary>
    class CSpxHttpRequest :
        public ISpxHttpRequest,
        public ISpxNetworkPlatformInit

    {
    public:
        /// <summary>
        /// Creates a new instance
        /// </summary>
        CSpxHttpRequest();

        /// <summary>
        /// Destructor
        /// </summary>
        virtual ~CSpxHttpRequest();

        SPX_INTERFACE_MAP_BEGIN()
            SPX_INTERFACE_MAP_ENTRY(ISpxHttpRequest)
            SPX_INTERFACE_MAP_ENTRY(ISpxNetworkPlatformInit)
        SPX_INTERFACE_MAP_END()

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
            const uint8_t* content = nullptr,
            size_t contentSize = 0,
            const ISpxHttpErrorHandler::ConstPtr& errorHandler = nullptr) override ;

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
            const IHttpEndpointInfo& endpoint,
            StreamedResponseDataHandler&& onDataCallback,
            const uint8_t* content = nullptr,
            size_t contentSize = 0,
            const ISpxHttpErrorHandler::ConstPtr& errorHandler = nullptr) override;

        virtual std::function<void()> Init() override;

    protected:
        std::unique_ptr<ISpxHttpResponse> CreateAndConfigureRequest(const IHttpEndpointInfo& endpoint, const size_t contentSize, const ISpxHttpErrorHandler::ConstPtr& errorHandler);
        void ClearHeaders();

    private:
        std::shared_ptr<HttpAdapter::IHttpAdapter> m_httpAdapter;
        HTTP_HEADERS_HANDLE m_requestHeaders;
    };

} } } } // Microsoft::CognitiveServices::Speech::Impl
