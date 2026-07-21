//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"

#include <array>
#include <tuple>

#include "handle_helpers.h"
#include "console_logger.h"
#include "file_logger.h"
#include "event_logger.h"
#include "event_source_logger.h"
#include "memory_logger.h"
#include "trace_message.h"
#include "log_config.h"

using namespace Microsoft::CognitiveServices::Speech;
using namespace Microsoft::CognitiveServices::Speech::Impl;

/// <summary>
/// Defined in speechapi_c_property_bag.cpp
/// </summary>
std::shared_ptr<ISpxNamedProperties> property_bag_from_handle(AZAC_HANDLE hpropbag);

static inline std::shared_ptr<ISpxNamedProperties> GetProperties(AZAC_HANDLE hpropbag, void* reserved)
{
    auto properties = reserved == nullptr
        ? property_bag_from_handle(hpropbag)
        : SpxSharedPtrFromThis((ISpxNamedProperties*)reserved);

    SPX_THROW_HR_IF(SPXERR_INVALID_ARG, properties == nullptr);
    return properties;
}

AZAC_API diagnostics_log_start_logging(AZAC_HANDLE hpropbag, void* reserved)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, hpropbag == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, hpropbag != SPXHANDLE_INVALID && reserved != nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto properties = GetProperties(hpropbag, reserved);

        FileLogger::Instance().SetFileOptions(properties);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

AZAC_API diagnostics_log_apply_properties(AZAC_HANDLE hpropbag, void* reserved)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, hpropbag == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, hpropbag != SPXHANDLE_INVALID && reserved != nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto properties = GetProperties(hpropbag, reserved);
        FileLogger::Instance().SetFileOptions(properties);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

AZAC_API diagnostics_log_stop_logging()
{
    SPXAPI_INIT_HR_TRY(hr)
    {
        FileLogger::Instance().CloseFile();
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

AZAC_API_(bool) diagnostics_is_log_level_enabled(int level)
{
    try
    {
        return SpxIsLogLevelEnabled(level);
    }
    catch(...)
    {
        return false;
    }
}

AZAC_API_(void) diagnostics_log_trace_string(int level, const char* pszTitle, const char* fileName, const int lineNumber, const char* psz)
{
    diagnostics_log_trace_message(level, pszTitle, fileName, lineNumber, "%s", psz);
}

AZAC_API_(void) diagnostics_log_trace_message(int level, const char* pszTitle, const char* fileName, const int lineNumber, const char* pszFormat, ...)
{
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

AZAC_API_(void) diagnostics_log_trace_message2(int level, const char* pszTitle, const char* fileName, const int lineNumber, const char* pszFormat, va_list argptr)
{
    try
    {
        SpxTraceMessage2(level, pszTitle, fileName, lineNumber, pszFormat, argptr);
    }
    catch(...)
    {
    }
}

AZAC_API_(void) diagnostics_log_format_message(char *buffer, size_t bufferSize, int level, const char* pszTitle, const char* fileName, const int lineNumber, const char* pszFormat, va_list argptr)
{
    try
    {
        SpxFormatMessage(buffer, bufferSize, level, pszTitle, fileName, lineNumber, pszFormat, argptr);
    }
    catch (...)
    {
    }
}

AZAC_API diagnostics_logmessage_set_callback(DIAGNOSTICS_CALLBACK_FUNC callback)
{
    EventLogger::Instance().AttachLogTarget(callback);
    return SPX_NOERROR;
}

AZAC_API diagnostics_logmessage_set_filters(const char *filters)
{
    EventLogger::Instance().SetLogFilters(filters);
    return SPX_NOERROR;
}

AZAC_API diagnostics_eventsource_logmessage_set_callback(DIAGNOSTICS_EVENTSOURCE_CALLBACK_FUNC callback)
{
    EventSourceLogger::Instance().AttachLogTarget(callback);
    return SPX_NOERROR;
}

AZAC_API diagnostics_eventsource_logmessage_set_filters(const char* filters)
{
    EventSourceLogger::Instance().SetLogFilters(filters);
    return SPX_NOERROR;
}

AZAC_API_(void) diagnostics_log_memory_start_logging()
{
    try
    {
        MemoryLogger::Instance().EnableLogging(true);
    }
    catch(...)
    {
    }
}

AZAC_API_(void) diagnostics_log_memory_stop_logging()
{
    try
    {
        MemoryLogger::Instance().EnableLogging(false);
    }
    catch(...)
    {
    }
}

AZAC_API_(void) diagnostics_log_memory_set_filters(const char* filters)
{
    try
    {
        MemoryLogger::Instance().SetFilters(filters);
    }
    catch(...)
    {
    }
}

AZAC_API_(size_t) diagnostics_log_memory_get_line_num_oldest()
{
    size_t line = 0;
    try
    {
        line = MemoryLogger::Instance().GetLineNumOldest();
    }
    catch(...)
    {
    }
    return line;
}

AZAC_API_(size_t) diagnostics_log_memory_get_line_num_newest()
{
    size_t line = 0;
    try
    {
        line = MemoryLogger::Instance().GetLineNumNewest();
    }
    catch(...)
    {
    }
    return line;
}

AZAC_API__(const char*) diagnostics_log_memory_get_line(size_t lineNum)
{
    const char* line = nullptr;
    try
    {
        line = MemoryLogger::Instance().GetLine(lineNum);
    }
    catch(...)
    {
    }
    return line;
}

AZAC_API diagnostics_log_memory_dump_to_stderr()
{
    return diagnostics_log_memory_dump(nullptr, nullptr, false, true);
}

AZAC_API diagnostics_log_memory_dump(const char* filename, const char* linePrefix, bool emitToStdOut, bool emitToStdErr)
{
    SPXAPI_INIT_HR_TRY(hr)
    {
        MemoryLogger::Instance().Dump(filename, linePrefix, emitToStdOut, emitToStdErr);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

AZAC_API diagnostics_log_memory_dump_on_exit(const char* filename, const char* linePrefix, bool emitToStdOut, bool emitToStdErr)
{
    SPXAPI_INIT_HR_TRY(hr)
    {
        MemoryLogger::Instance().DumpOnExit(filename, linePrefix, emitToStdOut, emitToStdErr);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

AZAC_API_(void) diagnostics_log_console_start_logging(bool logToStderr)
{
    ConsoleLogger::Instance().Start(logToStderr);
}

AZAC_API_(void) diagnostics_log_console_stop_logging()
{
    ConsoleLogger::Instance().Stop();
}

AZAC_API_(void) diagnostics_log_console_set_filters(const char* filters)
{
    ConsoleLogger::Instance().SetFilters(filters);
}

AZAC_API_(void) diagnostics_set_log_level(const char * logger, const char * level)
{
    auto _logger = Into<Logger>{}(logger);
    auto _level = Into<LogLevel>{}(level);
    auto& config = LogConfig::GetConfig(_logger);
    config.Level(_level);
}

//
// Memory tracking API's
///

AZAC_API_(size_t) diagnostics_get_handle_count()
{
    return CSpxSharedPtrHandleTableManager::GetTotalTrackedObjectCount();
}

AZAC_API__(const char*)diagnostics_get_handle_info()
{
    char* result = nullptr;

    auto tempValue = CSpxSharedPtrHandleTableManager::GetHandleCountJson();

    auto size = tempValue.size() + 1;

    result = new char[size];
    PAL::strcpy(result, size, tempValue.c_str(), size, true);

    return result;
}

AZAC_API diagnostics_free_string(const char* value)
{
    SPXAPI_INIT_HR_TRY(hr)
    {
        if (value)
            delete[] value;
    }
    /*
    * This code is often invoked as clean up code, or on function exit. Using the regular SPXAPI_CATCH_AND_RETURN_HR
    * macro has the side effect of storing any exceptions in the handle table. This means we now have to check the HR
    * and remember to remove the stored exception from the handle table. There are several problems here:
    * - Most places in the code don't bother to check the return value. This results in memory leaks as we lose the
    *   exception handle
    * - The default handlers for dealing with non-success codes throw exceptions. Since this can be called as part
    *   of the cleanup code when unwinding the stack for other exceptions, it is bad form to throw an exception
    * - Adding new handlers to retrieve, and log exceptions (but not throw) for all callers of this method seems
    *   excessive
    * Instead, replace with another macro that logs all errors and exceptions, and always returns success. To minimize
    * code changes elsewhere, return the success error code.
    *
    * -> In the future we may want to instead have a void return type (or possible bool) instead
    */
    SPXAPI_CATCH_AND_RETURN(hr, SPX_NOERROR);
}
