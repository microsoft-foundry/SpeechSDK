//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#pragma once

#include <memory>
#include <type_traits>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

template<typename T, std::enable_if_t<std::is_integral<T>::value, int> = 0>
class Buffer
{
public:
    template<typename B>
    explicit Buffer(B buffer):
        m_data{ new B(std::move(buffer)), +[](void* ptr) { delete static_cast<B *>(ptr); } },
        m_getFunction{ +[](void* ptr) { return static_cast<B *>(ptr)->Get(); } },
        m_sizeFunction{ +[](void* ptr) { return static_cast<B *>(ptr)->Size(); } }
    {}

    Buffer(std::nullptr_t):
        m_data{ nullptr },
        m_getFunction{ nullptr },
        m_sizeFunction{ nullptr }
    {}

    Buffer(const Buffer& other):
        m_data{ other.m_data },
        m_getFunction{ other.m_getFunction },
        m_sizeFunction{ other.m_sizeFunction }
    {}

    Buffer(Buffer&& other):
        m_data{ other.m_data },
        m_getFunction{ other.m_getFunction },
        m_sizeFunction{ other.m_sizeFunction }
    {
        other.m_data = nullptr;
        other.m_getFunction = nullptr;
        other.m_sizeFunction = nullptr;
    }

    Buffer& operator=(const Buffer& rhs)
    {
        m_data = rhs.m_data;
        m_getFunction = rhs.m_getFunction;
        m_sizeFunction = rhs.m_sizeFunction;
        return *this;
    }

    Buffer& operator=(Buffer&& rhs)
    {
        Buffer tmp{ std::move(*this) };
        m_data = rhs.m_data;
        m_getFunction = rhs.m_getFunction;
        m_sizeFunction = rhs.m_sizeFunction;
        rhs.m_data = nullptr;
        rhs.m_getFunction = nullptr;
        rhs.m_sizeFunction = nullptr;
        return *this;
    }

    ~Buffer() = default;

    operator bool() const
    {
        return Get() != nullptr;
    }

    T operator[](std::size_t index) const &
    {
        return Get()[index];
    }

    T * Get() const
    {
        if (m_getFunction != nullptr)
        {
            return m_getFunction(m_data.get());
        }
        return nullptr;
    }

    std::size_t Size() const
    {
        if (m_sizeFunction != nullptr)
        {
            return m_sizeFunction(m_data.get());
        }
        return 0;
    }

private:
    using GetFn = T * (*)(void *);
    using SizeFn = std::size_t(*)(void *);

    std::shared_ptr<void> m_data;

    GetFn m_getFunction;
    SizeFn m_sizeFunction;
};

template<typename T, std::enable_if_t<std::is_integral<T>::value, int> = 0>
class SharedBufferView
{
public:
    SharedBufferView(std::shared_ptr<T> ptr, size_t size):
        m_data{ ptr },
        m_size{ size }
    {}

    SharedBufferView(std::shared_ptr<T[]> ptr, size_t size):
        m_data{ ptr, ptr.get() },
        m_size{ size }
    {
    }

    template<typename U>
    SharedBufferView(std::decay_t<T>* ptr, size_t size, std::shared_ptr<U> owner):
        m_data{ owner, ptr },
        m_size{ size }
    {}

    SharedBufferView(std::nullptr_t):
        m_data{ nullptr },
        m_size{ 0 }
    {}

    SharedBufferView(const SharedBufferView& other):
        m_data{ other.m_data },
        m_size{ other.m_size }
    {
    }

    SharedBufferView(SharedBufferView&& other):
        m_data{ std::move(other.m_data) },
        m_size{ other.m_size }
    {
    }

    SharedBufferView& operator=(const SharedBufferView& rhs)
    {
        m_data = rhs.m_data;
        m_size = rhs.m_size;
        return *this;
    }

    SharedBufferView& operator=(SharedBufferView&& rhs)
    {
        m_data = std::move(rhs.m_data);
        m_size = std::move(rhs.m_size);
        return *this;
    }

    T * Get() const noexcept
    {
        return m_data.get();
    }

    std::size_t Size() const noexcept
    {
        return m_size;
    }

    T* operator->() const noexcept
    {
        return m_data.get();
    }

    T& operator[](size_t index) const
    {
        return (m_data.get())[index];
    }

    explicit operator T* ()
    {
        return m_data.get();
    }

    operator bool() const &
    {
        return m_data != nullptr;
    }

    SharedBufferView SubView(size_t offsetInBytes, size_t size) const
    {
        if ((offsetInBytes + size) > m_size)
        {
            std::abort();
        }
        return SharedBufferView{ m_data.get() + offsetInBytes, size, m_data };
    }

    void Swap(std::shared_ptr<T>& r)
    {
        m_size = 0;
        m_data.swap(r);
    }

    std::shared_ptr<T> InnerPtr()
    {
        return m_data;
    }

private:
    std::shared_ptr<T> m_data;
    size_t m_size;
};

template<typename T>
bool operator==(const SharedBufferView<T>& lhs, std::nullptr_t) noexcept
{
    return lhs.Get() == nullptr;
}

} } } }
