//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include <limits>
#include "interfaces/http_method.h"
#include "interfaces/i_http_endpoint_info.h"
#include "interfaces/ispx_http_response.h"
#include "interfaces/ispx_http_error_handler.h"
#include "interface_helpers.h"
#include "azure/core/http/raw_response.hpp"
#include "azure/core/http/transport.hpp"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

    class CSpxHttpRequest;

    /// <summary>
    /// The HTTP response to an HTTP request
    /// </summary>
    class CSpxHttpResponse : public ISpxHttpResponse
    {
    private:
        std::unique_ptr<Azure::Core::Http::HttpTransport> m_transport;
        HttpMethod m_requestMethod;
        std::unique_ptr<IHttpEndpointInfo> m_request;
        std::unique_ptr<Azure::Core::Http::RawResponse> m_response;
        ISpxHttpErrorHandler::ConstPtr m_errorHandler;

    public:
        /// <summary>
        /// Creates a new instance
        /// </summary>
        CSpxHttpResponse();

        /// <summary>
        /// Creates a new instance
        /// </summary>
        /// <param name="transport">The HTTP transport in use</param>
        /// <param name="method">The HTTP method of the request</param>
        /// <param name="request">The request</param>
        /// <param name="response">The raw response</param>
        /// <param name="errorHandler">The error handler</param>
        CSpxHttpResponse(
            std::unique_ptr<Azure::Core::Http::HttpTransport> transport,
            HttpMethod method,
            const IHttpEndpointInfo& request,
            std::unique_ptr<Azure::Core::Http::RawResponse>&& response,
            const ISpxHttpErrorHandler::ConstPtr& errorHandler);

        /// <summary>
        /// Destructor
        /// </summary>
        ~CSpxHttpResponse();

        SPX_INTERFACE_MAP_BEGIN()
            SPX_INTERFACE_MAP_ENTRY(ISpxHttpResponse);
        SPX_INTERFACE_MAP_END()

        /// <summary>
        /// Determines whether or not the HTTP response was successful. By default 2XX status codes
        /// will be treated as successful, all other status codes are errors
        /// </summary>
        /// <returns>True on success, false otherwise</returns>
        virtual bool IsSuccess() const override;

        /// <summary>
        /// Throws an exception if the HTTP response wasn't successful
        /// </summary>
        virtual void EnsureSuccess() const override;

        /// <summary>
        /// Gets the HTTP status code
        /// </summary>
        /// <returns>Status code e.g. 200, 404</returns>
        virtual unsigned int GetStatusCode() const override;

        /// <summary>
        /// Gets the HTTP reason phrase (if set)
        /// </summary>
        /// <returns>The HTTP reason phrase</returns>
        virtual std::string GetReasonPhrase() const override;

        /// <summary>
        /// Retrieves a header value from the response
        /// </summary>
        /// <param name="name">The name of the header</param>
        /// <returns>The header value. If that header was not present, an empty string will be returned</returns>
        virtual std::string GetHeader(const std::string& name) const override;

        /// <summary>
        /// Reads the content of the HTTP response as a string
        /// </summary>
        /// <param name="maxLength">The maximum number of chars to read from the response</param>
        /// <returns>The content as a string</returns>
        virtual std::string ReadContentAsString(const size_t maxLength = std::numeric_limits<size_t>::max()) const override;
    };

}}}} // Microsoft::CognitiveServices::Speech::Impl
