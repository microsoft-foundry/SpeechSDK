//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"
#include "web_socket_enum_helpers.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

    template<>
    const char* EnumHelpers::ToString(WebSocketError value)
    {
        switch (value)
        {
        case WebSocketError::CONNECTION_FAILURE:   return "CONNECTION_FAILURE";
        case WebSocketError::DNS_FAILURE:          return "DNS_FAILURE";
        case WebSocketError::REMOTE_CLOSED:        return "REMOTE_CLOSED";
        case WebSocketError::UNKNOWN:              return "UNKNOWN";
        case WebSocketError::WEBSOCKET_ERROR:      return "WEBSOCKET_ERROR";
        case WebSocketError::WEBSOCKET_SEND_FRAME: return "WEBSOCKET_SEND_FRAME";
        case WebSocketError::WEBSOCKET_UPGRADE:    return "WEBSOCKET_UPGRADE";
        }

        return nullptr;
    }

    template<>
    bool EnumHelpers::TryParse(const char* string, WebSocketError& value)
    {
        ENUM_PARSE(WebSocketError::CONNECTION_FAILURE, "CONNECTION_FAILURE");
        ENUM_PARSE(WebSocketError::DNS_FAILURE, "DNS_FAILURE");
        ENUM_PARSE(WebSocketError::REMOTE_CLOSED, "REMOTE_CLOSED");
        ENUM_PARSE(WebSocketError::UNKNOWN, "UNKNOWN");
        ENUM_PARSE(WebSocketError::WEBSOCKET_ERROR, "WEBSOCKET_ERROR");
        ENUM_PARSE(WebSocketError::WEBSOCKET_SEND_FRAME, "WEBSOCKET_SEND_FRAME");
        ENUM_PARSE(WebSocketError::WEBSOCKET_UPGRADE, "WEBSOCKET_UPGRADE");
        return false;
    }

    template<>
    const char* EnumHelpers::ToString(WebSocketState value)
    {
        switch (value)
        {
        case WebSocketState::CLOSED:     return "CLOSED";
        case WebSocketState::CONNECTED:  return "CONNECTED";
        case WebSocketState::DESTROYING: return "DESTROYING";
        case WebSocketState::INITIAL:    return "INITIAL";
        case WebSocketState::OPENING:    return "OPENING";
        }

        return nullptr;
    }

    template<>
    bool EnumHelpers::TryParse(const char* string, WebSocketState& value)
    {
        ENUM_PARSE(WebSocketState::CLOSED, "CLOSED");
        ENUM_PARSE(WebSocketState::CONNECTED, "CONNECTED");
        ENUM_PARSE(WebSocketState::DESTROYING, "DESTROYING");
        ENUM_PARSE(WebSocketState::INITIAL, "INITIAL");
        ENUM_PARSE(WebSocketState::OPENING, "OPENING");

        return false;
    }

    template<>
    const char* EnumHelpers::ToString(WebSocketDisconnectReason value)
    {
        switch (value)
        {
        case WebSocketDisconnectReason::CannotAcceptDataType: return "CannotAcceptDataType";
        case WebSocketDisconnectReason::EndpointUnavailable:  return "EndpointUnavailable";
        case WebSocketDisconnectReason::InternalServerError:  return "InternalServerError";
        case WebSocketDisconnectReason::InvalidPayloadData:   return "InvalidPayloadData";
        case WebSocketDisconnectReason::MessageTooBig:        return "MessageTooBig";
        case WebSocketDisconnectReason::Normal:               return "Normal";
        case WebSocketDisconnectReason::PolicyViolation:      return "PolicyViolation";
        case WebSocketDisconnectReason::ProtocolError:        return "ProtocolError";
        case WebSocketDisconnectReason::RequestThrottled:     return "RequestThrottled";
        case WebSocketDisconnectReason::ResourceExhausted:    return "ResourceExhausted";
        case WebSocketDisconnectReason::UnexpectedCondition:  return "UnexpectedCondition";
        case WebSocketDisconnectReason::Unknown:              return "Unknown";
        case WebSocketDisconnectReason::UnsupportedLocale:    return "UnsupportedLocale";
        }

        return nullptr;
    }

    template<>
    const char* EnumHelpers::ToString<WebSocketFrameType>(WebSocketFrameType type)
    {
        switch (type)
        {
        case WebSocketFrameType::Binary:  return "Binary";
        case WebSocketFrameType::Close:   return "Close";
        case WebSocketFrameType::Text:    return "Text";
        case WebSocketFrameType::Unknown: return "Unknown";
        }

        return nullptr;
    }

}}}}
