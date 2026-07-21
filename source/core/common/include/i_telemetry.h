//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// i_telemetry.h: Interfaces and types for telemetry
//

#pragma once

#include <string>
#include <metric_message_type.h>
#include "interfaces/network/web_socket_telemetry.h"
#include "ajv.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace USP {

enum IncomingMsgType
{
    turnStart,
    turnEnd,
    speechStartDetected,
    speechEndDetected,
    speechHypothesis,
    speechTentativePhrase,
    speechFragment,
    speechPhrase,
    translationHypothesis,
    translationPhrase,
    translationSynthesis,
    translationSynthesisEnd,
    audio,
    audioMetadata,
    response,
    audioStart,
    audioEnd,
    translationResponse,
    countOfMsgTypes
};

// errors that aren't the same as the 0x8XXXXXXX SDK errors


namespace event
{
    namespace keys
    {
        // Top level Array keys for events
        namespace array
        {
            constexpr auto ReceivedMessages = "ReceivedMessages";
            constexpr auto Metrics = "Metrics";
        }

        // Received Message event Keys
        namespace received
        {
            constexpr auto Audio = "audio";
            constexpr auto AudioMetadata = "audio.metadata";
            constexpr auto Response = "response";
        }

        constexpr auto Value = "value";
        constexpr auto EventType = "EventType";
        constexpr auto Name = "name";
        constexpr auto Start = "Start";
        constexpr auto End = "End";
        constexpr auto DeviceId = "DeviceId";
        constexpr auto Id = "Id";
        constexpr auto Memory = "Memory";
        constexpr auto CPU = "CPU";
        constexpr auto Error = "Error";
        constexpr auto Status = "Status";
    }

    // Metric event name keys
    namespace name
    {
        constexpr auto AudioPlayback = "audio:playback";
        constexpr auto AudioStart = "AudioStart";
        constexpr auto Microphone = "Microphone";
        constexpr auto ListeningTrigger = "ListeningTrigger";
        constexpr auto Connection = "Connection";
        constexpr auto Device = "device";
        constexpr auto Notification = "notification";
        constexpr auto SDK = "sdk";
        constexpr auto PhraseLatency = "PhraseLatencyMs";
        constexpr auto FirstHypothesisLatency = "FirstHypothesisLatencyMs";
        constexpr auto HypothesisLatency = "HypothesisLatencyMs";
    }
}

constexpr size_t NO_DASH_UUID_LEN = 37;
constexpr size_t TIME_STRING_MAX_SIZE = 30;

/**
 * Returns the current time in ISO8601 format
 * @param buffer char buffer to hold the ISO8601 time string
 * @param bufferLength the length of the buffer to be used to store the string
 * @return 0 in case of success and -1 in case of failure
 */
int GetISO8601Time(char *buffer, unsigned int bufferLength);

using namespace Microsoft::CognitiveServices::Speech::Impl;

// Received the specified message from the service.
inline void MetricsReceivedMessage(const ISpxWebSocketTelemetry::Ptr& telemetry, const std::string& requestId, const std::string& messagePath)
{
    telemetry->RecordReceivedMsg(requestId, messagePath);
}

// Metric Events defined in telemetry spec
inline void MetricsAudioStart(const ISpxWebSocketTelemetry::Ptr& telemetry, const std::string& requestId)
{
    telemetry->InbandEventTimestampPopulate(requestId, Speech::USP::event::name::Microphone, std::string{}, Speech::USP::event::keys::Start);
}

inline void MetricsAudioEnd(const ISpxWebSocketTelemetry::Ptr& telemetry, const std::string& requestId)
{
    telemetry->InbandEventTimestampPopulate(requestId, Speech::USP::event::name::Microphone, std::string{}, Speech::USP::event::keys::End);
}

inline void MetricsResultLatency(const ISpxWebSocketTelemetry::Ptr& telemetry, const std::string& requestId, uint64_t latencyInTicks, bool isPhraseLatency, bool isFirstHypothesisLatency)
{
    telemetry->RecordResultLatency(requestId, latencyInTicks, isPhraseLatency, isFirstHypothesisLatency);
}

inline void MetricsTransportStart(const ISpxWebSocketTelemetry::Ptr& telemetry, const std::string& connectionId)
{
    telemetry->InbandConnectionTelemetry(connectionId, Speech::USP::event::keys::Start, "");
}

inline void MetricsDeviceStartup(const ISpxWebSocketTelemetry::Ptr& telemetry, const std::string& connectionId, const std::string& deviceId)
{
    telemetry->InbandConnectionTelemetry(connectionId, Speech::USP::event::keys::DeviceId, deviceId);
}

inline void MetricsTransportConnected(const ISpxWebSocketTelemetry::Ptr& telemetry, const std::string& connectionId)
{
    telemetry->InbandConnectionTelemetry(connectionId, Speech::USP::event::keys::End, "");
}

inline void MetricsTransportError(const ISpxWebSocketTelemetry::Ptr& telemetry, const std::string& connectionId, double error)
{
    telemetry->InbandConnectionTelemetry(connectionId, Speech::USP::event::keys::Error, std::to_string(error));
}

// Transport metrics
/* The transport has started a state transition. */
template<typename T>
inline void MetricsTransportStateStart(T/* state*/)
{
    /* Implementation was a convoluted NoOp, leaving here to discuss the possibility of rewiring in the future (to one DS)*/
}

/* The transport has completed a state transition. */
template<typename T>
inline void MetricsTransportStateEnd(T/* state*/)
{
    /* Implementation was a convoluted NoOp, leaving here to discuss the possibility of rewiring in the future (to one DS)*/
}

/* Client request identifier for the current turn */
inline void MetricsTransportRequestId(ISpxWebSocketTelemetry* handle, const std::string& requestId)
{
    handle->RegisterNewRequestId(requestId);
}

inline void MetricsTransportCancelled()
{
    MetricsTransportStateEnd(MetricMessageType::METRIC_TRANSPORT_STATE_CANCELLED);
}

/* The transport connection was closed. */
inline void MetricsTransportClosed()
{
    MetricsTransportStateEnd(MetricMessageType::METRIC_TRANSPORT_STATE_CLOSED);
}

/* The transport connection was dropped. */
inline void MetricsTransportDropped()
{
    MetricsTransportStateEnd(MetricMessageType::METRIC_TRANSPORT_STATE_DROPPED);
}

/* The transport connection was reset.  We do this when the token changes
 * during the connection. */
inline void MetricsTransportReset()
{
    MetricsTransportStateEnd(MetricMessageType::METRIC_TRANSPORT_STATE_RESET);
}

/* The transport has failed to parse a response. */
inline void MetricsTransportParsingError()
{
    /* Implementation was a convoluted NoOp, leaving here to discuss the possibility of rewiring in the future (maybe to 1DS?)*/
}

/* The transport has dropped a packet because it was in a closed state. */
inline void MetricsTransportInvalidStateError()
{
    /* Implementation was a convoluted NoOp, leaving here to discuss the possibility of rewiring in the future (maybe to 1DS?)*/
}

/* Web socket response contained a request ID different from our last request */
inline void MetricsUnexpectedRequestId(const std::string&)
{
    /* Implementation was a convoluted NoOp, leaving here to discuss the possibility of rewiring in the future (maybe to 1DS?)*/
}

/* audio stream events*/
inline void MetricsAudioStreamInit()
{
    /* Implementation was a convoluted NoOp, leaving here to discuss the possibility of rewiring in the future (maybe to 1DS?)*/
}

inline void MetricsAudioStreamFlush()
{
    /* Implementation was a convoluted NoOp, leaving here to discuss the possibility of rewiring in the future (maybe to 1DS?)*/
}

template<typename T>
inline void MetricsAudioStreamData(T /*__i0*/)
{
    /* Implementation was a convoluted NoOp, leaving here to discuss the possibility of rewiring in the future (maybe to 1DS?)*/
}

} } } }
