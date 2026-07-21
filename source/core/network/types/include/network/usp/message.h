//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include <map>
#include <string>

#include "util/buffer.h"
#include "util/either.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {
namespace USP {

    using Headers = std::map<std::string, std::string, std::less<>>;

    struct Message
    {
        USP::Headers Headers;
        Either<std::string, SharedBufferView<uint8_t>> Data;
    };

} } } } }
