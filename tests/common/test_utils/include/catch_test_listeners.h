//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include <atomic>
#include <cassert>
#include <cinttypes>
#include <thread>
#include <mutex>
#include <set>
#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/catch_test_case_info.hpp>

#include "azac_api_c_diagnostics.h"

#if defined(__AZAC_DO_TRACE_IMPL) || defined(__SPX_DO_TRACE_IMPL)
    #define SUPPRESS_CORE_COMMON_TRACE_IMPL
#endif
#if defined(__AZAC_THROW_HR_IMPL) || defined(__SPX_THROW_HR_IMPL)
    #define SUPPRESS_CORE_COMMON_THROW_IMPL
#endif
#include "spxcore_common.h"

namespace Azure {
namespace AI {
namespace Test {
namespace Tools {

    #define CATCH_LOG_INFO(fileName, lineNumber, msg, ...) \
        __AZAC_DOTRACE(__AZAC_TRACE_LEVEL_INFO, "CATCH_2_INFO: ", fileName, (int)lineNumber, msg, ##__VA_ARGS__)
    #define CATCH_LOG_ERROR(fileName, lineNumber, msg, ...) \
        __AZAC_DOTRACE(__AZAC_TRACE_LEVEL_ERROR, "CATCH_2_ERROR: ", fileName, (int)lineNumber, msg, ##__VA_ARGS__)

    /// <summary>
    /// Helper class used to attach actions to the Catch2
    /// For more details, please refer to:
    /// https://github.com/catchorg/Catch2/blob/devel/docs/event-listeners.md
    /// https://github.com/catchorg/Catch2/blob/devel/docs/reporter-events.md
    /// </summary>
    class CatchTestRunListener : public Catch::EventListenerBase
    {
    private:
        std::set<std::string> m_testCases;
        std::mutex m_testCasesMutex;
        std::atomic<std::thread::id> m_expectedThreadId;

    public:
        using Catch::EventListenerBase::EventListenerBase;

        /// <summary>
        /// Called at the start of a test run before any tests have run
        /// </summary>
        /// <param name="runInfo">The run information</param>
        void testRunStarting(const Catch::TestRunInfo& runInfo) override
        {
            // TODO actually init memory logging here?

            m_expectedThreadId.store(std::this_thread::get_id(), std::memory_order_release);
            CATCH_LOG_INFO(__FILE__, __LINE__, "Test run starting '%s'", std::string(runInfo.name).c_str());
        }

        /// <summary>
        /// Called at the end of a test run after all the tests have run
        /// </summary>
        /// <param name="runStats">The run statistics</param>
        void testRunEnded(const Catch::TestRunStats& runStats) override
        {
            CATCH_LOG_INFO(
                __FILE__, __LINE__,
                "Test run completed '%s'. Aborting: %d, Passed: %" PRIu64 ", Failed: %" PRIu64,
                std::string(runStats.runInfo.name).c_str(),
                runStats.aborting,
                runStats.totals.testCases.passed,
                runStats.totals.testCases.failed
            );
        }

        /// <summary>
        /// Called when a test case is starting. This will be called only once even if the test case
        /// contains multiple sections
        /// </summary>
        /// <param name="testInfo">Information about the test</param>
        void testCaseStarting(const Catch::TestCaseInfo& testInfo) override
        {
            ValidateThreadId(testInfo.lineInfo.file, testInfo.lineInfo.line);

            CATCH_LOG_INFO(
                testInfo.lineInfo.file,
                testInfo.lineInfo.line,
                "Test case starting '%s'",
                testInfo.name.c_str());

            AddTestCase(testInfo.name);
        }

        /// <summary>
        /// Called when a test case has completed. This will be called only once at the end of the test
        /// case if it contains multiple sections
        /// </summary>
        /// <param name="testCaseStats">The results of the test case</param>
        void testCaseEnded(const Catch::TestCaseStats& testCaseStats) override
        {
            const auto& testInfo = *testCaseStats.testInfo;

            if (testCaseStats.totals.assertions.allPassed())
            {
                CATCH_LOG_INFO(
                    testInfo.lineInfo.file,
                    testInfo.lineInfo.line,
                    "Test case succeeded '%s'. Assertions: %" PRIu64,
                    testInfo.name.c_str(),
                    testCaseStats.totals.assertions.passed);
            }
            else
            {
                CATCH_LOG_ERROR(
                    testInfo.lineInfo.file,
                    testInfo.lineInfo.line,
                    "Test case failed '%s'. Aborted: %d",
                    testInfo.name.c_str(),
                    testCaseStats.aborting);
            }

            RemoveTestCase(testInfo.name);
        }

        /// <summary>
        /// Called when we are entering a test section. Note that all test cases contain one implicitly
        /// defined section
        /// </summary>
        /// <param name="sectionInfo">Information about the section</param>
        void sectionStarting(const Catch::SectionInfo& sectionInfo) override
        {
            ValidateThreadId(sectionInfo.lineInfo.file, sectionInfo.lineInfo.line);

            CATCH_LOG_INFO(
                sectionInfo.lineInfo.file,
                sectionInfo.lineInfo.line,
                "Section starting '%s'",
                sectionInfo.name.c_str());
        }

        /// <summary>
        /// Called when we are done a the test section. Note that all test cases contain one implicitly
        /// defined section
        /// </summary>
        /// <param name="sectionStats">Statistics about the section</param>
        void sectionEnded(const Catch::SectionStats& sectionStats)  override
        {
            auto sectionInfo = sectionStats.sectionInfo;

            if (sectionStats.assertions.allPassed())
            {
                CATCH_LOG_INFO(
                    sectionInfo.lineInfo.file,
                    sectionInfo.lineInfo.line,
                    "Section succeeded '%s'. Assertions: %" PRIu64 ", Runtime: %.2fs",
                    sectionInfo.name.c_str(),
                    sectionStats.assertions.passed,
                    sectionStats.durationInSeconds
                );
            }
            else
            {
                CATCH_LOG_ERROR(
                    sectionInfo.lineInfo.file,
                    sectionInfo.lineInfo.line,
                    "Section failed '%s'. Successful assertions: %" PRIu64 ", Failed assertions: %" PRIu64 ", Runtime: %.2fs",
                    sectionInfo.name.c_str(),
                    sectionStats.assertions.passed,
                    sectionStats.assertions.failed,
                    sectionStats.durationInSeconds
                );

                /*
                    For all test cases in Catch2, there is always an implicit section created with the same name
                    as the test case. So for example given this test case:
                    TEST_CASE("no sections", "")
                    {
                        int x = 12;
                        REQUIRE(x == 12);
                    }
                    sectionStarting and sectionEnded will be called for a section called "no sections"

                    Looking at more involved example:
                    TEST_CASE("The test case", "")
                    {
                        int x = 7;

                        SECTION("Check addition")
                        {
                            int z = 0;

                            WHEN("z is 5")
                            {
                                z = 5;
                            }

                            WHEN("z is 8")
                            {
                                z = 8;
                            }

                            REQUIRE(x + z == 12);
                        }
                    }

                    In this case, we will get called as follows:
                    - testCaseStarting for "The test case"
                        - sectionStarting for "The test case"
                        - sectionStarting for "Check addition"
                        - sectionStarting for "     When: z is 5"   - yes there are spaces at the start
                        - sectionEnded for "     When: z is 5"
                        - sectionEnded for "Check addition"
                        - sectionEnded for "The test case"

                        - sectionStarting for "The test case"
                        - sectionStarting for "Check addition"
                        - sectionStarting for "     When: z is 8"
                        - sectionEnded for "     When: z is 8"
                        - sectionEnded for "Check addition" that indicates failure
                        - sectionEnded for "The test case" that indicates failure
                    - testCaseEnded for "The test case"

                    To correctly only dump the memory logs once, we can simply check if the sectionEnded
                    section name matches the test case. This will ensure we calling it at the end of one
                    logical path through the entire test case

                    // TODO once we move to Catch 3.0.1 we should add handlers for testCasePartial events
                */
                if (ContainsTestCase(sectionInfo.name))
                {
                    DumpMemoryLogs();
                }
            }
        }

        /// <summary>
        /// Called before an assertion is evaluated (but after the expressions have been captured).
        /// </summary>
        /// <param name="assertionInfo">The assertion information</param>
        void assertionStarting(const Catch::AssertionInfo& assertionInfo) override
        {
            ValidateThreadId(assertionInfo.lineInfo.file, assertionInfo.lineInfo.line);
        }

        /// <summary>
        /// Called after an assertion has been evaluated
        /// </summary>
        /// <param name="assertionStats">Assertion statistics</param>
        void assertionEnded(const Catch::AssertionStats& assertionStats) override
        {
            auto sourceInfo = assertionStats.assertionResult.getSourceInfo();

            if (assertionStats.assertionResult.isOk())
            {
                CATCH_LOG_INFO(
                    sourceInfo.file,
                    sourceInfo.line,
                    "Assertion succeeded '%s' -> '%s'",
                    assertionStats.assertionResult.getExpressionInMacro().c_str(),
                    assertionStats.assertionResult.getExpandedExpression().c_str()
                );
            }
            else
            {
                bool hasMessage = assertionStats.assertionResult.hasMessage();
                const auto macroNameRef = assertionStats.assertionResult.getTestMacroName();
                std::string macroName(macroNameRef.data(), macroNameRef.size());

                // uncaught exceptions in a test case bubble up with the macro name set to TEST_CASE
                // and the message containing the exception message
                if (macroName == "TEST_CASE" && hasMessage)
                {
                    CATCH_LOG_ERROR(
                        sourceInfo.file,
                        sourceInfo.line,
                        "Uncaught exception in test case. Message: %s",
                        std::string(assertionStats.assertionResult.getMessage()).c_str()
                    );
                }
                else
                {
                    std::string errorMsg("Assertion failed");
                    if (assertionStats.assertionResult.hasExpression())
                    {
                        errorMsg += " '";
                        errorMsg += assertionStats.assertionResult.getExpressionInMacro();
                        errorMsg += "'";
                    }

                    if (assertionStats.assertionResult.hasExpandedExpression())
                    {
                        errorMsg += " -> '";
                        errorMsg += assertionStats.assertionResult.getExpandedExpression();
                        errorMsg += "'";
                    }

                    if (hasMessage)
                    {
                        errorMsg += ". Message: '";
                        errorMsg += std::string(assertionStats.assertionResult.getMessage());
                        errorMsg += "'";
                    }

                    CATCH_LOG_ERROR(sourceInfo.file, sourceInfo.line, "%s", errorMsg.c_str());
                }
            }
        }

    private:
        void ValidateThreadId(const char* fileName, size_t lineNum) const
        {
            // TODO: Should this error log be removed (or updated) now that we have locks around Catch2 calls?
            // See CATCH_AUTO_LOCK() in test_utils.h

            // Catch2 does not have built-in concurrency protection. Let's try to detect when Catch2
            // is invoked outside of the thread running the tests
            auto id = std::this_thread::get_id();
            if (id != m_expectedThreadId.load(std::memory_order_acquire))
            {
                CATCH_LOG_ERROR(fileName, lineNum,
                    "Catch2 methods called from wrong thread. Catch2 is not thread safe%s",
                    ""); // additional unused parameter since "ISO C++11 requires at least one argument for the '...' in a variadic macro"

                // TODO for now this is disabled since too many tests are using test assertions from
                //      from event callbacks. Uncomment this out once those are fixed
                //throw std::runtime_error("Catch2 methods called from wrong thread");
            }
        }

        void AddTestCase(const std::string& name)
        {
            std::lock_guard<std::mutex> lock(m_testCasesMutex);
            m_testCases.insert(name);
        }

        void RemoveTestCase(const std::string& name)
        {
            std::lock_guard<std::mutex> lock(m_testCasesMutex);
            m_testCases.erase(name);
        }

        bool ContainsTestCase(const std::string& name)
        {
            std::lock_guard<std::mutex> lock(m_testCasesMutex);
            return m_testCases.find(name) != m_testCases.cend();
        }

        void DumpMemoryLogs()
        {
#if _DEBUG
            // Debug builds log to stderr by default. To avoid double logging, we won't dump the memory logs here

            // TODO check if logging to stdout or stderr is enabled and if not dump the memory logs anyway?
#else
            diagnostics_log_memory_dump_to_stderr();
#endif
        }
    };

    #undef CATCH_LOG_INFO
    #undef CATCH_LOG_ERROR

}}}}
