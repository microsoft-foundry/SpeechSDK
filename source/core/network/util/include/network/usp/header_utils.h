//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include <map>
#include <string>

#include "util/maybe.h"
#include "network/usp/message.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {
namespace USP {

class USPHeaderUtils
{
public:
    static size_t Serialize(const Headers& headers, uint8_t * buffer, size_t size);
    static Headers Deserialize(const uint8_t * buffer, size_t size);
    static size_t Deserialize(const uint8_t* buffer, size_t size, std::map<std::string, std::string>& headers);

    template<typename T>
    static Maybe<std::reference_wrapper<const std::string>> Get(const Headers& headers, T&& headerName)
    {
        auto it = headers.find(headerName);
        if (it != headers.end())
        {
            return std::reference_wrapper<const std::string>{ it->second };
        }
        return nullptr;
    }
};

} } } } }
