//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#pragma once

#include <functional>
#include <type_traits>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

template<typename T, typename U>
class Either
{
    static_assert(!std::is_same<T, U>::value, "Both types can't be the same.");
    static_assert(!std::is_void<T>::value, "Types can't be void");
    static_assert(!std::is_void<U>::value, "Types can't be void");
public:
    Either(): m_isLeft{ true }, m_left{ T{} }
    {}

    Either(T value): m_isLeft{ true }, m_left{ std::move(value) }
    {}

    Either(U value): m_isLeft{ false }, m_right{ std::move(value) }
    {}

    Either(const Either& other): m_isLeft{ other.m_isLeft }
    {
        if (m_isLeft)
        {
            new (&m_left) T{ other.m_left };
        }
        else
        {
            new (&m_right) U{ other.m_right };
        }
    }

    Either(Either&& other): m_isLeft{ other.m_isLeft }
    {
        if (m_isLeft)
        {
            new (&m_left) T{ std::move(other.m_left) };
        }
        else
        {
            new (&m_right) U{ std::move(other.m_right) };
        }
    }

    Either& operator=(const Either& rhs)
    {
        if (this != &rhs)
        {
            if (rhs.m_isLeft)
            {
                if (m_isLeft)
                {
                    m_left = rhs.m_left;
                }
                else
                {
                    m_right.~U();
                    new (&m_left) T{ rhs.m_left };
                }
            }
            else
            {
                if (m_isLeft)
                {
                    m_left.~T();
                    new (&m_right) U{ rhs.m_right };
                }
                else
                {
                    m_right = rhs.m_right;
                }
            }
            m_isLeft = rhs.m_isLeft;
        }
        return *this;
    }

    Either& operator=(Either&& rhs)
    {
        if (this != &rhs)
        {
            if (rhs.m_isLeft)
            {
                if (m_isLeft)
                {
                    m_left = std::move(rhs.m_left);
                }
                else
                {
                    m_right.~U();
                    new (&m_left) T{ std::move(rhs.m_left) };
                }
            }
            else
            {
                if (m_isLeft)
                {
                    m_left.~T();
                    new (&m_right) U{ std::move(rhs.m_right) };
                }
                else
                {
                    m_right = std::move(rhs.m_right);
                }
            }
            m_isLeft = rhs.m_isLeft;
        }
        return *this;
    }

    ~Either()
    {
        if (m_isLeft)
        {
            m_left.~T();
        }
        else
        {
            m_right.~U();
        }
    }

    template<typename V>
    bool Has() const
    {
        if (m_isLeft)
        {
            return std::is_same<T, V>::value;
        }
        else
        {
            return std::is_same<U, V>::value;
        }
    }

    template<typename V, std::enable_if_t<std::is_same<V, T>::value, int> = 1>
    const V& Get() const &
    {
        if (m_isLeft)
        {
            return m_left;
        }
        std::abort();
    }

    template<typename V, std::enable_if_t<std::is_same<V, U>::value, int> = 0>
    const V& Get() const &
    {
        if (!m_isLeft)
        {
            return m_right;
        }
        std::abort();
    }

    template<typename V, std::enable_if_t<std::is_same<V, T>::value, int> = 0>
    V&& Get() &&
    {
        if (m_isLeft)
        {
            return std::move(m_left);
        }
        std::abort();
    }

    template<typename V, std::enable_if_t<std::is_same<V, U>::value, int> = 0>
    V&& Get() &&
    {
        if (!m_isLeft)
        {
            return std::move(m_right);
        }
        std::abort();
    }

    /* TODO: constrain template */
    template<typename Visitor>
    auto Visit(Visitor&& visitor) const &
    {
        if (m_isLeft)
        {
            return visitor(m_left);
        }
        else
        {
            return visitor(m_right);
        }
    }

    template<typename Visitor>
    auto Visit(Visitor&& visitor) &&
    {
        if (m_isLeft)
        {
            return visitor(std::move(m_left));
        }
        else
        {
            return visitor(std::move(m_right));
        }
    }
private:
    bool m_isLeft;

    union
    {
        T m_left;
        U m_right;
    };
};

} } } }
