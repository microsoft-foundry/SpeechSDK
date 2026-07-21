//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// event_log.h: Class to log to a user registered event.
//

#include "log_utils.h"
#include "speechapi_c_diagnostics.h"

#pragma once

class ConsoleLogger
{
public:
    static ConsoleLogger& Instance();

    ConsoleLogger(ConsoleLogger const&) = delete;
    void operator=(ConsoleLogger const&) = delete;

    void SetFilters(const char *filters);

    void Start(bool logToStderr);
    void Stop();

    bool IsLoggingEnabled();
    void LogToConsole(const char *format);
private:
    ConsoleLogger()
    {
#if defined(DEBUG) || defined(_DEBUG)
        m_isEnabled = true;
#endif
    };

    ReaderWriterLock m_targetLock;

    LogFilter m_filter;

    bool m_isEnabled = false;
    bool m_logToStderr = true;
};
