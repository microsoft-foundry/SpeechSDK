//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"
#include <sstream>
#include "azure_c_shared_utility_urlencode_wrapper.h"
#include "azure_c_shared_utility_httpapi_wrapper.h"
#include "default_http_error_handler.h"
#include "http_exception.h"
#include "http_request.h"
#include "http_response.h"
#include "i_http_adapter.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

    CSpxHttpResponse::CSpxHttpResponse() :
        m_httpAdapter(nullptr),
        m_statusCode(0),
        m_responseHeaders(HTTPHeaders_Alloc()),
        m_buffer(BUFFER_new()),
        m_reasonPhrase(),
        m_chunkCallback(),
        m_callbackExPtr(nullptr),
        m_requestMethod(HttpMethod::Get),
        m_request(),
        m_errorHandler()
    {
        m_reasonPhrase[0] = '\0';

        if (!m_buffer || !m_responseHeaders)
        {
            Term();
            throw std::bad_alloc();
        }

        m_httpAdapter = GetHttpAdapter();
        m_httpAdapter->Initialize();
    }

    CSpxHttpResponse::~CSpxHttpResponse()
    {
        Term();
    }

    void CSpxHttpResponse::Term()
    {
        if (m_buffer)
        {
            BUFFER_delete(m_buffer);
            m_buffer = nullptr;
        }

        if (m_responseHeaders)
        {
            HTTPHeaders_Free(m_responseHeaders);
            m_responseHeaders = nullptr;
        }

        if (m_httpAdapter)
        {
            try
            {
                m_httpAdapter->CloseHttpConnection();
            }
            catch (std::exception&)
            {
            }
            try
            {
                m_httpAdapter->Uninitialize();
            }
            catch (std::exception&)
            {
            }
            m_httpAdapter = nullptr;
        }
    }

    bool CSpxHttpResponse::IsSuccess() const
    {
        return m_errorHandler && m_errorHandler->IsSuccess(this);
    }

    void CSpxHttpResponse::EnsureSuccess() const
    {
        if (m_errorHandler)
        {
            m_errorHandler->HandleResponse(m_requestMethod, m_request.get(), this);
        }
        else
        {
            ThrowRuntimeError("No HTTP error handler set for the CSpxHttpResponse");
        }
    }

    unsigned int CSpxHttpResponse::GetStatusCode() const
    {
        return m_statusCode;
    }

    std::string CSpxHttpResponse::GetReasonPhrase() const
    {
        return std::string(m_reasonPhrase);
    }

    std::string CSpxHttpResponse::GetHeader(const std::string & key) const
    {
        const char* headerValue = HTTPHeaders_FindHeaderValue(m_responseHeaders, key.data());
        return headerValue == nullptr ? std::string() : std::string(headerValue);
    }

    std::string CSpxHttpResponse::ReadContentAsString(const size_t maxLength) const
    {
        unsigned char *ptr = BUFFER_u_char(m_buffer);
        if (ptr == nullptr)
        {
            return std::string();
        }

        size_t size = BUFFER_length(m_buffer);
        if (size == 0)
        {
            return std::string{};
        }

        // NOTE: This may break UTF8 character sequences when arbitrarily truncating!
        size_t numToRead = (std::min)(maxLength, size);
        return std::string((const char*)ptr, numToRead);
    }

    unsigned char* CSpxHttpResponse::GetUnderlyingResponseBuffer(size_t* bufferLength) const
    {
        if (bufferLength == nullptr)
        {
            return nullptr;
        }

        *bufferLength = 0;

        size_t size = BUFFER_length(m_buffer);

        if (size == 0)
        {
            return nullptr;
        }

        unsigned char* ptr = BUFFER_u_char(m_buffer);
        if (ptr == nullptr)
        {
            return nullptr;
        }

        *bufferLength = size;
        return ptr;
    }

} } } }
