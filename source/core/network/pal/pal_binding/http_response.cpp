//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"
#include <sstream>
#include "http_exception.h"
#include "http_request.h"
#include "http_response.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

CSpxBindingBasedHttpResponse::CSpxBindingBasedHttpResponse() :
    m_responseHeaders(),
    m_statusCode(0),
    m_reasonPhrase(""),
    m_dataAvailableCallback(nullptr)
{
}

CSpxBindingBasedHttpResponse::~CSpxBindingBasedHttpResponse()
{
    Term();
}

void CSpxBindingBasedHttpResponse::Term()
{
}

bool CSpxBindingBasedHttpResponse::IsSuccess() const
{
    return m_statusCode >= 200 && m_statusCode < 300;
}

void CSpxBindingBasedHttpResponse::EnsureSuccess() const
{
    if (!IsSuccess())
    {
        throw HttpException(m_statusCode);
    }
}

unsigned int CSpxBindingBasedHttpResponse::GetStatusCode() const
{
    return m_statusCode;
}

std::string CSpxBindingBasedHttpResponse::GetReasonPhrase() const
{
    return m_reasonPhrase;
}

std::string CSpxBindingBasedHttpResponse::GetHeader(const std::string& key) const
{
    auto headerValue = m_responseHeaders.find(key);

    return headerValue == m_responseHeaders.end() ? std::string() : headerValue->second;
}

std::string CSpxBindingBasedHttpResponse::ReadContentAsString(const size_t maxLength) const
{
    UNUSED(maxLength);

    if (m_buffer.kind == VariantKind::Empty)
    {
        return std::string();
    }

    return std::string((char*)(m_buffer.data.get()), m_buffer.dataSize);
}

unsigned char* CSpxBindingBasedHttpResponse::GetUnderlyingResponseBuffer(size_t* bufferLength) const
{
    if (bufferLength == nullptr)
    {
        return nullptr;
    }

    *bufferLength = 0;

    size_t size = m_buffer.dataSize;

    if (size == 0)
    {
        return nullptr;
    }

    unsigned char* ptr = m_buffer.data.get();
    if (ptr == nullptr)
    {
        return nullptr;
    }

    *bufferLength = size;
    return ptr;
}


void CSpxBindingBasedHttpResponse::SignalOnDataCallback(const uint8_t* data, size_t size)
{
    if (data == nullptr || size == 0)
    {
        return;
    }
    
    // If the response has a callback registered, invoke it with the data
    if (m_dataAvailableCallback)
    {
        m_dataAvailableCallback(data, size);
    }
}

}
}
}
}

