//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include <memory>
#include <chrono>
#include <interfaces/i_web_socket_message.h>
#include <metric_message_type.h>
#include <interface_helpers.h>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

    /// <summary>
    /// Helper class to wrap text, or binary data as a web socket message
    /// </summary>
    class WebSocketMessage : public IWebSocketMessage
    {
    private:
        Speech::USP::MetricMessageType m_metricType;
        WebSocketFrameType m_frameType;
        size_t m_size;
        std::shared_ptr<uint8_t> m_buffer;
        std::promise<void> m_promise;
        std::chrono::steady_clock::time_point m_createdTime = std::chrono::steady_clock::now();

        WebSocketMessage(size_t size, WebSocketFrameType frameType, Speech::USP::MetricMessageType metricType);

    public:
        WebSocketMessage(const std::string& data, Speech::USP::MetricMessageType metricType = Speech::USP::MetricMessageType::METRIC_MESSAGE_TYPE_INVALID);
        WebSocketMessage(const uint8_t* data, const size_t size, Speech::USP::MetricMessageType metricType = Speech::USP::MetricMessageType::METRIC_MESSAGE_TYPE_INVALID);

        virtual ~WebSocketMessage() = default;

        SPX_INTERFACE_MAP_BEGIN()
            SPX_INTERFACE_MAP_ENTRY(IWebSocketMessage)
        SPX_INTERFACE_MAP_END()

        int8_t MetricMessageType() const override;
        WebSocketFrameType FrameType() const override;
        size_t Size() const override;

        size_t Serialize(std::shared_ptr<uint8_t>& buffer) override;
        size_t Serialize(uint8_t* buffer, size_t size) override;

        std::string LogDescription() const override;

        void SetMessageSendSucceeded() override;
        void SetMessageSendFailed(std::exception_ptr eptr) override;
        std::future<void> MessageSendFuture();
        std::chrono::steady_clock::time_point CreationTime() const override
        {
            return m_createdTime;
        }
    };

}}}}
