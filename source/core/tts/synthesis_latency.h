//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once
#include "ispxinterfaces.h"
#include "interface_helpers.h"
#include "named_properties.h"
#include "time_utils.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

/// <summary>
/// Defines the latency types in speech synthesis.
/// </summary>
enum class SynthesisLatencyType
{
    /// <summary>
    /// None
    /// </summary>
    None = 0,

    /// <summary>
    /// The first byte latency, from synthesis started to the first audio chunk received.
    /// </summary>
    FirstByteLatency,

    /// <summary>
    /// The finish latency, from synthesis started to synthesis finished.
    /// </summary>
    FinishLatency,

    /// <summary>
    /// The time to establish the HTTP/WebSocket connection.
    /// If the connection is reused, this latency is 0.
    /// </summary>
    ConnectionLatency,

    /// <summary>
    /// The time to send the HTTP request and receive the response.
    /// Roughly this is the round trip time of the network.
    /// </summary>
    NetworkLatency,

    /// <summary>
    /// The time from the service receives the request to the service sends back the first audio chunk.
    /// </summary>
    ServiceLatency,

    /// <summary>
    /// Count of this enum class.
    /// </summary>
    Count,
};

class SynthesisLatency
{
private:
    std::array<int, static_cast<size_t>(SynthesisLatencyType::Count)> m_latencies;
    uint64_t m_synthesisStartedTime;

public:
    SynthesisLatency() = default;

    SynthesisLatency(const SynthesisLatency&) = delete;

    SynthesisLatency(SynthesisLatency&&) = delete;

    SynthesisLatency& operator=(const SynthesisLatency&) = delete;


    /// <summary>
    /// Gets the latency of the specified type.
    /// </summary>
    /// <param name="type">The latency type.</param>
    /// <returns>The latency in milliseconds.</returns>
    int GetLatency(SynthesisLatencyType type) const
    {
        return m_latencies[static_cast<size_t>(type)];
    }

    /// <summary>
    /// Sets the latency of the specified type.
    /// </summary>
    /// <param name="type">The latency type.</param>
    /// <param name="latency">The latency in milliseconds.</param>
    void SetLatency(SynthesisLatencyType type, int latency)
    {
        m_latencies[static_cast<size_t>(type)] = latency;
    }

    /// <summary>
    /// Sets the latency information to a property bag.
    /// </summary>
    void SetLatencyProperties(std::shared_ptr<ISpxNamedProperties> resultProperties) const
    {
        resultProperties->Set(PropertyId::SpeechServiceResponse_SynthesisFirstByteLatencyMs, GetLatency(SynthesisLatencyType::FirstByteLatency));
        resultProperties->Set(
            PropertyId::SpeechServiceResponse_SynthesisFinishLatencyMs,
            std::to_string(GetLatency(SynthesisLatencyType::FinishLatency)));
        resultProperties->Set(
            PropertyId::SpeechServiceResponse_SynthesisConnectionLatencyMs,
            std::to_string(GetLatency(SynthesisLatencyType::ConnectionLatency)));
        resultProperties->Set(
            PropertyId::SpeechServiceResponse_SynthesisNetworkLatencyMs,
            std::to_string(GetLatency(SynthesisLatencyType::NetworkLatency)));
        resultProperties->Set(
            PropertyId::SpeechServiceResponse_SynthesisServiceLatencyMs,
            std::to_string(GetLatency(SynthesisLatencyType::ServiceLatency)));
    }

    void OnSynthesisStarted()
    {
        m_latencies.fill(-1);
        m_synthesisStartedTime = PAL::GetMillisecondsSinceEpoch();
    }

    void OnAudioReceived()
    {
        if (GetLatency(SynthesisLatencyType::FirstByteLatency) < 0)
        {
            SetLatency(SynthesisLatencyType::FirstByteLatency, static_cast<int>(PAL::GetMillisecondsSinceEpoch() - m_synthesisStartedTime));

            if (GetLatency(SynthesisLatencyType::NetworkLatency) > 0)
            {
                SetLatency(SynthesisLatencyType::ServiceLatency, GetLatency(SynthesisLatencyType::FirstByteLatency) - GetLatency(SynthesisLatencyType::ConnectionLatency) - GetLatency(SynthesisLatencyType::NetworkLatency));
            }
        }
    }

    void OnAudioFinished()
    {
        SetLatency(SynthesisLatencyType::FinishLatency, static_cast<int>(PAL::GetMillisecondsSinceEpoch() - m_synthesisStartedTime));
    }

    void OnConnected()
    {
        SetLatency(SynthesisLatencyType::ConnectionLatency, static_cast<int>(PAL::GetMillisecondsSinceEpoch() - m_synthesisStartedTime));
    }

    void OnServiceEchoReceived()
    {
        if (GetLatency(SynthesisLatencyType::ConnectionLatency) < 0)
        {
            SetLatency(SynthesisLatencyType::ConnectionLatency, 0);
        }

        SetLatency(SynthesisLatencyType::NetworkLatency, static_cast<int>(PAL::GetMillisecondsSinceEpoch() - m_synthesisStartedTime) - GetLatency(SynthesisLatencyType::ConnectionLatency));
    }
};

} } } } // Microsoft::CognitiveServices::Speech::Impl
