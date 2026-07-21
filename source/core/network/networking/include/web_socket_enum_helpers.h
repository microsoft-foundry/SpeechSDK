//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include "interfaces/enum_helpers.h"
#include "interfaces/i_web_socket_state.h"
#include "interfaces/i_web_socket_message.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

    /// <summary>
    /// Converts an enum value to its string representation
    /// </summary>
    /// <param name="value">The value to convert</param>
    /// <returns>The corresponding string for that value</returns>
    template<>
    const char* EnumHelpers::ToString<WebSocketError>(WebSocketError error);

    /// <summary>
    /// Converts a string representation of enum back into the enum
    /// </summary>
    /// <param name="string">The string to convert</param>
    /// <param name="value">The value to set</param>
    /// <returns>True if the string is a valid enum value, false otherwise</returns>
    template<>
    bool EnumHelpers::TryParse<WebSocketError>(const char* string, WebSocketError& value);

    /// <summary>
    /// Converts an enum value to its string representation
    /// </summary>
    /// <param name="value">The value to convert</param>
    /// <returns>The corresponding string for that value</returns>
    template<>
    const char* EnumHelpers::ToString<WebSocketState>(WebSocketState error);

    /// <summary>
    /// Converts a string representation of enum back into the enum
    /// </summary>
    /// <param name="string">The string to convert</param>
    /// <param name="value">The value to set</param>
    /// <returns>True if the string is a valid enum value, false otherwise</returns>
    template<>
    bool EnumHelpers::TryParse<WebSocketState>(const char* string, WebSocketState& value);

    /// <summary>
    /// Converts an enum value to its string representation
    /// </summary>
    /// <param name="value">The value to convert</param>
    /// <returns>The corresponding string for that value</returns>
    template<>
    const char* EnumHelpers::ToString<WebSocketDisconnectReason>(WebSocketDisconnectReason error);

    /// <summary>
    /// Converts an enum value to its string representation
    /// </summary>
    /// <param name="value">The value to convert</param>
    /// <returns>The corresponding string for that value</returns>
    template<>
    const char* EnumHelpers::ToString<WebSocketFrameType>(WebSocketFrameType type);

}}}}
