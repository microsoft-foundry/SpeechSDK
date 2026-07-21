//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#pragma once

#include <set>
#include <map>
#include <type_traits>
#include <functional>

#include "util/result.h"
#include "util/maybe.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

enum class TransitionError
{
    NodeNotFound,
    InvalidTransition,
    PossibleBounce
};

template<typename S, std::enable_if_t<std::is_enum<S>::value, int> = 0>
class StateMachine
{
public:

    explicit StateMachine(std::map<S, std::set<S>> states, S initialState):
        m_nodes{ std::move(states) },
        m_current{ Find(m_nodes, initialState).Get() }
    {}

    S Current() const
    {
        return m_current.get().first;
    }

    Result<TransitionError> Transition(S state)
    {
        return ToResult(Find(m_nodes, state), TransitionError::NodeNotFound)
            .template Also<bool>([&]()
            {
                if (IsTransitionValid(state))
                {
                    return Result<bool, TransitionError>{ true };
                }
                if (m_current.get().first == state)
                {
                    return Result<bool, TransitionError>{ TransitionError::PossibleBounce };
                }
                return Result<bool, TransitionError>{ TransitionError::InvalidTransition };
            })
            .AndThen([&](auto value)
            {
                m_current = std::get<0>(value).get();
                if (m_onExit)
                {
                    m_onExit();
                }
                m_onExit = std::function<void()>{};
                return Result<TransitionError>{};
            });
    }

    Result<TransitionError> Transition(S state, std::function<void()> onExit)
    {
        return ToResult(Find(m_nodes, state), TransitionError::NodeNotFound)
            .template Also<bool>([&]()
            {
                if (IsTransitionValid(state))
                {
                    return Result<bool, TransitionError>{ true };
                }
                if (m_current.get().first == state)
                {
                    return Result<bool, TransitionError>{ TransitionError::PossibleBounce };
                }
                return Result<bool, TransitionError>{ TransitionError::InvalidTransition };
            })
            .AndThen([&](auto value)
            {
                m_current = std::get<0>(value).get();
                if (m_onExit)
                {
                    m_onExit();
                }
                m_onExit = std::move(onExit);
                return Result<TransitionError>{};
            });
    }

private:
    using MapType = std::map<S, std::set<S>>;
    using ValueType = typename MapType::value_type;

    static Maybe<std::reference_wrapper<const ValueType>> Find(const MapType& nodes, S state)
    {
        auto it = nodes.find(state);
        if (it != nodes.end())
        {
            return std::reference_wrapper<const ValueType>{ *it };
        }
        return nullptr;
    }

    bool IsTransitionValid(S state)
    {
        auto& transitions = m_current.get().second;
        return transitions.find(state) != transitions.end();
    }

    std::map<S, std::set<S>> m_nodes;
    std::reference_wrapper<const ValueType> m_current;
    std::function<void()> m_onExit;
};


} } } } // Microsoft::CognitiveServices::Speech::Impl
