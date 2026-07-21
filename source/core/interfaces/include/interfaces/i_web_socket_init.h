//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include <memory>
#include <chrono>
#include "interfaces/base.h"
#include "interfaces/ispx_http_error_handler.h"
#include "ispxinterfaces.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

    /// <summary>
    /// Forward declaration of the telemetry interface
    /// </summary>
    class ISpxWebSocketTelemetry;

    /// <summary>
    /// The interface that web socket implementations should implement
    /// </summary>
    SPX_INTERFACE(ISpxWebSocketInit)
    {
    public:
        /// <summary>
        /// Initializes the web socket implementation
        /// </summary>
        /// <param name="threadService">The thread service to use to schedule the web socket send/receive on</param>
        /// <param name="affinity">The thread service affinity to use for sending/receiving</param>
        /// <param name="pollingIntervalMs">How often to poll for received messages and/or send any queued outgoing messages</param>
        /// <param name="telemetry">The telemetry implementation to use</param>
        /// <param name="httpErrorHandler">(Optional) The error handler to use when handling web socket connection errors</param>
        virtual void Init(
            const std::shared_ptr<ISpxThreadService>& threadService,
            const ISpxThreadService::Affinity affinity,
            const std::chrono::milliseconds& pollingIntervalMs,
            const std::shared_ptr<ISpxWebSocketTelemetry>& telemetry,
            const std::shared_ptr<ISpxHttpErrorHandler>& httpErrorHandler = nullptr) = 0;

        /// <summary>
        /// Sets the polling interval for the web socket work loop. This can be called after
        /// initialization to dynamically adjust the polling interval.
        /// </summary>
        /// <param name="pollingIntervalMs">The new polling interval in milliseconds</param>
        virtual void SetPollingInterval(const std::chrono::milliseconds& pollingIntervalMs) = 0;
    };

}}}}
