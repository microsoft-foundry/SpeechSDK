//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include <chrono>
#include <string>

#include "util/buffer.h"
#include "network/usp/message.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {
namespace USP {

class USPMessageUtils
{
public:
    static size_t Serialize(const Message& message, uint8_t * buffer, size_t size);
    static SharedBufferView<uint8_t> Serialize(const Message& message);

    static Message Deserialize(const uint8_t * buffer, size_t size, bool binary);
    static Message Deserialize(const SharedBufferView<uint8_t>& buffer, bool binary);
};

} } } } }
