//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// usb_web_socket.h: defines the USP web socket connection.
//

#pragma once

#include <memory>
#include <map>
#include <atomic>
#include "interfaces/types.h"
#include "interfaces/web_socket.h"
#include "usp_message.h"
#include "i_telemetry.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace USP {

    typedef std::map<std::string, std::string> UspHeaders;

    /// <summary>
    /// Represents a USP protocol based web socket connection
    /// </summary>
    class UspWebSocket : public std::enable_shared_from_this<UspWebSocket>
    {
    public:
        /// <summary>
        /// Creates a new instance of the USP web socket
        /// </summary>
        /// <param name="threadService">The thread service implementation to use</param>
        /// <param name="affinity">The thread affinity to use when scheduling work on the thread service</param>
        /// <param name="pollingIntervalMs">How often to poll for new incoming messages and/or send outgoing queued messages</param>
        /// <param name="telemetry">The telemetry instance to use</param>
        /// <returns>The USP web socket instance</returns>
        static std::shared_ptr<UspWebSocket> Create(
            Impl::ISpxThreadService::Ptr threadService,
            Impl::ISpxThreadService::Affinity affinity,
            const std::chrono::milliseconds& pollingIntervalMs,
            ISpxWebSocketTelemetry::Ptr telemetry,
            ISpxGenericSite::Ptr site);

        /// <summary>
        /// Virtual destructor
        /// </summary>
        virtual ~UspWebSocket() = default;

        /// <summary>
        /// Connects to the web socket connection
        /// </summary>
        /// <param name="params">The connection parameters to use</param>
        /// <param name="connectionId">(Optional) The connection ID associated with this web socket connection which is used for telemetry</param>
        void Connect(const Impl::IHttpEndpointInfo& webSocketEndpoint, const std::string& connectionId = "");

        /// <summary>
        /// Disconnects the web socket connection
        /// </summary>
        void Disconnect();

        /// <summary>
        /// Sends audio data to the USP service
        /// </summary>
        /// <param name="path">The path to set on the USP message (usually "audio")</param>
        /// <param name="audioChunk">The binary audio chunk to send</param>
        /// <param name="requestId">The request ID to include with the binary USP message</param>
        /// <param name="newStream">Set this to true if you are starting a new audio stream</param>
        void SendAudioData(const std::string& path, const Impl::DataChunkPtr& audioChunk, const std::string& requestId = "", bool newStream = false);

        /// <summary>
        /// Sends a USP telemetry message
        /// </summary>
        /// <param name="data">The JSON telemetry data to send</param>
        /// <param name="requestId">The request ID to use</param>
        void SendTelemetryData(std::string&& data, const std::string& requestId);

        /// <summary>
        /// Sends a generic USP message to the server
        /// </summary>
        /// <param name="message">The USP message to send</param>
        virtual void SendData(const std::shared_ptr<USP::Message>& message);

        /// <summary>
        /// Sets the polling interval for the underlying web socket. This can be used to adjust
        /// the polling interval after connection (e.g., switching from a fast connect-time
        /// interval to a slower run-time interval).
        /// </summary>
        /// <param name="pollingIntervalMs">The new polling interval</param>
        void SetPollingInterval(const std::chrono::milliseconds& pollingIntervalMs);

        /// <summary>
        /// Event raised when the web socket connects
        /// </summary>
        Impl::Event<const std::string&> OnConnected;

        /// <summary>
        /// Event raised when the socket is disconnected. The first parameter will be the reason
        /// we were disconnected. The second will either be the message the server sent to, or
        /// an internally generated message in the case of errors. The third parameter is set
        /// to true if the server requested the web socket be disconnected.
        /// </summary>
        Impl::Event<Impl::WebSocketDisconnectReason, const std::string&, bool> OnDisconnected;

        /// <summary>
        /// Event raised when we encounter an error.
        /// </summary>
        Impl::Event<const std::shared_ptr<Impl::ISpxErrorInformation>&> OnError;

        /// <summary>
        /// Event raised when a text USP message is received from the service
        /// </summary>
        Impl::Event<const UspHeaders&, const std::string&> OnUspTextData;

        /// <summary>
        /// Event raised when a binary USP message is received from the service
        /// </summary>
        Impl::Event<const UspHeaders&, const uint8_t*, size_t> OnUspBinaryData;

        /// <summary>
        /// Event raised periodically when the web socket computes the average upload rate. Please note that
        /// this is the average upload rate of a moving window, and not the overall upload rate since the
        /// start of the connection. As such you will need to take into account previous values raised to
        /// get a clear picture of the overall web socket upload rate
        /// </summary>
        Impl::Event<const float> OnEstimatedUploadRateKBPerSec;

    private:
        DISABLE_DEFAULT_CTORS(UspWebSocket);
        UspWebSocket(std::shared_ptr<Impl::ISpxWebSocket> webSocket);

        void HandleConnected(const std::string& url);
        void HandleDisconnected(Impl::WebSocketDisconnectReason reason, const std::string& message, bool serverRequested);
        void HandleError(const std::shared_ptr<Impl::ISpxErrorInformation>& error);
        void HandleTextData(const std::string& data);
        void HandleBinaryData(const uint8_t* data, const size_t);
        void HandleEstimatedUploadRateComputed(const float uploadRate);

    private:
        std::atomic_bool m_chunkSent;
        std::atomic<std::uint32_t> m_streamId;
        std::shared_ptr<Impl::ISpxWebSocket> m_webSocket;
    };

} } } }
