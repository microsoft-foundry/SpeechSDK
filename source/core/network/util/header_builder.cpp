//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "network/usp/header_builder.h"
#include "header_names.h"
#include "time_utils.h"

using namespace Microsoft::CognitiveServices::Speech::Impl::USP;

USPHeaderBuilder::USPHeaderBuilder(std::string path)
{
    m_headers.insert({ Header::Path, std::move(path) });
    Timestamp(std::chrono::system_clock::now());
}

template<typename M, typename T>
void InsertOrAssign(M& map, T name, std::string&& value)
{
    auto it = map.find(name);
    if (it == map.end())
    {
        map.insert({ name, std::move(value) });
    }
    else
    {
        map[name] = std::move(value);
    }
}

USPHeaderBuilder& USPHeaderBuilder::RequestId(std::string requestId)
{
    InsertOrAssign(m_headers, Header::RequestId, std::move(requestId));
    return *this;
}

USPHeaderBuilder& USPHeaderBuilder::Timestamp(const std::chrono::system_clock::time_point& timestamp)
{
    InsertOrAssign(m_headers, Header::Timestamp, PAL::GetTimeInString(timestamp, 3));
    return *this;
}

USPHeaderBuilder& USPHeaderBuilder::ContentType(std::string contentType)
{
    InsertOrAssign(m_headers, Header::ContentType, std::move(contentType));
    return *this;
}

USPHeaderBuilder& USPHeaderBuilder::StreamId(int32_t streamId)
{
    InsertOrAssign(m_headers, Header::StreamId, std::to_string(streamId));
    return *this;
}

USPHeaderBuilder& USPHeaderBuilder::PresentationTimeStamp(uint64_t pts)
{
    InsertOrAssign(m_headers, Header::PTS, std::to_string(pts));
    return *this;
}

USPHeaderBuilder& USPHeaderBuilder::AddHeader(const char * name, std::string value)
{
    InsertOrAssign(m_headers, name, std::move(value));
    return *this;
}

USPHeaderBuilder& USPHeaderBuilder::AddHeader(const std::string& name, std::string value)
{
    InsertOrAssign(m_headers, name, std::move(value));
    return *this;
}

Headers USPHeaderBuilder::Build() &
{
    return std::move(m_headers);
}
