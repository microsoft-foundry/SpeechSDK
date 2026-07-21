//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"
#include "web_socket_message.h"
#include "time_utils.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

    WebSocketMessage::WebSocketMessage(size_t size, WebSocketFrameType frameType, Speech::USP::MetricMessageType metricType) :
        m_metricType(metricType),
        m_frameType(frameType),
        m_size(size),
        m_buffer(new uint8_t[size], std::default_delete<uint8_t[]>()),
        m_promise()
    {
    }

    WebSocketMessage::WebSocketMessage(const std::string& data, Speech::USP::MetricMessageType metricType) :
        WebSocketMessage(data.length(), WebSocketFrameType::Text, metricType)
    {
        // CAUTION: copied string will **NOT** be null terminated
        memcpy(m_buffer.get(), data.c_str(), m_size);
    }

    WebSocketMessage::WebSocketMessage(const uint8_t* data, const size_t size, Speech::USP::MetricMessageType metricType) :
        WebSocketMessage(size, WebSocketFrameType::Binary, metricType)
    {
        // we are sending on another thread. This means that we cannot guarantee that the pointer
        // will still be valid at the time we send so we create a copy here to be safe
        memcpy(m_buffer.get(), data, size);
    }

    int8_t WebSocketMessage::MetricMessageType() const { return static_cast<int8_t>(m_metricType); }
    WebSocketFrameType WebSocketMessage::FrameType() const { return m_frameType; }
    size_t WebSocketMessage::Size() const { return m_size; }

    size_t WebSocketMessage::Serialize(std::shared_ptr<uint8_t>& buffer)
    {
        buffer = m_buffer;
        return m_size;
    }

    size_t WebSocketMessage::Serialize(uint8_t* buffer, size_t size)
    {
        if (buffer == nullptr || size == 0)
        {
            return 0;
        }

        size_t toCopy = std::min(size, m_size);
        std::memcpy(buffer, m_buffer.get(), toCopy);

        return toCopy;
    }

    std::string WebSocketMessage::LogDescription() const
    {
        return std::string("IsBinary: ") + std::to_string(FrameType() == WebSocketFrameType::Binary)
            + ", Size: " + std::to_string(Size());
    }

    void WebSocketMessage::SetMessageSendSucceeded()
    {
        try
        {
            m_promise.set_value();
        }
        catch (const std::future_error&)
        {
            // ignore
        }
    }

    void WebSocketMessage::SetMessageSendFailed(std::exception_ptr eptr)
    {
        try
        {
            m_promise.set_exception(eptr);
        }
        catch (const std::future_error&)
        {
            // ignore
        }
    }

    std::future<void> WebSocketMessage::MessageSendFuture()
    {
        return m_promise.get_future();
    }

}}}}
