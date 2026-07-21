//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#define _CRT_SECURE_NO_WARNINGS

#include "stdafx.h"
#include "console_logger.h"
#include "exception.h"
#include <stdio.h>
#include <string.h>

void ConsoleLogger::SetFilters(const char *filters)
{
    m_filter.SetFilter(filters);
}

bool ConsoleLogger::IsLoggingEnabled()
{
    ReadLock myLock(&m_targetLock);
    return m_isEnabled;
}

void ConsoleLogger::LogToConsole(const char *logLine)
{
    if (!m_filter.ShouldLog(logLine))
    {
        return;
    }

    ReadLock myLock(&m_targetLock);
    if (m_isEnabled)
    {
        fprintf(m_logToStderr ? stderr : stdout, "%s", logLine);
    }
}

ConsoleLogger& ConsoleLogger::Instance()
{
    static ConsoleLogger instance;
    return instance;
}

void ConsoleLogger::Start(bool logToStderr)
{
    WriteLock myLock(&m_targetLock);
    m_isEnabled = true;
    m_logToStderr = logToStderr;
}

void ConsoleLogger::Stop()
{
    WriteLock myLock(&m_targetLock);
    m_isEnabled = false;
}
