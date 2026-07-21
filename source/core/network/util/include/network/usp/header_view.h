//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include <chrono>
#include <map>
#include <string>

#include "util/maybe.h"
#include "network/usp/message.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {
namespace USP {

class USPHeaderView
{
private:
    const Headers& m_headers;

public:
    explicit USPHeaderView(const USP::Headers& headers);

    Maybe<std::string> Path() const;
    Maybe<std::string> RequestId() const;
    Maybe<std::chrono::system_clock::time_point> Timestamp() const;
    Maybe<std::string> UtcTimestampString() const;
    Maybe<std::string> ContentType() const;

    Maybe<int32_t> StreamId() const;
    Maybe<uint64_t> PresentationTimeStamp() const;

    Maybe<std::string> GetHeader(const char * name) const;
    Maybe<std::string> GetHeader(const std::string& name) const;

private:
    USPHeaderView(const USPHeaderView&) = delete;
    USPHeaderView(USPHeaderView&&) = delete;

    USPHeaderView& operator =(const USPHeaderView&) = delete;
    USPHeaderView& operator =(USPHeaderView&&) = delete;
};

} } } } }
