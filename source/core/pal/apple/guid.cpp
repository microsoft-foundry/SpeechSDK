//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include <CoreFoundation/CFUUID.h>
#include "environment.h"
#include "guid.h"
#include "crt_abstractions.h"

namespace PAL
{
    std::string GenerateGUID()
    {
        CFUUIDRef uuid = CFUUIDCreate(nullptr);
        CFUUIDBytes b = CFUUIDGetUUIDBytes(uuid);
        CFRelease(uuid);

        std::string uuidStr(UUID_LENGTH, '\0');
        PAL::sprintf_s(const_cast<char *>(uuidStr.c_str()),
            uuidStr.size() + 1, // +1 for terminating \0 that the std::string code automatically adds
            "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
            b.byte0, b.byte1, b.byte2, b.byte3, b.byte4, b.byte5, b.byte6, b.byte7,
            b.byte8, b.byte9, b.byte10, b.byte11, b.byte12, b.byte13, b.byte14, b.byte15);

        return uuidStr;
    }
}
