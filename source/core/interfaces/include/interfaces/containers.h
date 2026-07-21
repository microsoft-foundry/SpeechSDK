//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#pragma once

#include <memory>
#include <type_traits>
#include <utility>
#include <map>
#include <vector>

#include <interfaces/aggregates.h>
#include <interfaces/base.h>

// TODO: This should be on its own helper module
#include <shared_ptr_helpers.h>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

template<typename T>
struct BufferContainer
{
public:
    BufferContainer(std::size_t size) : m_data{ SpxAllocUniqueBuffer<T>(size) }, m_size{ size }
    {}

    BufferContainer(buffer_unique_ptr_t<T> data, std::size_t size): m_data{ std::move(data) }, m_size{ size}
    {}

    T* get() const noexcept
    {
        return m_data.get();
    }

    std::size_t size() const noexcept
    {
        return m_size;
    }

    T& operator*() const
    {
        return *m_data;
    }

    T* operator->() const noexcept
    {
        return m_data.get();
    }

    explicit operator T* ()
    {
        return m_data.get();
    }

    T* Release() noexcept
    {
        m_size = 0;
        return m_data.release();
    }
private:
    buffer_unique_ptr_t<T> m_data;
    std::size_t m_size;
};

using SpxWaveFormatEx = BufferContainer<SPXWAVEFORMATEX>;

/* This is to be used when an aggregate needs to be stored in a handle table */
template<typename T, std::enable_if_t<std::is_trivial<T>::value, int> = 0>
class AggregateContainer: public ISpxInterfaceBaseFor<AggregateContainer<T>>
{
public:
    inline void Set(T&& val)
    {
        m_value = val;
    }

    inline const T& Get() const
    {
        return m_value;
    }

private:
    T m_value;
};

template<typename T, typename U>
using KeyValueVector = std::vector<std::pair<std::decay_t<T>, std::decay_t<U>>>;

using CSpxStringMap = std::map<std::string, std::string>;

} } } }
