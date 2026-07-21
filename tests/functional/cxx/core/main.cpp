//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"

#define SPXTEST_PROVIDES_MAIN
#include "test_utils.h"
#include "trace_message.h"
#include "http_platform.h"
#include <catch2/reporters/catch_reporter_registrars.hpp>
#include "catch_test_listeners.h"

using CatchTestRunListener = Azure::AI::Test::Tools::CatchTestRunListener;
CATCH_REGISTER_LISTENER(CatchTestRunListener)

#if defined(_MSC_VER) && defined(_DEBUG)
// in case of asserts in debug mode, print the message into stderr and throw exception
int HandleDebugAssert(int,               // reportType  - ignoring reportType, printing message and aborting for all reportTypes
    char *message,     // message     - fully assembled debug user message
    int * returnValue) // returnValue - retVal value of zero continues execution
{
    fprintf(stderr, "C-Runtime: %s\n", message);

    if (returnValue) {
        *returnValue = 0;   // return value of 0 will continue operation and NOT start the debugger
    }

    return 1;            // make sure no message box is displayed
}
#endif

int main(int argc, char* argv[])
{
#if defined(_MSC_VER) && defined(_DEBUG)
    // in case of asserts in debug mode, print the message into stderr and throw exception
    if (_CrtSetReportHook2(_CRT_RPTHOOK_INSTALL, HandleDebugAssert) == -1)
    {
        fprintf(stderr, "_CrtSetReportHook2 failed.\n");
        return -1;
    }
#endif

    PAL::HttpPlatform::SetLoggingFunction(SpxTraceMessage2);

    Catch::Session session; // There must be exactly one instance

    // The catch2 test adapter runs a Discovery phase and we shouldn't attempt io during this phase
    auto enableMemoryLogging = false;
    if (!checkForDiscovery(argc, argv))
    {
        auto logging = PAL::SpxGetEnv("SPEECHSDK_TEST_LOGGING").GetOr("memory");
        enableMemoryLogging = logging.find("memory") != logging.npos;

        if (enableMemoryLogging) diagnostics_log_memory_start_logging();

        std::string rootPath(argv[0]);
        std::replace(rootPath.begin(), rootPath.end(), '\\', '/');
        std::string rootPathOnly = rootPath.substr(0, rootPath.find_last_of('/') + 1);

        ConfigSettings::LoadFromJsonFile(rootPathOnly);
    }
    else
    {
        // Console logging output can interfere with discovery.
        diagnostics_log_console_stop_logging();
    }

    // Let Catch (using Clara) parse the command line
    int returnCode = parse_cli_args(session, argc, argv);
    if (returnCode != 0) // Indicates a command line error
    {
        return returnCode;
    }

    add_signal_handlers();

    auto memoryLoggerOnExit = enableMemoryLogging && (Config::MemoryLoggerExit.size() > 0 || Config::MemoryLoggerExitFile.size() > 0);
    if (memoryLoggerOnExit) diagnostics_log_memory_dump_on_exit(Config::MemoryLoggerExitFile.c_str(), "CRBN_EXIT", false, Config::MemoryLoggerExitFile.size() == 0);

    returnCode = session.run();

    auto keepMemoryLoggerOnExit = Config::MemoryLoggerExit == "always"
        || (Config::MemoryLoggerExit == "failed" && returnCode != 0)
        || (memoryLoggerOnExit && Config::MemoryLoggerExit.size() == 0);

    auto cancelMemoryLoggerOnExit = (!keepMemoryLoggerOnExit || Config::MemoryLoggerExit == "unexpected") && memoryLoggerOnExit;
    if (cancelMemoryLoggerOnExit) diagnostics_log_memory_dump_on_exit(nullptr, nullptr, false, false);

    return returnCode;
}

std::string ResolvePath(const std::string& relativePath)
{
    auto trimmed = PAL::StringUtils::Trim(relativePath);
    if (trimmed.empty())
    {
        throw std::invalid_argument("File path is not set");
    }

    return DefaultSettingsMap[INPUT_DIR] +
        (trimmed[0] == '/' || trimmed[0] == '\\'
            ? relativePath
            : "/" + relativePath);
}
