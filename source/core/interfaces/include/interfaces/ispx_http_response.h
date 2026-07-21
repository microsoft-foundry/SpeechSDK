//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include <string>
#include <interfaces/base.h>
#include <limits>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

    /// <summary>
    /// The interface for sending HTTP requests
    /// </summary>
    SPX_INTERFACE(ISpxHttpResponse)
    {
    public:
        /// <summary>
        /// Determines whether or not the HTTP response was successful. By default 2XX status codes
        /// will be treated as successful, all other status codes are errors
        /// </summary>
        /// <returns>True on success, false otherwise</returns>
        virtual bool IsSuccess() const = 0;

        /// <summary>
        /// Throws an exception if the HTTP response wasn't successful
        /// </summary>
        virtual void EnsureSuccess() const = 0;

        /// <summary>
        /// Gets the HTTP status code
        /// </summary>
        /// <returns>Status code e.g. 200, 404</returns>
        virtual unsigned int GetStatusCode() const = 0;

        /// <summary>
        /// Gets the HTTP reason phrase (if set)
        /// </summary>
        /// <returns>The HTTP reason phrase</returns>
        virtual std::string GetReasonPhrase() const = 0;

        /// <summary>
        /// Retrieves a header value from the response
        /// </summary>
        /// <param name="name">The name of the header</param>
        /// <returns>The header value. If that header was not present, an empty string will be returned</returns>
        virtual std::string GetHeader(const std::string & name) const = 0;

        /// <summary>
        /// Reads the content of the HTTP response as a string
        /// </summary>
        /// <param name="maxLength">The maximum number of chars to read from the response</param>
        /// <returns>The content as a string</returns>
        virtual std::string ReadContentAsString(const size_t maxLength = (std::numeric_limits<size_t>::max)()) const = 0;

        /// <summary>
        /// Get a pointer to the underlying HTTP response buffer
        /// </summary>
        /// <remarks>
        /// The caller should copy the content of this buffer if it needs it beyond the
        /// lifetime of the HttpResponse object. The caller should not attempt to free this buffer.
        /// </remarks>
        /// <param name="bufferLength">On return, will hold the length in bytes of the buffer</param>
        /// <returns>A pointer to the internal response buffer</returns>
        virtual unsigned char* GetUnderlyingResponseBuffer(size_t* bufferLength) const
        {
            // Default implementation returns nullptr
            if (bufferLength) *bufferLength = 0;
            return nullptr;
        };
    };

}}}}
