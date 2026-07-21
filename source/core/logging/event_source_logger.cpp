//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#define _CRT_SECURE_NO_WARNINGS

#include "stdafx.h"
#include "event_source_logger.h"
#include "exception.h"
#include <stdio.h>
#include <string.h>

void EventSourceLogger::SetLogFilters(const char *filters)
{
    es_filter->SetFilter(filters);
}

bool EventSourceLogger::IsLoggingEnabled()
{
    ReadLock myLock(&targetLock);
    return callback != NULL;
}

void EventSourceLogger::Log(const char *logLine, int level)
{
    if (callback == NULL || !es_filter->ShouldLog(logLine))
    {
        return;
    }

    ReadLock myLock(&targetLock);
    if (NULL != callback)
    {
        callback(logLine, level);
    }
}

void EventSourceLogger::AttachLogTarget(DIAGNOSTICS_EVENTSOURCE_CALLBACK_FUNC target)
{
    WriteLock myLock(&targetLock);
    callback = target;
}

EventSourceLogger& EventSourceLogger::Instance()
{
    static EventSourceLogger instance;
    return instance;
}
