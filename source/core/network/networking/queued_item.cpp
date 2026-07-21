// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.


#include "stdafx.h"
#include "queued_item.h"
#include "time_utils.h"

namespace
{
    using namespace Microsoft::CognitiveServices::Speech::Impl;

    constexpr auto DEFAULT_FRAME_TYPE = WebSocketFrameType::Unknown;
    constexpr auto DEFAULT_DISCONNECT = WebSocketDisconnectReason::Unknown;
}

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

    QueuedItem::QueuedItem() :
        queuedTime{ std::chrono::steady_clock::now() },
        utcTimeStamp{ PAL::GetUtcTimestamp() }
    {}

    OutgoingQueuedItem::OutgoingQueuedItem() :
        m_sentOrFailed{ false },
        m_message{}
    {}

    OutgoingQueuedItem::OutgoingQueuedItem(const IWebSocketMessage::Ptr& msg) :
        m_sentOrFailed{ false },
        m_message(msg)
    {
        SPX_THROW_HR_IF(SPXERR_INVALID_ARG, msg == nullptr);
        queuedTime = msg->CreationTime();
    }

    OutgoingQueuedItem::~OutgoingQueuedItem()
    {
        if (m_message && !m_sentOrFailed)
        {
            // For performance reasons, we skip collecting the call stack here and just set it to empty
            // Otherwise this may introduce longer delays if we are e.g. destroying a queue that has
            // many unsent items
            ExceptionWithCallStack ex(SPXERR_NETWORK_SEND_FAILED, "Message was not sent", "");
            TrySetFailed(std::make_exception_ptr(ex));
        }
    }

    IWebSocketMessage::Ptr OutgoingQueuedItem::Message()
    {
        return m_message;
    }

    bool OutgoingQueuedItem::TrySetComplete()
    {
        if (m_sentOrFailed)
        {
            return false;
        }

        m_sentOrFailed = true;
        if (m_message)
        {
            m_message->SetMessageSendSucceeded();
        }

        return true;
    }

    bool OutgoingQueuedItem::TrySetFailed(std::exception_ptr exception)
    {
        if (m_sentOrFailed)
        {
            return false;
        }

        m_sentOrFailed = true;
        if (m_message)
        {
            m_message->SetMessageSendFailed(exception);
        }

        return true;
    }
    
    IncomingQueuedItem::IncomingQueuedItem() :
        type{ DEFAULT_FRAME_TYPE },
        binaryData{},
        textData{},
        disconnectReason{ DEFAULT_DISCONNECT }
    {
    }
    
    IncomingQueuedItem::IncomingQueuedItem(const std::string& text) :
        type{ WebSocketFrameType::Text },
        binaryData{ },
        textData{ text },
        disconnectReason{ DEFAULT_DISCONNECT }
    {
    }
    
    IncomingQueuedItem::IncomingQueuedItem(std::vector<uint8_t>&& binary) :
        type{ WebSocketFrameType::Binary },
        binaryData{ std::move(binary) },
        textData{ },
        disconnectReason{ DEFAULT_DISCONNECT }
    {
    }
    
    IncomingQueuedItem::IncomingQueuedItem(WebSocketDisconnectReason reason, const std::string& message) :
        type{ WebSocketFrameType::Close },
        binaryData{ },
        textData{ message },
        disconnectReason{ reason }
    {
    }

}}}}
