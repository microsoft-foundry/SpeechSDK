//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#pragma once

#include "ajv.h"

#include "util/state_machine.h"
#include "interfaces/i_http_endpoint_info.h"
#include "interfaces/i_web_socket_message.h"
#include "util/result.h"
#include "util/maybe.h"
#include "network/usp/message.h"
#include "network/usp/header_builder.h"
#include "network/usp/message_utils.h"
#include "network/usp/message_builder.h"
#include "network/usp/header_view.h"
#include "network/message/control.h"
#include "error_info.h"

#include "unit_test_utils.h"
#include "mocks.h"

using WebSocket = MergedMocks<MockWebSocket, MockWebSocketInit, MockObjectInit>;


struct MockUSPService
{
    enum class State
    {
        Disconnected,
        Connected,
        ConfigReceived,
        ContextReceived,
        Ready,
        Stopping,
        Error
    };
    using SM = Carbon::StateMachine<State>;

    static SM InitStateMachine()
    {
        return SM
        { {
            { State::Disconnected, { State::Connected, State::Error } },
            { State::Connected, { State::Disconnected, State::ConfigReceived, State::Error } },
            { State::ConfigReceived, { State::Disconnected, State::ContextReceived, State::Error } },
            { State::ContextReceived, { State::Disconnected, State::Ready, State::Error } },
            { State::Ready, { State::Disconnected, State::Stopping, State::Error } },
            { State::Stopping, { State::Disconnected, State::Connected, State::Error } },
            { State::Error, {} }
        }, State::Disconnected };
    }

    struct Event
    {
        std::string Name{};
        Carbon::Maybe<std::string> Message{};
        std::map<std::string, std::string> Extras{};
    };

    MockUSPService() = default;

    void Reset()
    {
        if (m_webSocket)
        {
            m_webSocket->ConnectHandler = std::function<void(const Carbon::IHttpEndpointInfo&, const std::string&)>{};
            m_webSocket->SendDataHandler = std::function<void(const Carbon::IWebSocketMessage::Ptr&)>{};
            m_webSocket = nullptr;
        }
    }

    void UpdateWebSocket(std::shared_ptr<WebSocket> webSocket)
    {
        Reset();
        m_webSocket = webSocket;
        if (m_webSocket)
        {
            m_webSocket->ConnectHandler = [this](auto& endpointInfo, auto& connectionId)
            {
                this->OnConnect(endpointInfo, connectionId);
            };
            m_webSocket->SendDataHandler = [this](auto& message)
            {
                this->OnData(message);
            };
        }
    }

    bool HasWebSocket() const
    {
        return m_webSocket != nullptr;
    }

    ~MockUSPService()
    {
        Reset();
    }

    bool Error() const
    {
        return m_stateMachine.Current() == State::Error;
    }

    operator bool() const
    {
        return !Error();
    }

    void OnConnect(const Carbon::IHttpEndpointInfo& endpointInfo, const std::string& connectionId)
    {
        AddEvent(Event{ "connect", nullptr, {
            { "endpoint",  endpointInfo.EndpointUrl() },
            { "connectionId", connectionId }
        } });
        m_stateMachine.Transition(State::Connected)
            .AndThen([&]()
            {
                if (m_checkpoint)
                {
                    m_currentOffset = m_checkpoint.Get();
                }
                else
                {
                    m_currentOffset = nullptr;
                }
                m_webSocket->OnConnected.Raise("https://mock.url");
                return Carbon::Result<Carbon::TransitionError>{};
            })
            .OrElse<Carbon::TransitionError>([&](auto)
            {
                this->AddEvent(Event{ "error", { "Call to connect invalid." } });
                return m_stateMachine.Transition(State::Error);
            });
    }

    void OnData(const Carbon::IWebSocketMessage::Ptr& message)
    {
        const auto isBinary = message->FrameType() == Carbon::WebSocketFrameType::Binary;
        std::shared_ptr<uint8_t> buffer{};
        auto size = message->Serialize(buffer);
        auto uspMessage = Carbon::USP::USPMessageUtils::Deserialize(buffer.get(), size, isBinary);
        const Carbon::USP::USPHeaderView headerView{ uspMessage.Headers };
        auto path = headerView.Path().GetOr("");
        if (CheckMessage(path, uspMessage))
        {
            if (path == "config")
            {
                return OnConfig(uspMessage);
            }
            if (path == "context")
            {
                return OnContext(uspMessage);
            }
            AddEvent(Event{ "error", { "Unexpected path" }, { { "path", path } } });
            m_stateMachine.Transition(State::Error);
        }
        else
        {
            AddEvent(Event{ "error", { "Message failed provided predicate" }, { { "path", path } } });
            m_stateMachine.Transition(State::Error);
        }
    }

    void OnConfig(const Carbon::USP::Message&)
    {
        AddEvent(Event{ "config", { "Config received" } });
        m_stateMachine.Transition(State::ConfigReceived)
            .OrElse<Carbon::TransitionError>([&](auto)
            {
                this->AddEvent(Event{ "error", { "Invalid transition (config)" } });
                return m_stateMachine.Transition(State::Error);
            });
    }

    void OnContext(const Carbon::USP::Message&)
    {
        AddEvent(Event{ "context", { "Context received" } });
        m_stateMachine.Transition(State::ContextReceived)
            .AndThen([&]()
            {
                auto payload = Carbon::Message::Control{ "ready" }.Serialize();
                auto headers = [&]()
                {
                    auto builder = Carbon::USP::USPHeaderBuilder{ "control" }
                        .ContentType("application/json");
                    if (m_checkpoint)
                    {
                        builder.PresentationTimeStamp(m_checkpoint.Get());
                    }
                    return builder.Build();
                }();
                auto message = Carbon::USP::USPMessageBuilder()
                    .Headers(std::move(headers))
                    .Payload(payload.AsJson())
                    .Build();
                auto serialized = Carbon::USP::USPMessageUtils::Serialize(message);
                return m_stateMachine.Transition(State::Ready)
                    .AndThen([&]()
                    {
                        m_webSocket->OnTextData.Raise(
                            std::string{ reinterpret_cast<const char *>(serialized.Get()), serialized.Size() });
                        AddEvent(Event{ "control", { "Control message sent (ready)" } });
                        return Carbon::Result<Carbon::TransitionError>{};
                    });

            })
            .OrElse<Carbon::TransitionError>([&](auto)
            {
                this->AddEvent(Event{ "error", { "Invalid transition (context)" } });
                return m_stateMachine.Transition(State::Error);
            });
    }

    void AddPredicate(std::string path, std::function<bool(const Carbon::USP::Message&)> predicate)
    {
        m_messagePredicates.emplace(std::move(path), std::move(predicate));
    }

    bool CheckMessage(const std::string& path, const Carbon::USP::Message& message) const
    {
        auto it = m_messagePredicates.find(path);
        if (it !=  m_messagePredicates.end())
        {
            return it->second(message);
        }
        return true;
    }

    void AddEvent(Event event)
    {
        std::lock_guard<std::mutex> lk{ m_eventMutex };
        m_eventLog.push_back(std::move(event));
    }

    void DumpEventLog(std::ostream& out) const
    {
        std::lock_guard<std::mutex> lk{ m_eventMutex };
        for (const auto& evt : m_eventLog)
        {
            out << "Event: " << evt.Name;
            if (evt.Message)
            {
                out << " -> " << evt.Message.Get();
            }
            out << "\n";
            for (const auto& extra : evt.Extras)
            {
                out << "    " << extra.first << ": \"" << extra.second << "\"\n";
            }
        }
    }

    mutable std::mutex m_eventMutex{};
    std::shared_ptr<WebSocket> m_webSocket;
    SM m_stateMachine{ InitStateMachine() };
    std::vector<Event> m_eventLog{};
    std::map<std::string, std::function<bool(const Carbon::USP::Message&)>> m_messagePredicates{};

    Carbon::Maybe<uint64_t> m_currentOffset{};
    Carbon::Maybe<uint64_t> m_checkpoint{};

};
