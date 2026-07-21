//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#pragma once

#include <functional>
#include <string>

#include "interfaces/base.h"
#include "interfaces/ispx_telemetry_base.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

/**
 * The PTELEMETRY_WRITE type represents an application-defined
 * callback function used to handle raw telemetry data.
 * @param data The telemetry string data to write
 * @param requestId The request ID for the current turn
 */
using PTELEMETRY_WRITE = const std::function<void(std::string&& data, const std::string& requestId)>;

/**
 * The interface to implement to handle the sending of telemetry events
 */
SPX_INTERFACE(ISpxWebSocketTelemetry), public ISpxTelemetryBase
{
public:
    using ISpxInterfaceBaseFor<ISpxWebSocketTelemetry>::shared_from_this;
    using ISpxInterfaceBaseFor<ISpxWebSocketTelemetry>::Ptr;

    /**
     * Destructor
     */
    virtual ~ISpxWebSocketTelemetry() = default;

    /**
     * Flushes any outstanding telemetry events. Also flushes the telemetry associated with a given requestId.
     * @param requestId used to specify what telemetry to flush
     */
    virtual void Flush(const std::string& requestId, PTELEMETRY_WRITE callback) = 0;

    /**
     * Timestamps and records telemetry event.
     * @param requestId The requestId associated with the current event.
     * @param eventName The name of the event to be recorded.
     * @param id The unique id of the event to be recorded.
     * @param key The key of the JSON element to be recorded.
     * @param value The value of the JSON element to be recorded.
     */
    virtual void InbandEventTimestampPopulate(const std::string& requestId, const std::string& eventName, const std::string& id, const std::string& key) = 0;

    /**
     *  Records connection telemetry event.
     * @param eventName The name of the event to be recorded.
     * @param id The unique id of the event to be recorded.
     * @param key The key of the JSON element to be recorded.
     * @param value The value of the JSON element to be recorded.
     */
    virtual void InbandConnectionTelemetry(const std::string& id, const std::string& key, const std::string &value) = 0;

    /**
     * Received metric event population function.
     * @param requestId The requestId associated with the received message.
     * @param receivedMsg The name of the received event from service.
     */
    virtual void RecordReceivedMsg(const std::string& requestId, const std::string& receivedMsg) = 0;

    /**
     * Result latency population function.
     * @param requestId The requestId associated with the received message.
     * @param latencyInTicks The latency value in ticks.
     * @param isPhraseLatency If it is true, the latency is for phrase result. Otherwise it is for hypothesis result.
     */
    virtual void RecordResultLatency(const std::string& requestId, uint64_t latencyInTicks, bool isPhraseLatency, bool isFirstHypothesisLatency) = 0;

    /**
     * Handles the necessary changes for a requestId change event.
     * @param requestId the currently active requestId.
     */
    virtual void RegisterNewRequestId(const std::string& requestId) = 0;
};

} } } }
