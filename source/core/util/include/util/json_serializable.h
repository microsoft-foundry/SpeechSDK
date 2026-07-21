//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#pragma once

#include <functional>

#include "ajv.h"

#include "util/traits.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

class JSONSerializable
{
public:
    template<typename T, typename = std::enable_if_t<!IsSpecialization<T, std::shared_ptr>::value>>
    explicit JSONSerializable(T&& object):
        m_ptr{ new T(std::forward<T>(object)), +[](void * ptr) { delete static_cast<T *>(ptr); } },
        m_serialize{ +[](void * ptr) { return static_cast<T *>(ptr)->Serialize(); } }
    {}

    template<typename T>
    explicit JSONSerializable(std::shared_ptr<T> ptr):
        m_ptr{ std::static_pointer_cast<void>(ptr) },
        m_serialize{ +[](void * ptr) { return static_cast<T *>(ptr)->Serialize(); } }
    {}

    JSONSerializable(const JSONSerializable& other): m_ptr{ other.m_ptr }, m_serialize{ other.m_serialize }
    {}

    JSONSerializable(JSONSerializable&& other): m_ptr{ std::move(other.m_ptr) }, m_serialize{ other.m_serialize }
    {}

    JSONSerializable& operator=(const JSONSerializable& rhs)
    {
        if (this != &rhs)
        {
            m_ptr = rhs.m_ptr;
            m_serialize = rhs.m_serialize;
        }
        return *this;
    }

    JSONSerializable& operator=(JSONSerializable&& rhs)
    {
        if (this != &rhs)
        {
            m_ptr = std::move(rhs.m_ptr);
            m_serialize = rhs.m_serialize;
        }
        return *this;
    }

    ajv::JsonBuilder Serialize() const
    {
        if (m_ptr)
        {
            return m_serialize(m_ptr.get());
        }
        else
        {
            return ajv::JsonBuilder{};
        }
    }

private:
    using SerializeFunction = ajv::JsonBuilder(*)(void *);

    std::shared_ptr<void> m_ptr;

    SerializeFunction m_serialize;
};

} } } }
