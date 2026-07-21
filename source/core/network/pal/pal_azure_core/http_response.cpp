//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"
#include <sstream>
#include "http_response.h"
#include "http_endpoint_info.h"
#include "default_http_error_handler.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

    using namespace Azure::Core;

    CSpxHttpResponse::CSpxHttpResponse() :
        CSpxHttpResponse(nullptr, HttpMethod::Get, HttpEndpointInfo(), nullptr, nullptr)
    {
    }

    CSpxHttpResponse::CSpxHttpResponse(
        std::unique_ptr<Http::HttpTransport> transport,
        HttpMethod method,
        const IHttpEndpointInfo& request,
        std::unique_ptr<Http::RawResponse>&& response,
        const ISpxHttpErrorHandler::ConstPtr& errorHandler
    ) :
        m_transport(std::move(transport)),
        m_requestMethod(method),
        m_request(request.Clone()),
        m_response(std::move(response)),
        m_errorHandler(errorHandler ? errorHandler : GetDefaultHttpErrorHandler())
    {
    }

    CSpxHttpResponse::~CSpxHttpResponse()
    {
    }

    bool CSpxHttpResponse::IsSuccess() const
    {
        return m_response && m_errorHandler->IsSuccess(this);
    }

    void CSpxHttpResponse::EnsureSuccess() const
    {
        m_errorHandler->HandleResponse(m_requestMethod, m_request.get(), this);
    }

    unsigned int CSpxHttpResponse::GetStatusCode() const
    {
        return m_response
            ? static_cast<int>(m_response->GetStatusCode())
            : 0;
    }

    std::string CSpxHttpResponse::GetReasonPhrase() const
    {
        return m_response
            ? m_response->GetReasonPhrase()
            : std::string{};
    }

    std::string CSpxHttpResponse::GetHeader(const std::string& key) const
    {
        if (m_response == nullptr)
        {
            return {};
        }

        const auto& headers = m_response->GetHeaders();
        auto found = headers.find(key);
        return found == headers.end()
            ? std::string{}
            : found->second;
    }

    static std::string ReadChunkedResponse(IO::BodyStream* stream, size_t maxLength, const Azure::Core::Context& context)
    {
        constexpr size_t bufferSize = 2048;
        uint8_t buffer[bufferSize];

        size_t totalBytes = 0;
        std::ostringstream oss;

        while (totalBytes < maxLength)
        {
            size_t size = std::min(bufferSize, maxLength - totalBytes);
            size = stream->ReadToCount(buffer, size, context);
            totalBytes += size;
            if (size == 0)
            {
                break;
            }

            oss.write(reinterpret_cast<const char*>(buffer), size);
        }
        
        return oss.str();
    }

    std::string CSpxHttpResponse::ReadContentAsString(const size_t maxLength) const
    {
        if (m_response == nullptr)
        {
            return {};
        }

        auto context = Azure::Core::Context();

        auto bodyStream = m_response->ExtractBodyStream();
        if (bodyStream == nullptr)
        {
            return "";
        }
        else if (bodyStream->Length() == 0)
        {
            return {};
        }
        else if (bodyStream->Length() < 0)
        {
            // probably chunked response
            return ReadChunkedResponse(bodyStream.get(), maxLength, context);
        }
        else
        {
            size_t size = static_cast<size_t>(
                std::min(
                    static_cast<uint64_t>(bodyStream->Length()),
                    static_cast<uint64_t>(maxLength)));

            std::string str(size, '\0');
            size = bodyStream->ReadToCount(reinterpret_cast<uint8_t*>(&str[0]), str.length());

            str.resize(size);
            return str;
        }
    }

} } } }
