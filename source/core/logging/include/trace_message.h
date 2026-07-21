//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// trace_message.h: SpxTraceMessage() implementation declaration
//

#pragma once

#include <stdarg.h>

#if defined(__GNUC__) || defined(__clang__)
#define ATTRIB_FORMAT_PRINTF(x1, x2) __attribute__ (( format( printf, x1, x2 ) ))
#else
#define ATTRIB_FORMAT_PRINTF(x1, x2)
#endif

#ifdef _MSC_VER
#define MSVC_PRINTF_FORMAT _Printf_format_string_
#else
#define MSVC_PRINTF_FORMAT
#endif

SPX_EXTERN_C {

extern void SpxFormatMessage(char *buffer, size_t bufferSize, int level, const char* pszTitle, const char* fileName, const int lineNumber, const char* pszFormat, va_list argptr);
extern void SpxTraceMessage1(int level, const char* pszTitle, const char* fileName, const int lineNumber, MSVC_PRINTF_FORMAT const char* pszFormat, ...) ATTRIB_FORMAT_PRINTF(5, 6);
extern void SpxTraceMessage2(int level, const char* pszTitle, const char* fileName, const int lineNumber, MSVC_PRINTF_FORMAT const char* pszFormat, va_list argptr);
extern bool SpxIsLogLevelEnabled(int level);

}
