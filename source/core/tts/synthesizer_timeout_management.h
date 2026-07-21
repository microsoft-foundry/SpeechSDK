//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include <chrono>
#include "ispxinterfaces.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {


class SynthesisTimeoutManagement : public std::enable_shared_from_this<SynthesisTimeoutManagement>
{
public:
    SynthesisTimeoutManagement(std::shared_ptr<ISpxThreadService> threadService,
                               std::chrono::milliseconds checkInterval = std::chrono::milliseconds(50));
    void SetTimeoutValues(double rtf, std::chrono::milliseconds timeout);
    void SetTimeoutCallback(std::function<void(const std::shared_ptr<ISpxErrorInformation> &)> callback);
    void OnSynthesisStart();
    void OnAudioReceived(int audioLengthMs);
    void OnSynthesisEnd();

private:

    bool IsTimeout() const;
    static void CheckLoop(std::weak_ptr<SynthesisTimeoutManagement> weakPtr);

private:
    std::shared_ptr<ISpxThreadService> m_threadService;
    std::function<void(const std::shared_ptr<ISpxErrorInformation> &)> m_timeoutCallback;
    double m_rtf { 0 };
    std::chrono::milliseconds m_frameTimeout { 0 };
    uint64_t m_hardFrameTimeoutMs { 0 };
    std::chrono::milliseconds m_checkInterval;
    std::atomic<int> m_audioDurationMs{ 0 };

    uint64_t m_firstByteTime { 0 };
    std::atomic<uint64_t> m_lastFrameTime { 0 };
    std::atomic<bool> m_stopped { true };
};


}}}} // Microsoft::CognitiveServices::Speech::Impl
