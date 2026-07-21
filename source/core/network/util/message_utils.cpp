//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include <array>
#include <string>

#include "stdafx.h"
#include "network/usp/header_utils.h"
#include "network/usp/message_utils.h"
#include "crt_abstractions.h"

using namespace Microsoft::CognitiveServices::Speech::Impl;
using namespace Microsoft::CognitiveServices::Speech::Impl::USP;

size_t WriteData(const SharedBufferView<uint8_t>& data, uint8_t * buffer, size_t size)
{
    if (buffer != nullptr)
    {
        if (data.Size() > size)
        {
            AZAC_THROW_HR(AZAC_ERR_BUFFER_TOO_SMALL);
        }
        std::memcpy(buffer, reinterpret_cast<const void *>(data.Get()), data.Size());
    }
    return data.Size();
}

size_t WriteData(const std::string& data, uint8_t * buffer, size_t size)
{
    if (buffer != nullptr)
    {
        if (data.size() > size)
        {
            AZAC_THROW_HR(AZAC_ERR_BUFFER_TOO_SMALL);
        }
        std::memcpy(buffer, reinterpret_cast<const void *>(data.c_str()), data.size());
    }
    return data.size();
}

size_t USPMessageUtils::Serialize(const Message& message, uint8_t * buffer, size_t size)
{
    auto isBinary = !message.Data.Has<std::string>();
    auto applyOffset = [buffer](size_t offset) { return buffer == nullptr ? nullptr : buffer + offset; };
    auto sizeRemaining = [size](size_t offset) { return size < offset ? 0 : size - offset; };
    /* If the message is binary, the first 16 bits are for the frame size */
    size_t offset{ isBinary ? sizeof(uint16_t) : 0 };

    offset += USPHeaderUtils::Serialize(message.Headers, applyOffset(offset), sizeRemaining(offset));

    if (isBinary && (buffer != nullptr))
    {
        auto headerSize = static_cast<uint16_t>(offset - sizeof(uint16_t));
        buffer[0] = reinterpret_cast<uint8_t *>(&headerSize)[1];
        buffer[1] = reinterpret_cast<uint8_t *>(&headerSize)[0];
    }
    else if (!isBinary)
    {
        if (buffer != nullptr)
        {
            buffer[offset++] = '\r';
            buffer[offset++] = '\n';
        }
        else
        {
            offset += 2;
        }
    }

    offset += message.Data.Visit([&](auto&& data)
    {
        return WriteData(data, applyOffset(offset), sizeRemaining(offset));
    });

    return offset;
}

SharedBufferView<uint8_t> USPMessageUtils::Serialize(const Message& message)
{
    auto size = Serialize(message, nullptr, 0);
    auto buffer = SpxAllocSharedBuffer<uint8_t>(size);
    Serialize(message, buffer.get(), size);
    return SharedBufferView<uint8_t>{ std::move(buffer), size};
}

Message DeserializeTextMessage(const uint8_t * buffer, size_t size)
{
    auto charBuffer = reinterpret_cast<const char *>(buffer);
    auto headerTail = std::strstr(charBuffer, "\r\n\r\n");
    if (headerTail == nullptr)
    {
        AZAC_THROW_HR(AZAC_ERR_NETWORK_MALFORMED);
    }
    headerTail += 2;
    size_t headerSize = headerTail - charBuffer;
    auto payloadSize = size - headerSize - 2;
    auto headers = USPHeaderUtils::Deserialize(buffer, headerSize);
    using E = Either<std::string, SharedBufferView<uint8_t>>;
    Message message{ std::move(headers), E{ std::string{ headerTail + 2, payloadSize } } };
    return message;
}

Message DeserializeBinaryMessage(const uint8_t * buffer, size_t size)
{
    uint16_t headerSize = (buffer[0] << 8) | buffer[1];
    size_t offset{ sizeof(uint16_t) };
    auto headers = USPHeaderUtils::Deserialize(buffer + offset, headerSize);
    offset += headerSize;
    auto dataBuffer = SpxAllocSharedBuffer<uint8_t>(size - offset);
    std::memcpy(dataBuffer.get(), buffer + offset, size - offset);
    SharedBufferView<uint8_t> bufferView{ dataBuffer, size - offset };
    using E = Either<std::string, SharedBufferView<uint8_t>>;
    Message message{ std::move(headers), E{ std::move(bufferView) } };
    return message;
}

Message USPMessageUtils::Deserialize(const uint8_t * buffer, size_t size, bool binary)
{
    if (binary)
    {
        return DeserializeBinaryMessage(buffer, size);
    }
    else
    {
        return DeserializeTextMessage(buffer, size);
    }
}

Message USPMessageUtils::Deserialize(const SharedBufferView<uint8_t>& buffer, bool binary)
{
    return Deserialize(buffer.Get(), buffer.Size(), binary);
}
