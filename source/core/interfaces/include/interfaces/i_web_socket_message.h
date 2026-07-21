//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include <memory>
#include <chrono>
#include <exception>
#include <cstdint>
#include <interfaces/base.h>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

    /// <summary>
    /// The frame type of the web socket message
    /// </summary>
    enum class WebSocketFrameType
    {
        /// <summary>
        /// Unknown type of web socket message
        /// </summary>
        Unknown = 0,

        /// <summary>
        /// A text web socket message
        /// </summary>
        Text = 1,

        /// <summary>
        /// A binary web socket message
        /// </summary>
        Binary = 2,

        /// <summary>
        /// A close web socket message (internal use only for now)
        /// </summary>
        Close = 3
    };

    /// <summary>
    /// The interface for web socket messages
    /// </summary>
    SPX_INTERFACE(IWebSocketMessage)
    {
    public:
        /// <summary>
        /// Constructor
        /// </summary>
        IWebSocketMessage() = default;

        /// <summary>
        /// Destructor
        /// </summary>
        virtual ~IWebSocketMessage() = default;

        /// <summary>
        /// Gets the telemetry type of the web socket message
        /// </summary>
        /// <returns>The message type</returns>
        virtual int8_t MetricMessageType() const = 0;

        /// <summary>
        /// The type of web socket frame (e.g. text, or binary)
        /// </summary>
        /// <returns>The web socket frame type</returns>
        virtual WebSocketFrameType FrameType() const = 0;

        /// <summary>
        /// The total size in bytes of the message
        /// </summary>
        /// <returns>The message size in bytes</returns>
        virtual size_t Size() const = 0;

        /// <summary>
        /// Serializes this web socket message. This will also attempt to use some optimizations
        /// to reduce the number of copies in memory
        /// </summary>
        /// <param name="buffer">The buffer to initialise and write to</param>
        /// <returns>The number of bytes written to the buffer</returns>
        virtual size_t Serialize(std::shared_ptr<uint8_t>& buffer) = 0;

        /// <summary>
        /// Serializes this web socket message
        /// </summary>
        /// <param name="buffer">The buffer to write</param>
        /// <param name="size">The size of the buffer</param>
        /// <returns>The number of bytes written</returns>
        virtual size_t Serialize(uint8_t * buffer, size_t size) = 0;

        /// <summary>
        /// Gets the description of this message to use when logging. This should be brief (e.g. path, and total size for USP messages)
        /// </summary>
        /// <returns>The description to use when logging</returns>
        virtual std::string LogDescription() const = 0;

        /// <summary>
        /// Sets that the message has been successfully sent
        /// </summary>
        virtual void SetMessageSendSucceeded() = 0;

        /// <summary>
        /// Sets that the message has failed to send
        /// </summary>
        /// <param>The exception when sending the message.</param>
        virtual void SetMessageSendFailed(std::exception_ptr eptr) = 0;

        /// <summary>
        /// Time the websocket message was created.
        /// </summary>
        /// <returns></returns>
        virtual std::chrono::steady_clock::time_point CreationTime() const = 0;
    };

}}}}
