// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#include <memory>
#include <atomic>
#include <future>
#include <chrono>
#include <exception>
#include <vector>
#include <string>

#include <interfaces/i_web_socket_message.h>
#include <interfaces/i_web_socket_state.h>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

    struct QueuedItem
    {
    public:
        QueuedItem();
        virtual ~QueuedItem() = default;

        QueuedItem(const QueuedItem&) = delete;
        QueuedItem& operator=(const QueuedItem&) = delete;

        QueuedItem(QueuedItem&&) = default;
        QueuedItem& operator=(QueuedItem&&) = default;

        std::chrono::steady_clock::time_point queuedTime;
        std::string utcTimeStamp;
    };

    struct OutgoingQueuedItem : public QueuedItem
    {
    private:
        bool m_sentOrFailed;
        IWebSocketMessage::Ptr m_message;

    public:
        OutgoingQueuedItem();
        explicit OutgoingQueuedItem(const IWebSocketMessage::Ptr& msg);

        OutgoingQueuedItem(OutgoingQueuedItem&&) = default;
        OutgoingQueuedItem& operator=(OutgoingQueuedItem&&) = default;

        virtual ~OutgoingQueuedItem();

        IWebSocketMessage::Ptr Message();

        bool TrySetComplete();
        bool TrySetFailed(std::exception_ptr exception);
    };

    struct IncomingQueuedItem : public QueuedItem
    {
        IncomingQueuedItem();
        IncomingQueuedItem(const std::string& text);
        IncomingQueuedItem(std::vector<uint8_t>&& binary);
        IncomingQueuedItem(WebSocketDisconnectReason reason, const std::string& message);

        WebSocketFrameType type;
        std::vector<uint8_t> binaryData;
        std::string textData;
        WebSocketDisconnectReason disconnectReason;
    };
    
}}}}

