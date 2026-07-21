//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#define __STDC_WANT_LIB_EXT1__ 1

#include <errno.h>
#include <stdio.h>
#include <stdarg.h>

namespace PAL {

    int sprintf_s(char* buffer, size_t count, const char* format, ...)
    {
        if (count == 0)
        {
            return 0;
        }

        if (buffer == nullptr || format == nullptr)
        {
            errno = EINVAL;
            return -1;
        }

        int ret;
        va_list args;
        va_start(args, format);

#if _MSC_VER || defined __STDC_LIB_EXT1__
        ret = ::vsprintf_s(buffer, count, format, args);
#else
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored  "-Wformat-nonliteral"
#endif
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored  "-Wformat-nonliteral"
#endif
        ret = vsnprintf(buffer, count, format, args);
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
#ifdef __clang__
#pragma clang diagnostic pop
#endif
        if ((size_t)ret >= count)
        {
            // this means we overflowed
            buffer[0] = 0;
            ret = -1;
        }
#endif

        va_end(args);
        return ret;
    }

}
