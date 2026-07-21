//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once
#include <functional>
#include <interfaces/ispx_http_response.h>
#include <map>
#include <string>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

class CSpxBindingBasedHttpResponse;
using StreamedResponseDataHandler = std::function<void(const uint8_t*, size_t)>;

/// <summary>
/// The HTTP response to an HTTP request
/// </summary>
class CSpxBindingBasedHttpResponse : public ISpxHttpResponse
{
private:
    friend class CSpxHttpRequest;
    friend class CSpxBindingBasedHttpRequest;

    std::map<std::string, std::string> m_responseHeaders;
    unsigned int m_statusCode;
    VariantValue m_buffer;
    std::string m_reasonPhrase;
    StreamedResponseDataHandler m_dataAvailableCallback;


public:
    /// <summary>
    /// Creates a new instance
    /// </summary>
    CSpxBindingBasedHttpResponse();
    /// <summary>
    /// Destructor
    /// </summary>
    ~CSpxBindingBasedHttpResponse();

    SPX_INTERFACE_MAP_BEGIN()
        SPX_INTERFACE_MAP_ENTRY(ISpxHttpResponse);
    SPX_INTERFACE_MAP_END()

    /// <summary>
    /// Determines whether or not the HTTP response has a success status code (i.e. 2XX)
    /// </summary>
    /// <returns>True on success, false otherwise</returns>
    virtual bool IsSuccess() const override;

    /// <summary>
    /// Throws an exception if the HTTP response doesn't have a success status code
    /// </summary>
    /// <exception cref="HttpException">If the status code is outside of the range 200-299 (inclusive)</exception>
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
    
    /// <summary>
    /// Signal streaming data to the response
    /// </summary>
    /// <param name="data">Pointer to the data</param>
    /// <param name="size">Size of the data in bytes</param>
    void SignalOnDataCallback(const uint8_t* data, size_t size);

private:
    void Term();
  

    
};


}
}
}
}
