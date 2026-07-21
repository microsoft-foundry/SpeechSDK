//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include "azure_c_shared_utility_httpapi_wrapper.h"
#include "interfaces/ispx_http_response.h"
#include "interfaces/ispx_http_error_handler.h"
#include "interface_helpers.h"

namespace HttpAdapter
{
    class IHttpAdapter;
}

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

    class CSpxHttpRequest;
    class CSpxBindingBasedHttpRequest;

    /// <summary>
    /// The HTTP response to an HTTP request
    /// </summary>
    class CSpxHttpResponse : public ISpxHttpResponse
    {
    private:
        friend class CSpxHttpRequest;
        friend class CSpxBindingBasedHttpRequest;

        static const size_t REASON_PHRASE_MAX_LENGTH = 1024;

        std::shared_ptr<HttpAdapter::IHttpAdapter> m_httpAdapter;
        unsigned int m_statusCode;
        HTTP_HEADERS_HANDLE m_responseHeaders;
        BUFFER_HANDLE m_buffer;
        char m_reasonPhrase[REASON_PHRASE_MAX_LENGTH];
        ISpxHttpRequest::StreamedResponseDataHandler m_chunkCallback;
        std::exception_ptr m_callbackExPtr;
        HttpMethod m_requestMethod;
        std::shared_ptr<IHttpEndpointInfo> m_request;
        ISpxHttpErrorHandler::ConstPtr m_errorHandler;

    public:
        /// <summary>
        /// Creates a new instance
        /// </summary>
        CSpxHttpResponse();

        /// <summary>
        /// Destructor
        /// </summary>
        ~CSpxHttpResponse();

        SPX_INTERFACE_MAP_BEGIN()
            SPX_INTERFACE_MAP_ENTRY(ISpxHttpResponse);
        SPX_INTERFACE_MAP_END()

        /// <summary>
        /// Determines whether or not the HTTP response was successful
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
        virtual std::string ReadContentAsString(const size_t maxLength = (std::numeric_limits<size_t>::max)()) const override;

        /// <summary>
        /// Get a pointer to the underlying HTTP response buffer
        /// </summary>
        /// <remarks>
        /// The caller should copy the content of this buffer if it needs it beyond the
        /// lifetime of the HttpResponse object. The caller should not attempt to free this buffer.
        /// </remarks>
        /// <param name="bufferLength">On return, will hold the length in bytes of the buffer</param>
        /// <returns>A pointer to the internal response buffer</returns>
        virtual unsigned char* GetUnderlyingResponseBuffer(size_t* bufferLength) const override;

    private:
        void Term();
    };

}}}} // Microsoft::CognitiveServices::Speech::Impl
