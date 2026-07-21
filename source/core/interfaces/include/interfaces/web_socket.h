//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include <memory>
#include <string>

#include "interfaces/base.h"
#include "interfaces/errors.h"
#include "interfaces/i_http_endpoint_info.h"
#include "interfaces/i_web_socket_message.h"
#include "interfaces/i_web_socket_state.h"
#include "util/event.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

    /// <summary>
    /// The interface that web socket implementations should implement
    /// </summary>
    SPX_INTERFACE(ISpxWebSocket)
    {
    public:
        /// <summary>
        /// Connects to the web socket connection
        /// </summary>
        /// <param name="params">The connection parameters to use</param>
        /// <param name="connectionId">(Optional) The connection ID associated with this web socket connection which is used for telemetry</param>
        virtual void Connect(const IHttpEndpointInfo& webSocketEndpoint, const std::string& connectionId = "") = 0;

        /// <summary>
        /// Disconnects the web socket connection
        /// </summary>
        virtual void Disconnect() = 0;

        /// <summary>
        /// Sends text data to the server
        /// </summary>
        /// <param name="text">The text data to send</param>
        virtual void SendTextData(const std::string& text) = 0;

        /// <summary>
        /// Sends binary data to the server
        /// </summary>
        /// <param name="data">The data to send</param>
        /// <param name="size">The size of the data to send</param>
        virtual void SendBinaryData(const uint8_t* data, const size_t size) = 0;

        /// <summary>
        /// Sends a web socket message to the server
        /// </summary>
        /// <param name="message">The message to send</param>
        virtual void SendData(const std::shared_ptr<IWebSocketMessage>& message) = 0;

        /// <summary>
        /// Gets the web socket connection state
        /// </summary>
        virtual WebSocketState GetState() const = 0;

        /// <summary>
        /// Checks if the web socket is connected. This will return if and only if the current
        /// state is Connected
        /// </summary>
        bool IsConnected() const
        {
            return GetState() == WebSocketState::CONNECTED;
        }

        /// <summary>
        /// Event raised when the web socket connects
        /// </summary>
        Event<const std::string&> OnConnected;

        /// <summary>
        /// Event raised when the socket is disconnected. The first parameter will be the reason
        /// we were disconnected. The second will either be the message the server sent to, or
        /// an internally generated message in the case of errors. The third parameter is set
        /// to true if the server requested the web socket be disconnected.
        /// </summary>
        Event<WebSocketDisconnectReason, const std::string&, bool> OnDisconnected;

        /// <summary>
        /// Event raised when we receive text data from the server
        /// </summary>
        Event<const std::string&> OnTextData;

        /// <summary>
        /// Event raised when we receive binary data from the server. The first parameter will
        /// be the pointer to the data, the second will be the size
        /// </summary>
        Event<const uint8_t*, const size_t> OnBinaryData;

        /// <summary>
        /// Event raised when we encounter an error.
        /// </summary>
        Event<const std::shared_ptr<ISpxErrorInformation>&> OnError;

        /// <summary>
        /// Event raised periodically when the web socket computes the average upload rate. Please note that
        /// this is the average upload rate of a moving window, and not the overall upload rate since the
        /// start of the connection. As such you will need to take into account previous values raised to
        /// get a clear picture of the overall web socket upload rate
        /// </summary>
        Event<const float> OnEstimatedUploadRateKBPerSec;
    };

}}}}
