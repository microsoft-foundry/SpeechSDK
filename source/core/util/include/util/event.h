//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

// Based on code from here:
// https://www.codeproject.com/Articles/1256352/CppEvent-How-to-Implement-Events-using-Standard-Cp

#pragma once

#include <atomic>
#include <mutex>
#include <functional>
#include <list>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

    // Event<>::Raise snapshots the registered handler list under m_lock,
    // releases the lock, then invokes handlers from the snapshot. Consequence:
    // Event<>::Remove and Event<>::Clear cannot retract handlers from snapshots
    // that other threads are already iterating -- a handler can be invoked
    // after its own Remove(id) returned.
    //
    // Consumers must use the Add(shared_ptr, &method) overload below or
    // capture a shared_ptr in the handler lambda. Raw [this] captures into
    // Event<>::Add or operator+= leave the handler holding a dangling pointer
    // when the registering object is destroyed before an in-flight Raise
    // finishes invoking from its snapshot.
    //
    // See docs/architecture/core-architecture/lifetime-safety-invariants.md
    // invariants 2 and 3.
    template<typename ...Args>
    class Event
    {
    public:
        Event() = default;

        Event(const std::string& name, std::function<void(bool, const std::string&)> connectDisconnectHandler) :
            m_name{ name },
            m_connectedDisconnectedHandler{ connectDisconnectHandler }
        {}

        size_t Add(std::function<void(Args...)> callback)
        {
            std::lock_guard<std::mutex> lock(m_lock);
            bool wasEmpty = m_handlers.empty();
            m_handlers.emplace_back(++m_nextId, std::move(callback));

            if (wasEmpty && m_connectedDisconnectedHandler)
            {
                m_connectedDisconnectedHandler(true, m_name);
            }

            return m_nextId;
        }

        template<typename C>
        size_t Add(std::shared_ptr<C> instance, void (C::* callback)(Args...))
        {
            std::weak_ptr<C> weak(instance);
            std::function<void(Args...)> boundCallback = [weak, callback](Args... args)
            {
                auto ptr = weak.lock();
                if (ptr != nullptr)
                {
                    (ptr.get()->*callback)(args...);
                }
            };

            return Add(boundCallback);
        }

        void Remove(const size_t id)
        {
            std::lock_guard<std::mutex> lock(m_lock);
            m_handlers.remove_if([id](const auto& handler)
            {
                return handler.id == id;
            });

            if (m_handlers.empty() && m_connectedDisconnectedHandler)
            {
                m_connectedDisconnectedHandler(false, m_name);
            }
        }

        void Clear()
        {
            std::lock_guard<std::mutex> lock(m_lock);
            bool hadValues = !m_handlers.empty();

            m_handlers.clear();

            if (hadValues && m_connectedDisconnectedHandler)
            {
                m_connectedDisconnectedHandler(false, m_name);
            }
        }

        void Raise(Args... params)
        {
            std::list<EventHandler> allHandlers;
            {
                std::lock_guard<std::mutex> lock(m_lock);
                allHandlers = m_handlers; // copy list
            }

            for (const auto& handler : allHandlers)
            {
                handler.callback(params...);
            }
        }

        size_t operator+=(std::function<void(Args...)> handler)
        {
            return Add(handler);
        }

        void operator-=(const size_t id)
        {
            Remove(id);
        }

        void operator()(Args... params)
        {
            Raise(params...);
        }

        Event(const Event& other) = delete;
        Event(Event&& other) = delete;
        Event& operator=(const Event& other) = delete;
        Event& operator=(Event&& other) = delete;

    private:
        struct EventHandler
        {
            EventHandler(size_t id, std::function<void(Args...)> callback)
                : id(id), callback(callback)
            {}

            size_t id;
            std::function<void(Args...)> callback;

            bool operator==(const EventHandler& other) const
            {
                return id == other.id;
            }
        };

        const std::string m_name;
        std::function<void(bool, const std::string&)> m_connectedDisconnectedHandler;
        size_t m_nextId = 0;
        std::mutex m_lock;
        std::list<EventHandler> m_handlers;
    };

} } } } // Microsoft::CognitiveServices::Speech::Impl
