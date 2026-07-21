//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#define _CRT_SECURE_NO_WARNINGS

#include "stdafx.h"
#include "memory_logger.h"
#include "fatal_exit_monitor.h"
#include "file_utils.h"
#include "exception.h"
#include "time_utils.h"
#include <stdio.h>
#include <string.h>
#include <chrono>

// Note: Log to logcat for Android
#if defined(ANDROID) || defined(__ANDROID__)
#include <android/log.h>
#endif

MemoryLogger& MemoryLogger::Instance()
{
    static MemoryLogger instance;
    return instance;
}

void MemoryLogger::EnableLogging(bool enable)
{
    if (enable)
    {
        m_started++;
    }
    else if (m_started > 0)
    {
        m_started--;
    }

    auto msg = m_started == 0 ? "stopped logging"
        : enable && m_started == 1 ? "started logging"
        : enable ? "enable logging" : "disable logging";

    auto now = std::chrono::system_clock::now();
    auto time = PAL::GetTimeInString(now);

    SPX_TRACE_INFO("%s; c=%zu; %s", msg, m_started, time.c_str());
}

void MemoryLogger::SetFilters(const char *filters)
{
    if (filters)
    {
        m_filter.SetFilter(filters);
    }
    else
    {
        m_filter.SetFilter("");
    }
}

bool MemoryLogger::IsLoggingEnabled()
{
    return m_started > 0;
}

void MemoryLogger::Dump(const char* filename, const char* linePrefix, bool emitToStdOut, bool emitToStdErr)
{
    bool emitToFile = filename != nullptr && filename[0] != '\0';
    if (!emitToFile && !emitToStdOut && !emitToStdErr)
    {
        return;
    }

    FILE* outputFile = nullptr;
    if (emitToFile)
    {
        PAL::fopen_s(&outputFile, filename, "w");
        SPX_THROW_HR_IF(SPXERR_MEMORY_LOG_FILE_OPEN_FAILED, outputFile == nullptr);
    }

    // Lock the ticket range, so nobody can add more log lines, until we're done
    // NOTE: We lock starting at LogTicketStep2_UpdateLines, to allow 1 competing thread to start
    // its logging by copying into the buffer (step1)... by design and experimentation/validation it's safe
    auto lockSteps = m_tickets.LockStepsInclusive(LogTicketStep2_UpdateLines, LogDumpStep1_FileWrite);

    linePrefix = (linePrefix == nullptr) ? "CRBN" : linePrefix;

    auto start = GetLineNumOldest();
    auto stop = GetLineNumNewest();

    for (auto i = start; i < stop; i++)
    {
        auto line = GetLine(i);
        if (line != nullptr)
        {
#if defined(ANDROID) || defined(__ANDROID__)
            SPX_IFTRUE(emitToStdOut, __android_log_write(ANDROID_LOG_INFO, linePrefix, line));
            SPX_IFTRUE(emitToStdErr, __android_log_write(ANDROID_LOG_WARN, linePrefix, line));
#else
            SPX_IFTRUE(emitToStdOut, fprintf(stdout, "%s: %s", linePrefix, line));
            SPX_IFTRUE(emitToStdErr, fprintf(stderr, "%s: %s", linePrefix, line));
#endif
            SPX_IFTRUE(emitToFile, fprintf(outputFile, "%s: %s", linePrefix, line));
        }
    }

    // Dispose our tickets... we don't need any locks to flush/close the file
    lockSteps[1].DisposeTicket();
    lockSteps[0].DisposeTicket();
    SPX_IFTRUE(emitToFile, fclose(outputFile));
}

void MemoryLogger::DumpOnExit(const char* filename, const char* linePrefix, bool emitToStdOut, bool emitToStdErr)
{
    SPX_DBG_TRACE_FUNCTION();

    auto fileOk = filename != nullptr && filename[0] != '\0';
    m_dumpOnExit.m_enabled = fileOk || emitToStdOut || emitToStdErr;
    FatalExitMonitor::Instance().Enable(m_dumpOnExit.m_enabled);

    m_dumpOnExit.m_fileName = fileOk ? filename : "";
    m_dumpOnExit.m_linePrefix = linePrefix ? linePrefix : "";
    m_dumpOnExit.m_emitToStdOut = emitToStdOut;
    m_dumpOnExit.m_emitToStdErr = emitToStdErr;
}

void MemoryLogger::Exit()
{
    SPX_DBG_TRACE_FUNCTION();
    if (m_dumpOnExit.m_enabled)
    {
        return;
    }

    try
    {
        auto linePrefix = m_dumpOnExit.m_linePrefix.size() ? m_dumpOnExit.m_linePrefix.c_str() : nullptr;
        Dump(m_dumpOnExit.m_fileName.c_str(), linePrefix, m_dumpOnExit.m_emitToStdOut, m_dumpOnExit.m_emitToStdErr);
    }
    catch (...)
    {
        // eat all exceptions, as the process is terminating...
    }
}

void MemoryLogger::LogToMemory(const char *traceLine)
{
    if (!m_filter.ShouldLog(traceLine))
    {
        return;
    }

    auto traceLineCch = strlen(traceLine) + 1;

    auto ticket = m_tickets.CreateTicketGuard();
    ticket.AdvanceToStep(LogTicketStep1_UpdateBuffer); // ---------- STEP 1

    SPX_IFTRUE(m_bufferPtr + traceLineCch + 1 > m_bufferEnd, m_bufferPtr = m_bufferStart);
    auto lineBufferPtr = m_bufferPtr;
    m_bufferPtr += traceLineCch + 1;

    strcpy(lineBufferPtr, traceLine);
    lineBufferPtr[traceLineCch + 1] = '\0';

    ticket.AdvanceToStep(LogTicketStep2_UpdateLines); // ---------- STEP 2

    auto lineNum = m_numLines % m_maxLines;
    m_traceLinePtrs[lineNum] = lineBufferPtr;
    m_numLines++;
}
