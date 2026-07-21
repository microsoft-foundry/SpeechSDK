//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "network/usp/header_view.h"
#include "header_names.h"
#include "time_utils.h"

using namespace Microsoft::CognitiveServices::Speech::Impl;
using namespace Microsoft::CognitiveServices::Speech::Impl::USP;

USPHeaderView::USPHeaderView(const USP::Headers& headers) :
    m_headers(headers)
{}

Maybe<std::string> USPHeaderView::Path() const
{
    return GetHeader(USP::Header::Path);
}

Maybe<std::string> USPHeaderView::RequestId() const
{
    return GetHeader(USP::Header::RequestId);
}

Maybe<std::chrono::system_clock::time_point> USPHeaderView::Timestamp() const
{
    return GetHeader(USP::Header::Timestamp)
        .Map<std::chrono::system_clock::time_point>([](const std::string& val)
        {
            std::chrono::system_clock::time_point parsed;
            if (PAL::TryParseUtcTimestamp(val, parsed))
            {
                return Maybe<std::chrono::system_clock::time_point>(parsed);
            }
            else
            {
                return Maybe<std::chrono::system_clock::time_point>{};
            }
        });
}

Maybe<std::string> USPHeaderView::UtcTimestampString() const
{
    return GetHeader(USP::Header::Timestamp);
}

Maybe<std::string> USPHeaderView::ContentType() const
{
    return GetHeader(USP::Header::ContentType);
}

Maybe<int32_t> USPHeaderView::StreamId() const
{
    return GetHeader(USP::Header::StreamId)
        .Map<int32_t>([](const std::string& val)
        {
            return static_cast<int32_t>(
                std::strtol(val.c_str(), nullptr, 10));
        });
}

Maybe<uint64_t> USPHeaderView::PresentationTimeStamp() const
{
    return GetHeader(USP::Header::PTS)
        .Map<uint64_t>([](const std::string& val)
        {
            return static_cast<uint64_t>(
                std::strtoull(val.c_str(), nullptr, 10));
        });
}

Maybe<std::string> USPHeaderView::GetHeader(const char* name) const
{
    return GetHeader(std::string{ name });
}

Maybe<std::string> USPHeaderView::GetHeader(const std::string& name) const
{
    auto iter = m_headers.find(name);
    if (iter == m_headers.end())
    {
        return Maybe<std::string>{};
    }

    return Maybe<std::string>(iter->second);
}
