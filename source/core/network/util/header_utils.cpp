//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include <array>

#include "stdafx.h"
#include "header_names.h"
#include "network/usp/header_utils.h"
#include "crt_abstractions.h"
#include "string_utils.h"

using namespace Microsoft::CognitiveServices::Speech::Impl::USP;

size_t SerializeHeader(const std::string& name, const std::string& value, uint8_t * buffer, size_t size)
{
    auto requiredSize = name.size() + value.size() + 3;
    if (buffer == nullptr)
    {
        return requiredSize;
    }

    // PAL::sprintf_s always null terminates the string. However we do not null terminate in the
    // serialised USP format even for strings. In the valid case where we have no data and only
    // headers, this would require us to over allocate by 1 to ensure we don't write beyond where
    // we are supposed to. To work around this, only use PAL::sprintf_s to write the \r and then
    // manually overwrite the \0 with \n
    if (size < requiredSize)
    {
        AZAC_THROW_HR(AZAC_ERR_BUFFER_TOO_SMALL);
    }

    size_t written = PAL::sprintf_s(
        reinterpret_cast<char *>(buffer),
        size,
        "%s:%s\r",
        name.c_str(), value.c_str());

    buffer[written] = '\n';
    written++;
    return written;
}

size_t USPHeaderUtils::Serialize(const Headers& headers, uint8_t * buffer, size_t size)
{
    constexpr std::array<const char *, 4> standardHeaders{
        Header::Path,
        Header::Timestamp,
        Header::ContentType,
        Header::RequestId
    };

    size_t offset{ 0 };
    auto applyOffset = [buffer](size_t offset) { return buffer == nullptr ? nullptr : buffer + offset; };
    for (auto& name : standardHeaders)
    {
        auto it = headers.find(name);
        if (it != headers.end())
        {
            offset += SerializeHeader(it->first, it->second, applyOffset(offset), size - offset);
        }
    }
    for (auto& entry : headers)
    {
        auto it = std::find_if(standardHeaders.begin(), standardHeaders.end(), [&](auto&& name) { return entry.first == name; });
        if (it == standardHeaders.end())
        {
            offset += SerializeHeader(entry.first, entry.second, applyOffset(offset), size - offset);
        }
    }
    return offset;
}

template<typename TMap>
size_t DeserializeHeaders(const uint8_t* buffer, size_t size, TMap& headers)
{
    size_t ns, vs, offset;
    bool isDone;

    isDone = false;
    std::string name;
    std::string value;

    for (ns = offset = vs = 0; offset < size && !isDone; offset++)
    {
        switch (buffer[offset])
        {
        case ' ':
            break;
        case ':':
            // we only care about the first ':' separator, everything else is part of the value.
            if (name.empty())
            {
                name = std::string(reinterpret_cast<const char*>(buffer + ns), offset - ns);
                vs = offset + 1;
            }
            break;
        case '\r':
            if (!name.empty())
            {
                value = std::string(reinterpret_cast<const char*>(buffer + vs), offset - vs);
                headers.emplace(PAL::StringUtils::TrimEnd(name), PAL::StringUtils::TrimStart(value));

                name.clear();
                value.clear();
            }
            else
            {
                isDone = true;
            }
            break;
        case '\n':
            vs = 0;
            ns = offset + 1;
            break;
        }
    }

    // skip the trailing '\n'
    if (isDone)
    {
        offset++;
    }

    return offset;
}

Headers USPHeaderUtils::Deserialize(const uint8_t* buffer, size_t size)
{
    Headers headers;
    DeserializeHeaders(buffer, size, headers);
    return headers;
}

size_t Microsoft::CognitiveServices::Speech::Impl::USP::USPHeaderUtils::Deserialize(const uint8_t* buffer, size_t size, std::map<std::string, std::string>& headers)
{
    return DeserializeHeaders(buffer, size, headers);
}
