//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include <cstdint>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace USP {

/// <summary>
/// Enumeration of message types
/// </summary>
enum class MetricMessageType : int8_t
{
    INTERNAL_TRANSPORT_ERROR_PARSEERROR = -1,
    INTERNAL_TRANSPORT_UNHANDLEDRESPONSE = -2,
    INTERNAL_TRANSPORT_INVALIDSTATE = -3,
    INTERNAL_CORTANA_EVENT_COALESCED = -4,
    METRIC_MESSAGE_TYPE_INVALID = 0,
    METRIC_MESSAGE_TYPE_DEVICECONTEXT = 1,
    METRIC_MESSAGE_TYPE_AUDIO_START = 2,
    METRIC_MESSAGE_TYPE_AUDIO_LAST = 3,
    METRIC_MESSAGE_TYPE_TELEMETRY = 4,
    METRIC_TRANSPORT_STATE_DNS = 5,
    METRIC_TRANSPORT_STATE_DROPPED = 6,
    METRIC_TRANSPORT_STATE_CLOSED = 7,
    METRIC_TRANSPORT_STATE_CANCELLED = 8,
    METRIC_TRANSPORT_STATE_RESET = 9
};

} } } }
