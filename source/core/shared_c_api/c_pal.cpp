//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#include "stdafx.h"
#include "azac_api_c_pal.h"

#include <cstring>

#include "string_utils.h"

template<typename Dst, typename Src, typename F>
size_t ConvertString(Dst * dst, const Src * src, size_t dstSize, F convertFn)
{
    if (src == nullptr)
    {
        return 0;
    }
    auto dstStr = convertFn(src);
    if ((dst != nullptr) && (dstSize > 0))
    {
        auto bufferSizeInChars = std::min(dstSize, (dstStr.size() + 1));
        std::memcpy(dst, dstStr.c_str(), bufferSizeInChars * sizeof(Dst));
        dst[bufferSizeInChars - 1] = Dst{ 0 };
    }
    return dstStr.size() + 1;
}

AZAC_API_(size_t) pal_wstring_to_string(char * dst, const wchar_t * src, size_t dstSize)
{
    return ConvertString(dst, src, dstSize, &PAL::ToString);
}

AZAC_API_(size_t) pal_string_to_wstring(wchar_t * dst, const char * src, size_t dstSize)
{
    return ConvertString(dst, src, dstSize, &PAL::ToWString);
}