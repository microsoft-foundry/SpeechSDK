//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#pragma once

#include "usp_client_configuration.h"
#include "usp_message.h"
#include "interfaces/base.h"
#include "interfaces/utils.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

SPX_INTERFACE(ISpxUspConnection)
{
public:

    /**
    * Establishes a new connection to the service, launches a worker thread
    * to process incoming/outgoing requests.
    */
    virtual void Connect() = 0;

    virtual void SetConfiguration(const USP::ClientConfiguration& config) = 0;

    /**
    * Adds an audio segment to the outgoing queue.
    * @param audioChunk the audio chunk to be queued.
    */
    virtual void QueueAudioSegment(const DataChunkPtr & audioChunk) = 0;

    /**
    * Adds an empty audio segment to the outgoing queue, which serves as a signal to the service that the audio
    * stream has ended (i.e., has been uploaded completely).
    * @param uspHandle the UspHandle for sending the audio.
    */
    virtual void QueueAudioEnd() = 0;

    /**
    * Adds a message to the outgoing queue.
    * @param message The message being sent.
    */
    virtual void QueueMessage(std::unique_ptr<USP::Message> message) = 0;

    /**
    * Writes latency value into telemetry data.
    * @param latencyInTicks The latency value in ticks.
    * @param isPhraseLatency If it is true, the latency is for phrase result. Otherwise it is for hypothesis result.
    */
    virtual void WriteTelemetryLatency(uint64_t latencyInTicks, bool isPhraseLatency, bool isFirstHypothesisLatency) = 0; 

    /**
     * Flushes telemetry data.
     */
    virtual void FlushTelemetry() = 0;

    /**
    * Requests the connection to service to be shut down.
    * @param uspContext A pointer to the UspContext.
    */
    virtual void Shutdown() = 0;

    /**
    * Returns true if the status is connected.
    */
    virtual bool IsConnected() = 0;

    /**
    * Returns the URL used for connection.
    */
    virtual std::string GetConnectionUrl() = 0;
};

}}}}
