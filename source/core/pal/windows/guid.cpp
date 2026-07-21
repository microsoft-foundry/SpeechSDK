//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include <stdexcept>

#include <combaseapi.h>

#include "environment.h"
#include "guid.h"

namespace PAL
{
    std::string GenerateGUID()
    {
        RPC_CSTR str = nullptr;

        try
        {
            GUID guid;
            HRESULT hr = CoCreateGuid(&guid);
            if (hr != S_OK)
            {
                throw std::runtime_error{ "GUID creation failed." };
            }

            RPC_STATUS res = UuidToStringA(&guid, &str);
            if (res != S_OK)
            {
                throw std::runtime_error{ "Failed to convert the GUID to a string." };
            }

            std::string guidStr((const char*)str, UUID_LENGTH);
            RpcStringFreeA(&str);

            return guidStr;
        }
        catch (...)
        {
            if (str)
            {
                RpcStringFreeA(&str);
            }

            throw;
        }
    }
}
