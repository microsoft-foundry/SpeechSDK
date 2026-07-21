//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "network/usp/message_builder.h"

using namespace Microsoft::CognitiveServices::Speech::Impl::USP;


USPMessageBuilder& USPMessageBuilder::Headers(USP::Headers headers)
{
    m_headers = std::move(headers);
    return *this;
}

USPMessageBuilder& USPMessageBuilder::Payload(std::string payload)
{
    m_payload = Either<std::string, SharedBufferView<uint8_t>>{ std::move(payload) };
    return *this;
}

USPMessageBuilder& USPMessageBuilder::Payload(SharedBufferView<uint8_t> payload)
{
    m_payload = Either<std::string, SharedBufferView<uint8_t>>{ std::move(payload) };
    return *this;
}

Message USPMessageBuilder::Build() &
{
    return Message{ std::move(m_headers), std::move(m_payload) };
}
