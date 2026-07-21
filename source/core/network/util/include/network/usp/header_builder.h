//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include <chrono>
#include <map>
#include <string>

#include "network/usp/message.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {
namespace USP {

class USPHeaderBuilder
{
public:
    explicit USPHeaderBuilder(std::string path);

    USPHeaderBuilder& RequestId(std::string requestId);
    USPHeaderBuilder& Timestamp(const std::chrono::system_clock::time_point& timestamp);
    USPHeaderBuilder& ContentType(std::string contentType);

    USPHeaderBuilder& StreamId(int32_t streamId);
    USPHeaderBuilder& PresentationTimeStamp(uint64_t pts);

    USPHeaderBuilder& AddHeader(const char * name, std::string value);
    USPHeaderBuilder& AddHeader(const std::string& name, std::string value);

    Headers Build() &;
private:
    Headers m_headers;
};

} } } } }
