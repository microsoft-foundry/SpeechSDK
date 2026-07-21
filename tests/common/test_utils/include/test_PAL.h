//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include <string>
#include <interfaces/proxy_server_info.h>

/// <summary>
/// Some test related general utilities
/// </summary>
class TestUtils
{
private:
    static bool m_debugFlag;

public:
    /// <summary>
    /// Gets whether or not a debugger is currently attached to your running instance
    /// </summary>
    /// <returns>True if a debugger is attached, false if there isn't one or we don't know</returns>
    static bool IsDebuggerAttached();

    /// <summary>
    /// Gets the proxy information set in your OS (if any)
    /// </summary>
    /// <param name="info">The proxy server information to fill out</param>
    /// <returns>True if there was a proxy set, and we were able to read the
    /// information. False otherwise</returns>
    static bool GetSystemProxy(ProxyServerInfo& info);

    /// <summary>
    /// Sets whether or not the debug flag (--debug) was specified when running the tests
    /// </summary>
    /// <param name="debugFlag">True if the debug flag was set, false otherwise</param>
    static void SetDebugFlag(bool debugFlag);

    /// <summary>
    /// Gets whether or not the debug flag was set when running the tests
    /// </summary>
    /// <returns>True if the debug flag was set</returns>
    static bool GetDebugFlag();

private:
    TestUtils() = default;
    TestUtils(const TestUtils&) = delete;
    TestUtils(TestUtils&&) = delete;
};
