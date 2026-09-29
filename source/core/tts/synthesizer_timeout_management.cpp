//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"
#include <future>
#include <sstream>
#include "synthesizer_timeout_management.h"
#include "service_helpers.h"
#include "create_object_helpers.h"
#include "log_helpers.h"
#include "time_utils.h"
#include "error_info.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

using namespace std;

SynthesisTimeoutManagement::SynthesisTimeoutManagement(
    std::shared_ptr<ISpxThreadService> threadService,
    std::chrono::milliseconds checkInterval)
    : m_threadService(threadService),
    m_checkInterval(checkInterval)
{}

int64_t SynthesisTimeoutManagement::GetMonotonicMilliseconds()
{
    // A monotonic clock is used on purpose: unlike the system (wall) clock it is not
    // affected by NTP sync, manual time changes or vehicle time synchronization, so
    // elapsed time and frame intervals stay valid even if the system time jumps
    // mid-synthesis.
    const auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

void SynthesisTimeoutManagement::SetTimeoutValues(double rtf, std::chrono::milliseconds timeout)
{
    SPX_DBG_TRACE_VERBOSE("%s: RTF set to %.2f; frame timeout interval set to %" PRId64 " ms.", __FUNCTION__, rtf, static_cast<int64_t>(timeout.count()));
    m_rtf = rtf;
    m_frameTimeout = timeout;
    // Set a hard frame timeout of max(10s, timeout * 5)
    m_hardFrameTimeoutMs = max(static_cast<int64_t>(timeout.count() * 5), static_cast<int64_t>(10 * 1000));
}

void SynthesisTimeoutManagement::SetTimeoutCallback(std::function<void(const std::shared_ptr<ISpxErrorInformation> &)> callback)
{
    m_timeoutCallback = callback;
}

void SynthesisTimeoutManagement::OnSynthesisStart()
{
    m_stopped = false;
    m_audioDurationMs = 0;
    m_firstByteTime = 0;
}

void SynthesisTimeoutManagement::OnAudioReceived(int audioLengthMs)
{
    m_audioDurationMs += audioLengthMs;
    m_lastFrameTime = GetMonotonicMilliseconds();
    if (m_firstByteTime.load() == 0)
    {
        m_firstByteTime = m_lastFrameTime.load();
        CheckLoop(shared_from_this());
    }
}

void SynthesisTimeoutManagement::OnSynthesisEnd()
{
    m_stopped = true;
}

bool SynthesisTimeoutManagement::IsTimeout() const
{
    // SynthesisTimeoutManagement doesn't handle first byte timeout now.
    if (m_audioDurationMs == 0)
    {
        return false;
    }

    // Use a monotonic (steady) clock and signed arithmetic: a system time jump can no
    // longer inflate the elapsed time, and an unexpected backwards step can no longer
    // wrap into a huge unsigned interval and fire a false timeout.
    const int64_t currentTime = GetMonotonicMilliseconds();
    const int64_t firstByteTime = m_firstByteTime.load();
    const int64_t lastFrameTime = m_lastFrameTime.load();
    const int64_t elapsedMs = currentTime - firstByteTime;
    const int64_t frameIntervalMs = currentTime - lastFrameTime;

    // Skip the check if timestamps are not initialized yet; do not assume non-positive
    // time_since_epoch() indicates a clock violation.
    if (firstByteTime == 0 || lastFrameTime == 0)
    {
        return false;
    }

    // Defensive monotonicity guard: reject only negative calculated intervals.
    if (elapsedMs < 0 || frameIntervalMs < 0)
    {
        SPX_TRACE_WARNING("%s: monotonicity violation detected (elapsedMs=%" PRId64 ", frameIntervalMs=%" PRId64 "), skip timeout check",
                          __FUNCTION__, elapsedMs, frameIntervalMs);
        return false;
    }

    const auto rtf = static_cast<double>(elapsedMs) / m_audioDurationMs.load();

    // Forcedly set timeout if we didn't receive any audio chunk is last 10s.
    if ((frameIntervalMs > m_frameTimeout.count()
        && rtf > m_rtf) || frameIntervalMs > m_hardFrameTimeoutMs)
    {
        SPX_TRACE_WARNING("%s: synthesis timed out, current RTF: %.2f (threshold: %.2f), frame interval %" PRId64 " ms (threshold %" PRId64 "ms)",
                          __FUNCTION__, rtf, m_rtf, frameIntervalMs, static_cast<int64_t>(m_frameTimeout.count()));
        return true;
    }

    SPX_TRACE_VERBOSE_IF(rtf > m_rtf || frameIntervalMs > m_frameTimeout.count(),
                         "%s: synthesis might timeout, current RTF: %.2f (threshold: %.2f), frame interval %" PRId64 " ms (threshold %" PRId64 "ms)",
                         __FUNCTION__, rtf, m_rtf, frameIntervalMs, static_cast<int64_t>(m_frameTimeout.count()));

    return false;
}

void SynthesisTimeoutManagement::CheckLoop(weak_ptr<SynthesisTimeoutManagement> weakPtr)
{
    packaged_task<void()> task([weakPtr]() -> void
    {
        auto ptr = weakPtr.lock();
        if (ptr == nullptr || ptr->m_stopped)
        {
            return;
        }

        // If the timeout has been reached, call the error callback.
        if (ptr->IsTimeout())
        {
            // Format the error message.
            const int64_t currentTime = GetMonotonicMilliseconds();
            const int64_t elapsedMs = currentTime - ptr->m_firstByteTime.load();
            const int64_t frameIntervalMs = currentTime - ptr->m_lastFrameTime.load();
            const auto rtf = static_cast<double>(elapsedMs) / ptr->m_audioDurationMs.load();
            std::ostringstream ss;
            ss << "Timeout while synthesizing. Current RTF: " << rtf << " (threshold "
               << ptr->m_rtf << "), frame interval " << frameIntervalMs
               << "ms (threshold " << ptr->m_frameTimeout.count() << "ms).";

            ptr->m_timeoutCallback(ErrorInfo::FromExplicitError(
                CancellationErrorCode::ServiceTimeout,
                ss.str()));
        }
        else
        {
            // Otherwise, check again later.
            packaged_task<void()> task([ptr]() { CheckLoop(ptr); });
            ptr->m_threadService->ExecuteAsync(std::move(task), ptr->m_checkInterval);
        }
    });

    auto ptr = weakPtr.lock();
    if (ptr == nullptr || ptr->m_stopped)
    {
        return;
    }

    ptr->m_threadService->ExecuteAsync(std::move(task));
}

}}}} // Microsoft::CognitiveServices::Speech::Impl
