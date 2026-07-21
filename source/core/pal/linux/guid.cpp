//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include <uuid/uuid.h>
#include "environment.h"
#include "guid.h"

namespace PAL
{
    std::string GenerateGUID()
    {
        uuid_t uuidVal;
        uuid_generate(uuidVal);

        char uuidChars[UUID_LENGTH + 1];
        uuid_unparse_lower(uuidVal, uuidChars);

        return std::string{ uuidChars, UUID_LENGTH };
    }
}
