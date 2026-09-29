//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include <string>
#include <map>
#include <chrono>
#include <future>
#include <interfaces/i_web_socket_message.h>
#include <metric_message_type.h>
#include <interface_helpers.h>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace USP {

    enum class MessageType { Config, Context, Agent, AgentContext, SpeechEvent, Event, Ssml, Audio, Commit, Unknown };

    /// <summary>
    /// Base class representing all USP web socket messages
    /// </summary>
    class Message : public Impl::IWebSocketMessage
    {
    public:
        /// <summary>
        /// Class destructor
        /// </summary>
        virtual ~Message();

        SPX_INTERFACE_MAP_BEGIN()
            SPX_INTERFACE_MAP_ENTRY(Impl::IWebSocketMessage)
        SPX_INTERFACE_MAP_END()

        /// <summary>
        /// Sets the message path for this message
        /// </summary>
        /// <param name="path">The path to set</param>
        /// <returns>Reference to this instance</returns>
        Message& Path(const std::string& path);

        /// <summary>
        /// Gets the message path
        /// </summary>
        /// <returns>Message path</returns>
        std::string Path() const;

        /// <summary>
        /// Sets the UUID for this request. This should not have any dashes
        /// </summary>
        /// <param name="requestId">The request ID to set</param>
        /// <returns>Reference to this instance</returns>
        Message& RequestId(const std::string& requestId);

        /// <summary>
        /// Gets the request ID for this message
        /// </summary>
        /// <returns>Request ID</returns>
        std::string RequestId() const;

        /// <summary>
        /// Sets the timestamp of the message
        /// </summary>
        /// <param name="time">The timestamp to send</param>
        /// <returns>Reference to this instance</returns>
        Message& Timestamp(const std::chrono::system_clock::time_point& time);

        /// <summary>
        /// Gets the timestamp in local time
        /// </summary>
        /// <returns>The equivalent local time for the message timestamp</returns>
        std::chrono::system_clock::time_point Timestamp() const;

        /// <summary>
        /// Gets the creation time of the message
        /// </summary>
        /// <returns>This is useful for determining how long the message has been around, but not when it was created relative to external processes.</returns>
        std::chrono::steady_clock::time_point CreationTime() const override { return m_creationTime; };

        /// <summary>
        /// Gets the UTC timestamp string
        /// </summary>
        /// <returns>The UTC timestamp in ISO 8601 format</returns>
        std::string UtcTimestampAsString() const;

        /// <summary>
        /// Sets the message content type (e.g. audio/wav, application/json; charset=utf8)
        /// </summary>
        /// <param name="contentType">The content type to set</param>
        /// <returns>Reference to this instance</returns>
        Message& ContentType(const std::string& contentType);

        /// <summary>
        /// Gets the content type of the message
        /// </summary>
        /// <returns>The content type</returns>
        std::string ContentType() const;

        /// <summary>
        /// Sets a USP message header
        /// </summary>
        /// <param name="name">The header to set</param>
        /// <param name="value">The value to set the header to. If this is empty the header value will be deleted</param>
        /// <returns>Reference to this instance</returns>
        Message& SetHeader(const std::string& name, const std::string& value);

        /// <summary>
        /// Gets the value for a given header
        /// </summary>
        /// <param name="name">The name of the header to retrieve</param>
        /// <returns>The value for the header. If the header does not exist, you will get
        /// back an empty string</returns>
        std::string GetHeader(const std::string& name) const;

        /// <summary>
        /// Gets whether or not this is binary USP message
        /// </summary>
        /// <returns>True if this is a binary message, false otherwise</returns>
        bool IsBinary() const;

        /// <summary>
        /// Gets whether or not this binary message has data
        /// </summary>
        /// <returns>True if we have any data, false otherwise</returns>
        bool HasData() const;

        /// <summary>
        /// Computes the total buffer size in bytes needed to store this serialized USP message
        /// </summary>
        /// <returns>The total size needed in bytes</returns>
        size_t Size() const override;

        /// <summary>
        /// Sets the message type
        /// </summary>
        /// <param name="messageType">The message type</param>
        /// <returns>Reference to this instance</returns>
        Message& MessageType(USP::MessageType messageType);

        /// <summary>
        /// Gets the USP message type
        /// </summary>
        /// <returns>The message type</returns>
        USP::MessageType MessageType() const;

        /// <summary>
        /// Gets the telemetry type of the web socket message
        /// </summary>
        /// <returns>Reference to this instance</returns>
        Message& MetricMessageType(Speech::USP::MetricMessageType metricType);

        /// <summary>
        /// Gets the telemetry type of the web socket message
        /// </summary>
        /// <returns>The message type</returns>
        int8_t MetricMessageType() const override;

        /// <summary>
        /// The type of web socket frame (e.g. text, or binary)
        /// </summary>
        /// <returns>The web socket frame type</returns>
        Impl::WebSocketFrameType FrameType() const override;

        /// <summary>
        /// Serializes this USP message. This will also attempt to use some optimizations
        /// to reduce the number of copies in memory
        /// </summary>
        /// <param name="buffer">The buffer instantiate and serialize to</param>
        /// <returns>The number of bytes written</returns>
        size_t Serialize(std::shared_ptr<uint8_t>& buffer) override;

        /// <summary>
        /// Serializes this web socket message
        /// </summary>
        /// <param name="buffer">The buffer to write</param>
        /// <param name="size">The size of the buffer</param>
        /// <returns>The number of bytes written</returns>
        size_t Serialize(uint8_t* buffer, size_t size) override;

        /// <summary>
        /// Gets the description of this message to use when logging. This should be brief (e.g. path, and total size for USP messages)
        /// </summary>
        /// <returns>The description to use when logging</returns>
        std::string LogDescription() const override;

        /// <summary>
        /// Set a promise that will signal when the message is being sent.
        /// </summary>
        /// <param name="messageSentPromise">The promise from the caller</param>
        /// <returns>Reference to this instance</returns>
        Message& SetMessageSentPromise(const std::shared_ptr<std::promise<bool>>& messageSentPromise);

        /// <summary>
        /// Sets that the message has been successfully sent
        /// </summary>
        void SetMessageSendSucceeded() override;

        /// <summary>
        /// Sets that the message has failed to send
        /// </summary>
        /// <param>The exception when sending the message.</param>
        void SetMessageSendFailed(std::exception_ptr eptr) override;

    protected:
        Message(bool isBinary, const std::string& path, USP::MessageType messageType, const std::string& requestId);

        virtual size_t GetDataSize() const = 0;
        virtual size_t WriteData(uint8_t * buffer, const size_t bufferSize) const = 0;

        std::map<std::string, std::string> m_headers;

    private:
        // disallow copy constructors
        Message(Message&) = delete;

        bool m_isBinary = false;
        std::chrono::system_clock::time_point m_timestamp;
        std::chrono::steady_clock::time_point m_creationTime = std::chrono::steady_clock::now();
        Speech::USP::MessageType m_msgType;
        Speech::USP::MetricMessageType m_metricType;
        std::shared_ptr<std::promise<bool>> m_messageSent;
    };

}}}}
