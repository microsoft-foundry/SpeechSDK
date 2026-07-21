//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// event_log.h: Class to log to a user registered event.
//

#include "log_utils.h"
#include "speechapi_c_diagnostics.h"

#pragma once

class EventSourceLogger
{
public:
    static EventSourceLogger& Instance();

    EventSourceLogger(EventSourceLogger const&) = delete;
    void operator=(EventSourceLogger const&) = delete;

    void SetLogFilters(const char *filters);
    void AttachLogTarget(DIAGNOSTICS_EVENTSOURCE_CALLBACK_FUNC target);
    
    bool IsLoggingEnabled();
    void Log(const char *format, int level);
private:
    EventSourceLogger()
    {
        es_filter = std::make_shared<LogFilter>();
    };
    ~EventSourceLogger()
    {
        callback = NULL;
    }

    ReaderWriterLock targetLock;

    std::shared_ptr<LogFilter> es_filter;

    DIAGNOSTICS_EVENTSOURCE_CALLBACK_FUNC callback;
};
