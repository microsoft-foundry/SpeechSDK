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

class USPMessageBuilder
{
public:
    USPMessageBuilder() = default;

    USPMessageBuilder& Headers(Headers headers);
    USPMessageBuilder& Payload(std::string payload);
    USPMessageBuilder& Payload(SharedBufferView<uint8_t> payload);

    Message Build() &;
private:
    Either<std::string, SharedBufferView<uint8_t>> m_payload;
    USP::Headers m_headers;
};

} } } } }
