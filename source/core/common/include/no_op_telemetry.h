//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// no_op_telemetry.h: An implementation of the telemetry interface that does nothing
//

#pragma once

#include <i_telemetry.h>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace USP {

    /// <summary>
    /// Telemetry implementation that does nothing
    /// </summary>
    class NoOpTelemetry : public ISpxWebSocketTelemetry
    {
    public:
        /// <summary>
        /// Gets the singleton instance of the telemetry implementation that does nothing
        /// </summary>
        /// <returns>The singleton instance</returns>
        static std::shared_ptr<NoOpTelemetry> Instance()
        {
            static std::shared_ptr<NoOpTelemetry> _instance;
            if (_instance == nullptr)
            {
                _instance = std::make_shared<NoOpTelemetry>();
            }

            return _instance;
        }

        NoOpTelemetry() {}

        virtual void Flush(const std::string &, PTELEMETRY_WRITE) override {}
        virtual void InbandEventTimestampPopulate(const std::string &, const std::string &, const std::string &, const std::string &) override {}
        virtual void InbandConnectionTelemetry(const std::string &, const std::string &, const std::string&) override {}
        virtual void RecordReceivedMsg(const std::string &, const std::string &) override {}
        virtual void RecordResultLatency(const std::string &, uint64_t, bool, bool) override {}
        virtual void RegisterNewRequestId(const std::string &) override {}

    private:
        
        NoOpTelemetry(const NoOpTelemetry&) = delete;
        NoOpTelemetry& operator=(const NoOpTelemetry&) = delete;
        NoOpTelemetry(NoOpTelemetry&&) = delete;
        NoOpTelemetry& operator=(NoOpTelemetry&&) = delete;
    };

} } } }
