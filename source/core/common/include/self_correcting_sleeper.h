//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

// std::this_thread::sleep_for() only guarantees that the thread will sleep until *at least* the requested duration
// has elapsed -- depending on system load and environment clock configuration, significantly more real time than the
// the requested duration may elapse before the thread resumes.
//
// This isn't usually a problem for small deviations on single sleeps, but in situations where we use injected sleeps
// to produce or simulate a specific rate (e.g. streaming data at the same bitrate as realtime), even "small" defects
// accumulate and add to a lower rate than intended, owing to the higher individual sleep times.
//
// This class is just a very basic wrapper that will record the "time defect" after each sleep performed and then
// deduct as much of this defect as it can from subsequent sleep requests made on the same object. In that way, the net
// balance of actual time slept across repeated use will stay within the bounds of a single defect instead of
// accumulating.

#pragma once

#include <chrono>
#include <thread>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

class SelfCorrectingSleeper
{
private:
    // nanoseconds chosen arbitrarily for maximum precision; most environments won't really use ns resolution
    std::chrono::nanoseconds m_cumulativeSleepDebt{ 0 };

public:
    void sleep_for(
        std::chrono::nanoseconds sleep_time,
        std::chrono::nanoseconds maximum_sleep_debt)
    {
        // We'll repay as much of the accumulated sleep debt as we can by reducing the net duration of the requested
        // sleep -- but this repayment can't be bigger than the requested maximum or the sleep itself.
        std::chrono::nanoseconds sleepDebtPayment{ m_cumulativeSleepDebt };
        sleepDebtPayment = std::min(sleepDebtPayment, maximum_sleep_debt);
        sleepDebtPayment = std::min(sleepDebtPayment, sleep_time);

        auto netSleepDuration = sleep_time - sleepDebtPayment;
        m_cumulativeSleepDebt -= sleepDebtPayment;

        // Record when we "should" wake up and then increase the accumulated debt we'll pay back (as per above, with
        // reductions to future sleep call durations) by the observed defect from this sleep
        auto targetWakeTime = std::chrono::steady_clock::now() + netSleepDuration;
        std::this_thread::sleep_for(netSleepDuration);
        m_cumulativeSleepDebt += std::chrono::steady_clock::now() - targetWakeTime;
    }

    void sleep_for(std::chrono::nanoseconds sleep_time)
    {
        sleep_for(sleep_time, std::chrono::nanoseconds::max());
    }

    void reset()
    {
        m_cumulativeSleepDebt = std::chrono::nanoseconds(0);
    }
};

}}}} // Microsoft::CognitiveServices::Speech::Impl
