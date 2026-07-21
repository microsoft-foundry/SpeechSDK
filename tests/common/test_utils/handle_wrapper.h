//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include <stdexcept>

namespace Azure {
namespace AI {
namespace Test {
namespace Tools {

    /// <summary>
    /// Helper class to make working with OS specific handles easier in C++
    /// </summary>
    /// <typeparam name="THandle"></typeparam>
    template<typename THandle>
    class HandleWrapper
    {
    public:
        using HandleDeleter = void(*)(THandle);

    private:
        THandle m_handle;
        HandleDeleter m_deleter;

    public:
        /// <summary>
        /// Creates a new instance
        /// </summary>
        /// <param name="deleter">The function to call when the handle goes out of scope. This is usually
        /// used to close/free</param>
        HandleWrapper(HandleDeleter deleter) :
            m_handle(),
            m_deleter(deleter)
        {
            memset(&m_handle, 0, sizeof(THandle));
        }

        ~HandleWrapper()
        {
            Close();
        }

        HandleWrapper(const HandleWrapper&) = delete;
        HandleWrapper& operator=(const HandleWrapper&) = delete;

        HandleWrapper(HandleWrapper&& other) :
            m_handle(),
            m_deleter()
        {
            m_handle = other.m_handle;
            m_deleter = other.m_deleter;

            memset(&other.m_handle, 0, sizeof(THandle));
            other.m_deleter = nullptr;
        }

        HandleWrapper& operator=(HandleWrapper&& other)
        {
            if (this == &other)
            {
                return *this;
            }

            Close();

            m_handle = other.m_handle;
            m_deleter = other.m_deleter;

            memset(&other.m_handle, 0, sizeof(THandle));
            other.m_deleter = nullptr;
        }

        THandle& operator=(const THandle& other)
        {
            if (m_handle)
            {
                Close();
            }

            m_handle = other;
            return m_handle;
        }

        THandle* operator&()
        {
            return &m_handle;
        }

        operator THandle() const
        {
            return m_handle;
        }

    private:
        void Close()
        {
            if (m_deleter)
            {
                m_deleter(m_handle);
                memset(&m_handle, 0, sizeof(THandle));
            }
        }
    };

    template<typename TFunc>
    class ScopeGuard
    {
    public:
        ScopeGuard(ScopeGuard&&) = default;
        ScopeGuard(const ScopeGuard&) = delete;

        explicit ScopeGuard(TFunc f): m_fn{ f }
        {}

        ~ScopeGuard()
        {
            m_fn();
        }

    private:
        TFunc m_fn;
    };

    template<typename TFunc>
    ScopeGuard<TFunc> MakeScopeGuard(TFunc fn)
    {
        return ScopeGuard<TFunc>{ fn };
    }

}}}}
