//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// handle_table.h: Implementation declarations/definitions for Handle Table C++  classes
//

#pragma once
#include <atomic>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <platform.h>
#include <spxdebug.h>
#include <interfaces/types.h>
#include <mutex>

#include "ajv.h"

#ifdef _MSC_VER
#include <shared_mutex>
#endif // _MSC_VER


namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

// Class to count the number of handles a CSpxHandleTable instance is tracking
class CSpxHandleCounter
{
public:
    CSpxHandleCounter(const uint64_t typeId, const char * typeName) : m_size(0), m_id(typeId), m_name(typeName)
    {
        SPX_TRACE_INFO("Creating handle table counter for type id=%" PRIu64 ", name=%s\n", m_id, m_name);
    }

    virtual ~CSpxHandleCounter() {}

    size_t Increment()
    {
        return ++m_size;
    }

    size_t Decrement()
    {
        return --m_size;
    }

    size_t Count()
    {
        return m_size;
    }

    uint64_t Id() const
    {
        return m_id;
    }

    const char* Name() const
    {
        return m_name;
    }

private:
    std::atomic_size_t m_size;
    uint64_t m_id;
    const char * m_name;
};

class CSpxHandleTableBase : public CSpxHandleCounter
{
public:

    CSpxHandleTableBase(const uint64_t typeId, const char * typeName) : CSpxHandleCounter(typeId, typeName)
    {
    }
};

template <class T, class Handle>
class CSpxHandleTable : public CSpxHandleTableBase
{
private:
    const Handle INVALID_HANDLE{ (Handle)SPXHANDLE_INVALID };

public:

    CSpxHandleTable() : CSpxHandleTableBase(Type<T>::Id, Type<T>::Name)
    {
    }

    CSpxHandleTable(const uint64_t typeId, const char* typeName) : CSpxHandleTableBase(typeId, typeName)
    {
    }

    ~CSpxHandleTable()
    {
        Term();
    }

    Handle TrackHandle(std::shared_ptr<T> t)
    {
        Handle handle = INVALID_HANDLE;
        WriteLock_Type writeLock(m_mutex);

        T* ptr = t.get();
        SPX_DBG_TRACE_VERBOSE_IF(1,
            "CSpxHandleTable::TrackHandle p=0x%8p", (void*)ptr);

        if (ptr != nullptr)
        {
            handle = reinterpret_cast<Handle>(ptr);
            SPX_DBG_TRACE_VERBOSE_IF(1,
                "CSpxHandleTable::TrackHandle class=%s, h=0x%8p, p=0x%8p, tot=%zu",
                Name(),
                (void*)handle,
                (void*)ptr,
                m_ptrMap.size() + 1);

            this->Increment();
            m_handleMap.emplace(handle, t);
            m_ptrMap.emplace(ptr, handle);
        }

        return handle;
    }

    bool IsTracked(Handle handle)
    {
        ReadLock_Type readLock(m_mutex);
        return m_handleMap.find(handle) != m_handleMap.end();
    }

    bool IsTracked(T* ptr)
    {
        ReadLock_Type readLock(m_mutex);
        return m_ptrMap.find(ptr) != m_ptrMap.end();
    }

    std::shared_ptr<T> TryGetPtr(Handle handle)
    {
        ReadLock_Type readLock(m_mutex);
        auto item = m_handleMap.find(handle);
        return item == m_handleMap.end() ? nullptr : item->second;
    }

    std::shared_ptr<T> GetPtr(Handle handle)
    {
        if (handle == INVALID_HANDLE)
        {
            SPX_TRACE_ERROR("GetPtr called with INVALID_HANDLE for class=%s", Name());
            SPX_THROW_HR(SPXERR_INVALID_HANDLE);
        }
        
        auto ptr = TryGetPtr(handle);
        if (ptr == nullptr)
        {
            SPX_TRACE_ERROR("GetPtr failed to find handle 0x%8p for class=%s", (void*)handle, Name());
            SPX_THROW_HR(SPXERR_INVALID_ARG);
        }
        return ptr;
    }

    Handle TryGetHandle(T* ptr)
    {
        ReadLock_Type readLock(m_mutex);
        auto item = m_ptrMap.find(ptr);
        return item == m_ptrMap.end() ? INVALID_HANDLE : item->second;
    }

    Handle GetHandle(T* ptr)
    {
        auto handle = TryGetHandle(ptr);
        if (handle == INVALID_HANDLE)
        {
            SPX_TRACE_ERROR("GetHandle failed to find pointer 0x%8p for class=%s", (void*)ptr, Name());
            SPX_THROW_HR(SPXERR_INVALID_ARG);
        }
        return handle;
    }

    std::shared_ptr<T> operator[](Handle handle) { return GetPtr(handle); }
    Handle operator[](T* ptr) { return GetHandle(ptr); }

    bool StopTracking(Handle handle)
    {
        auto stopped = false;
        SPX_DBG_TRACE_VERBOSE_IF(1,
            "CSpxHandleTable::StopTracking(h) h=0x%8p",
            (void*)handle);
        if (IsTracked(handle))
        {
            WriteLock_Type writeLock(m_mutex);
            auto iterHandleMap = m_handleMap.find(handle);
            if (iterHandleMap != m_handleMap.end())
            {
                auto sharedPtr = iterHandleMap->second;
                auto iterPtrMap = m_ptrMap.find(sharedPtr.get());

                SPX_DBG_TRACE_VERBOSE_IF(1,
                    "CSpxHandleTable::StopTracking(h) class=%s, h=0x%8p, p=0x%8p, tot=%zu",
                    Name(),
                    (void*)handle,
                    (void*)sharedPtr.get(),
                    m_ptrMap.size() - 1);

                m_handleMap.erase(iterHandleMap);
                m_ptrMap.erase(iterPtrMap);
                this->Decrement();
                stopped = true;

                // If the "sharedPtr" ends up being the very last reference to the "T" object
                // the scope exit will cause T's dtor to be called, which in turn could, potentially
                // result in a call back to this same handle table, but from another thread. That would
                // cause a deadlock (this thread waiting for that thread, but this thread owns the mutex
                // and that thread will never be able to obtain it)... Unless ... We unlock the write lock
                // and then have the shared_ptr release it's reference .. So ... That's what we'll do.
                writeLock.unlock();
                sharedPtr.reset();
            }
        }
        return stopped;
    }

    bool StopTracking(T* ptr)
    {
        auto stopped = false;
        SPX_DBG_TRACE_VERBOSE_IF(1,
            "CSpxHandleTable::StopTracking(p) iid=%llu, p=0x%8x",
            Id(),
            ptr);
        if (IsTracked(ptr))
        {
            WriteLock_Type writeLock(m_mutex);
            auto iterPtrMap = m_ptrMap.find(ptr);
            if (iterPtrMap != m_ptrMap.end())
            {
                auto handle = iterPtrMap->second;
                auto iterHandleMap = m_handleMap.find(handle);
                auto sharedPtr = iterHandleMap->second;

                SPX_DBG_TRACE_VERBOSE_IF(1,
                    "CSpxHandleTable::StopTracking(p) class=%s h=0x%8x, p=0x%8p, tot=%zu",
                    Name(),
                    (void*)handle,
                    (void*)sharedPtr.get(),
                    m_ptrMap.size() - 1);

                m_ptrMap.erase(iterPtrMap);
                m_handleMap.erase(iterHandleMap);
                this->Decrement();
                stopped = true;

                // If the "sharedPtr" ends up being the very last reference to the "T" object
                // the scope exit will cause T's dtor to be called, which in turn could, potentially
                // result in a call back to this same handle table, but from another thread. That would
                // cause a deadlock (this thread waiting for that thread, but this thread owns the mutex
                // and that thread will never be able to obtain it)... Unless ... We unlock the write lock
                // and then have the shared_ptr release it's reference .. So ... That's what we'll do.
                writeLock.unlock();
                sharedPtr.reset();
            }
        }
        return stopped;
    }

    void Term()
    {
        SPX_DBG_TRACE_VERBOSE_IF(m_ptrMap.size() == 0,
            "CSpxHandleTable::Term: ZERO handles 'leaked' for class=%s",
            Name());
        SPX_TRACE_WARNING_IF(m_ptrMap.size() >= 1,
            "CSpxHandleTable::Term: %zu handles 'leaked' for class=%s",
            m_ptrMap.size(),
            Name());

#if _DEBUG
        for (const auto& entry : m_handleMap)
        {
            SPX_DBG_TRACE_WARNING("LEAKED HANDLE: 0x%8p,     LEAKED POINTER: 0x%8p", (void *)entry.first, (void *)entry.second.get());
        }
#endif

        WriteLock_Type lock(m_mutex);
        m_handleMap.clear();
        m_ptrMap.clear();
    }

private:

    #ifdef _MSC_VER
    using ReadWriteMutex_Type = std::shared_mutex;
    using WriteLock_Type = std::unique_lock<std::shared_mutex>;
    using ReadLock_Type = std::shared_lock<std::shared_mutex>;
    #else
    using ReadWriteMutex_Type = std::mutex;
    using WriteLock_Type = std::unique_lock<std::mutex>;
    using ReadLock_Type = std::unique_lock<std::mutex>;
    #endif

    ReadWriteMutex_Type m_mutex;
    std::multimap<Handle, std::shared_ptr<T>> m_handleMap;
    std::multimap<T*, Handle> m_ptrMap;
};

class CSpxSharedPtrHandleTableManager
{
private:

    static CSpxHandleCounter* GetCounter(const uint64_t typeId, const char* typeName, CSpxHandleCounter*(*fnCreate)(uint64_t, const char*))
    {
        std::unique_lock<std::mutex> lock(s_mutex);
        auto iter = s_counters->find(typeId);
        if( iter == s_counters->end())
        {
            SPX_TRACE_INFO("Creating handle table counter for type id=%" PRIu64 ", name=%s in manager %p", typeId, typeName, (void*)s_counters.get());
            return s_counters->emplace(typeId, fnCreate(typeId, typeName)).first->second;  
        }
        else
        {    
            return iter->second;
        }
    }

    template<class T, class Handle>
    static CSpxHandleCounter* CreateHandleTable(const uint64_t typeId, const char* typeName)
    {
        SPX_TRACE_FUNCTION();
        return new CSpxHandleTable<T, Handle>(typeId, typeName);
    }

public:

    template<class T, class Handle>
    static CSpxHandleTable<T, Handle>* Get()
    {
        return (CSpxHandleTable<T, Handle>*)GetCounter(Type<T>::Id, Type<T>::Name, CreateHandleTable<T, Handle>);
    }

    template<class T, class Handle>
    static std::shared_ptr<T> GetPtr(Handle handle)
    {
        auto handletable = Get<T, Handle>();
        return handletable->GetPtr(handle);
    }

    template<class T, class Handle>
    static std::shared_ptr<T> TryGetPtr(Handle handle)
    {
        auto handletable = Get<T, Handle>();
        return handletable->TryGetPtr(handle);
    }

    template<class T, class Handle>
    static Handle TrackHandle(std::shared_ptr<T> t)
    {
        auto handletable = Get<T, Handle>();
        return handletable->TrackHandle(t);
    }

    template<class T, class Handle>
    static bool IsTracked(Handle handle)
    {
        auto handletable = Get<T, Handle>();
        return handletable->IsTracked(handle);
    }

    template<class T, class Handle>
    static bool IsTracked(std::shared_ptr<T> t)
    {
        auto handletable = Get<T, Handle>();
        return handletable->IsTracked(t.get());
    }

    template<class T, class Handle>
    static void StopTracking(std::shared_ptr<T> t)
    {
        auto handletable = Get<T, Handle>();
        handletable->StopTracking(t.get());
    }

    template<class T, class Handle>
    static void StopTracking(Handle handle)
    {
        auto handletable = Get<T, Handle>();
        handletable->StopTracking(handle);
    }

    static void Term(bool runLockFree = false)
    {
        SPX_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
        // TODO: on OSX statics are destroyed before LibUnload is invoked,
        // so Term() should be called from the s_counters delete first.
        // Second time, when it's called from LibUnload, bail out if statics
        // are already deleted.
        // Is there a cleaner way to do the shut-down?
        if (s_counters == nullptr)
        {
            SPX_TRACE_VERBOSE("Handle table manager already terminated.\n");
            return;
        }

        std::unique_lock<std::mutex> lock(s_mutex, std::defer_lock);

        if(!runLockFree)
        {
            lock.lock();
        }

        SPX_TRACE_VERBOSE("Terminating handle table manager %p with %zu handle types tracked.\n", (void*)s_counters.get(), s_counters->size());
#ifdef ASAN_BUILD
// Only clear the table out for an ASAN enabled build.
// This prevents issues where an entry in the handle table references objects in its destructor that have already been destroyed, like static objects.
        for (const auto& item : *s_counters) {
            SPX_TRACE_VERBOSE("Terminating handle table for type id=%" PRIu64 ", name=%s with %zu handles still tracked.\n", item.first, item.second->Name(), item.second->Count());
            delete item.second;
        }
#endif
        s_counters->clear();
        s_counters = nullptr;
    }

    static size_t GetTotalTrackedObjectCount()
    {
        std::unique_lock<std::mutex> lock(s_mutex);
        size_t objCount = 0;

        for (const auto &counter : *s_counters) {
            objCount += counter.second->Count();
        }

        return objCount;
    }

    static std::string GetHandleCountByType()
    {
        std::unique_lock<std::mutex> lock(s_mutex);
        std::string ret;

        for (const auto &item : *s_counters) {
            ret += std::to_string(item.first) + " " + std::to_string(item.second->Count()) + "\r\n";
        }

        return ret;
    }

    static std::string GetHandleCountJson()
    {
        std::unique_lock<std::mutex> lock(s_mutex);

        ajv::JsonBuilder builder;
        int i = 0;

        for (const auto& item : *s_counters) {
            auto entry = builder[i++];
            entry["id"] = item.first;
            entry["name"] = item.second->Name();
            entry["count"] = item.second->Count();
        }

        return builder.AsJson();
    }

private:

    template<typename T>
    using deleted_unique_ptr = std::unique_ptr<T, std::function<void(T*)>>;

    static std::mutex s_mutex;

    static deleted_unique_ptr<std::map<uint64_t, CSpxHandleCounter*>> s_counters;
};

} } } } // Microsoft::CognitiveServices::Speech::Impl
