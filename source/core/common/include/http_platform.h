//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include <cstdarg>
#include <cstdint>
#include <memory>
#include <functional>

namespace PAL {

    /// <summary>
    /// Represents the HTTP platform
    /// </summary>
    class HttpPlatform
    {
    public:
        /// <summary>
        /// External log function
        /// </summary>
        /// <param name="level">The log level. Please refer to azac_debug.h for supported log levels</param>
        /// <param name="title">Null terminated string that used as the title in the log entry</param>
        /// <param name="fileName">Null terminated string for the name of the file that generated the log entry</param>
        /// <param name="lineNumber">The line number in the file that generated the log entry</param>
        /// <param name="format">Null terminated log format string. This is the message that will logged</param>
        /// <param name="argptr">Pointer to list of dynamic arguments</param>
        typedef void (*ExternalLogFunction)(int level, const char* title, const char* fileName, const int lineNumber, const char* format, va_list argptr);

        /// <summary>
        /// Initializes the HTTP platform. Will throw exceptions in the case of errors. You should call this once before using
        /// any HTTP/WS 
        /// </summary>
        static std::function<void()> Init();

        /// <summary>
        /// Tears down the HTTP platform. This should only be called once after you have called Init(). Will throw exceptions
        /// in the case of errors
        /// </summary>
        static void Teardown();

        /// <summary>
        /// Sets the callback to use for logging from native networking implementation. You should set this once as early as
        /// possible preferable in your main() method or dll load methods. Does not throw exceptions
        /// </summary>
        /// <param name="logFunction">The log function to use</param>
        static void SetLoggingFunction(ExternalLogFunction logFunction);
    };
}
