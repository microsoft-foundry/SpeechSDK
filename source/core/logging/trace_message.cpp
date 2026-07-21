//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// trace_message.cpp: SpxTraceMessage() implementation definition
//
#define _CRT_SECURE_NO_WARNINGS

#include "stdafx.h"

#include <chrono>
#include <stdio.h>
#include <sstream>
#include <iostream>

#include "console_logger.h"
#include "event_logger.h"
#include "event_source_logger.h"
#include "exception.h"
#include "file_logger.h"
#include "file_utils.h"
#include "log_config.h"
#include "memory_logger.h"
#include "speechapi_c_diagnostics.h"
#include "string_utils.h"
#include "trace_message.h"
#include "spx_build_information.h"

// Note: in case of android, log to logcat
#if defined(ANDROID) || defined(__ANDROID__)
#include <android/log.h>
#endif

#define SPX_CONFIG_INCLUDE_TRACE_THREAD_ID      1
#define SPX_CONFIG_INCLUDE_TRACE_HIRES_CLOCK    1
// #define SPX_CONFIG_INCLUDE_TRACE_WINDOWS_DEBUGGER   1

#ifdef SPX_CONFIG_INCLUDE_TRACE_WINDOWS_DEBUGGER
#include <windows.h>
#endif // SPX_CONFIG_INCLUDE_TRACE_WINDOWS_DEBUGGER

decltype(std::chrono::high_resolution_clock::now()) __g_spx_trace_message_time0 = std::chrono::high_resolution_clock::now();

struct ShouldLoggerEmitAccounting
{
    bool ShouldAnyEmit = false;
    bool ShouldEmit[5];

    ShouldLoggerEmitAccounting() = default;

    ShouldLoggerEmitAccounting(bool console, bool file, bool events, bool memory, bool eventsource)
    : ShouldAnyEmit { console || file || events || memory || eventsource }
    , ShouldEmit { console, file, events, memory, eventsource }
    {}
};

static inline ShouldLoggerEmitAccounting GetAllLoggerConfigForLevel(int level)
{
    static const ShouldLoggerEmitAccounting cLoggingDisabled {};

    using namespace Microsoft::CognitiveServices::Speech::Impl;
    bool logToConsole = ConsoleLogger::Instance().IsLoggingEnabled();
    bool logToFile = FileLogger::Instance().IsFileLoggingEnabled();
    bool logToEvents = EventLogger::Instance().IsLoggingEnabled();
    bool logToMemory = MemoryLogger::Instance().IsLoggingEnabled();
    bool logToEventSource = EventSourceLogger::Instance().IsLoggingEnabled();

    bool enabled = logToConsole || logToFile || logToEvents || logToMemory || logToEventSource;
    if (!enabled)
    {
        return cLoggingDisabled;
    }

    auto shouldEmit = [level](auto logger)
    {
        auto& config = LogConfig::GetConfig(logger);
        return (level & static_cast<int>(config.Level())) != 0;
    };

    ShouldLoggerEmitAccounting loggerConfigForLogLevel
        {
            logToConsole && shouldEmit(Logger::Console),
            logToFile && shouldEmit(Logger::File),
            logToEvents && shouldEmit(Logger::Events),
            logToMemory && shouldEmit(Logger::Memory),
            logToEventSource && shouldEmit(Logger::EventSource)
        };

    return loggerConfigForLogLevel;
}

SPX_EXTERN_C bool SpxIsLogLevelEnabled(int level)
{
    return GetAllLoggerConfigForLevel(level).ShouldAnyEmit;
}

SPX_EXTERN_C void SpxTraceMessage1(int level, const char* pszTitle, const char* fileName, const int lineNumber, const char* pszFormat, ...)
{
    UNUSED(level);
    try
    {
        va_list argptr;
        va_start(argptr, pszFormat);
        SpxTraceMessage2(level, pszTitle, fileName, lineNumber, pszFormat, argptr);
        va_end(argptr);
    }
    catch(...)
    {
    }
}

SPX_EXTERN_C void SpxTraceMessage2(int level, const char* pszTitle, const char* fileName, const int lineNumber, const char* pszFormat, va_list argptr)
{
    using namespace Microsoft::CognitiveServices::Speech::Impl;

    ShouldLoggerEmitAccounting loggerConfigForLogLevel = GetAllLoggerConfigForLevel(level);
    if (!loggerConfigForLogLevel.ShouldAnyEmit)
    {
        return;
    }

    char sz[4096];
    SpxFormatMessage(sz, 4096, level, pszTitle, fileName, lineNumber, pszFormat, argptr);

// #if (defined(DEBUG) || defined(_DEBUG)) && defined(_MSC_VER)
//     static bool breakOnError = true;
//     if (breakOnError && strstr(pszTitle, "_ERROR") != nullptr)
//     {
//         __debugbreak();
//     }
//     if (breakOnError && strstr(pszTitle, "_WARNING") != nullptr)
//     {
//         __debugbreak();
//     }
//     if (breakOnError && strstr(pszTitle, "FAIL") != nullptr)
//     {
//         __debugbreak();
//     }
// #endif


#ifdef SPX_CONFIG_INCLUDE_TRACE_WINDOWS_DEBUGGER
    OutputDebugStringA(sz);
#endif // SPX_CONFIG_INCLUDE_TRACE_WINDOWS_DEBUGGER

#if defined(ANDROID) || defined(__ANDROID__)
    int androidPrio = ANDROID_LOG_ERROR;
    switch (level)
    {
    case __SPX_TRACE_LEVEL_INFO:    androidPrio = ANDROID_LOG_INFO;     break; // Trace_Info
    case __SPX_TRACE_LEVEL_WARNING: androidPrio = ANDROID_LOG_WARN;     break; // Trace_Warning
    case __SPX_TRACE_LEVEL_ERROR:   androidPrio = ANDROID_LOG_ERROR;    break; // Trace_Error
    case __SPX_TRACE_LEVEL_VERBOSE: androidPrio = ANDROID_LOG_VERBOSE;  break; // Trace_Verbose
    default: androidPrio = ANDROID_LOG_FATAL; break;
    }

    if (loggerConfigForLogLevel.ShouldEmit[static_cast<int>(Logger::Console)])
    {
        __android_log_write(androidPrio, Microsoft::CognitiveServices::Speech::Impl::BuildInformation::g_SpeechSDKName, sz);
    }
#else
    if (loggerConfigForLogLevel.ShouldEmit[static_cast<int>(Logger::Console)])
    {
        ConsoleLogger::Instance().LogToConsole(sz);
    }
    if (loggerConfigForLogLevel.ShouldEmit[static_cast<int>(Logger::EventSource)])
    {
        EventSourceLogger::Instance().Log(sz, level);
    }
#endif
    if (loggerConfigForLogLevel.ShouldEmit[static_cast<int>(Logger::File)])
    {
        FileLogger::Instance().LogToFile(sz);
    }
    if (loggerConfigForLogLevel.ShouldEmit[static_cast<int>(Logger::Memory)])
    {
        MemoryLogger::Instance().LogToMemory(sz);
    }
    if (loggerConfigForLogLevel.ShouldEmit[static_cast<int>(Logger::Events)])
    {
        EventLogger::Instance().LogToEvent(sz);
    }
}

SPX_EXTERN_C void SpxFormatMessage(char *buffer, size_t bufferSize, int level, const char* pszTitle, const char* fileName, const int lineNumber, const char* pszFormat, va_list argptr)
{
    UNUSED(level);

    if (bufferSize == 0)
    {
        return;
    }

    std::string format;
    if (SPX_CONFIG_INCLUDE_TRACE_THREAD_ID)
    {
        auto threadHash = std::hash<std::thread::id>{}(std::this_thread::get_id());
        format += "[" + std::to_string(threadHash % 1000000) + "]: ";
    }

    if (SPX_CONFIG_INCLUDE_TRACE_HIRES_CLOCK)
    {
        auto now = std::chrono::high_resolution_clock::now();
        unsigned long delta = (unsigned long)std::chrono::duration_cast<std::chrono::milliseconds>(now - __g_spx_trace_message_time0).count();
        format += std::to_string(delta) + "ms ";
    }

    while (*pszFormat == '\n' || *pszFormat == '\r')
    {
        if (*pszFormat == '\r')
        {
            pszTitle = nullptr;
        }

        format += *pszFormat++;
    }

    if (pszTitle != nullptr)
    {
        format += pszTitle;
    }

    std::string fileNameOnly(fileName);
    std::replace(fileNameOnly.begin(), fileNameOnly.end(), '\\', '/');

    std::ostringstream fileNameLineNumber;
    fileNameLineNumber << " " << fileNameOnly.substr(fileNameOnly.find_last_of('/', std::string::npos) + 1) << ":" << lineNumber << " ";

    format += fileNameLineNumber.str();

    format += pszFormat;
    if (format.length() < 1 || format[format.length() - 1] != '\n')
    {
        format += "\n";
    }

// Our upstream callers are protected.
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored  "-Wformat-nonliteral"
#endif
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored  "-Wformat-nonliteral"
#endif
    /*
     * Some notes about vsnprintf:
     * - returns negative value in the case of formatting errors
     * - returns the number of characters that would have been written to the buffer even if the buffer is not large enough
     * - will truncate the output to fit in the buffer
     * - will always null terminate the string written to the buffer (unless the buffer size is 0)
     */
    int numWritten = vsnprintf(buffer, bufferSize, format.c_str(), argptr);
    if (numWritten <= 0)
    {
        buffer[0] = '\0';
        return;
    }
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
#ifdef __clang__
#pragma clang diagnostic pop
#endif

    // If the formatted string was too long for the buffer, we have lost the trailing \n and
    // need to add it back. The buffer will always have a \0 at (bufferSize - 1) index, and
    // we need the string to be null terminated. So we just set the (bufferSize - 2) index to
    // be \n overwriting the character at that position
    if (static_cast<size_t>(numWritten) >= bufferSize)
    {
        buffer[bufferSize - 2] = '\n';
    }

    static constexpr char prefix[] = "Microsoft::CognitiveServices::Speech::Impl::";
    static constexpr auto prefixLength = sizeof(prefix) - 1;
    auto start = buffer;
    while (true)
    {
        auto found = strstr(start, prefix);
        if (!found) break;

        // Calculate remaining length after the prefix
        auto remainingLength = strlen(&found[prefixLength]) + 1; // +1 for null terminator
        memmove(found, &found[prefixLength], remainingLength);

        start = found;
    }
}
