//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// interface_delegate_helpers.h: Implementation declarations/definitions for CSpxDelegate* helpers
//

#pragma once
#include "spxcore_common.h"
#include "shared_ptr_helpers.h"
#include "string_utils.h"
#include "interface_helpers.h"
#include "service_helpers.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

template <class T, bool allowMutableInit = false>
class CSpxDelegateToSharedPtrHelper
{
public:
    using Ptr_Type = std::shared_ptr<T>;
    using Delegate_Type = std::shared_ptr<T>;
    using Const_Delegate_Type = std::shared_ptr<T>;

    CSpxDelegateToSharedPtrHelper() = default;
    virtual ~CSpxDelegateToSharedPtrHelper() = default;

    bool IsZombie() const
    {
        return m_zombie;
    }

    void Zombie(bool zombie = true)
    {
        m_zombie = zombie;
    }

    bool IsClear() const
    {
        return !m_ptr;
    }

    void Clear()
    {
        m_ptr = nullptr;
    }

    void ZombieTermAndClear()
    {
        auto ptr = m_ptr;
        Zombie(true);
        Clear();
        SpxTermAndClear(ptr);
    }

    bool IsReady()
    {
        return !IsZombie() && !IsClear();
    }

    void SetDelegate(Delegate_Type ptr)
    {
        m_ptr = ptr;
    }

    Delegate_Type GetDelegate()
    {
        if (!m_zombie && !m_ptr)
        {
            Zombie(true);
            InitDelegatePtr(m_ptr);
            Zombie(m_ptr == nullptr);
        }

        return m_zombie ? nullptr : m_ptr;
    }

    Const_Delegate_Type GetConstDelegate() const
    {
        if (!m_zombie && !m_ptr && allowMutableInit)
        {
            auto nonconst = const_cast<CSpxDelegateToSharedPtrHelper<T, allowMutableInit>*>(this);
            nonconst->GetDelegate();
        }

        return m_zombie ? nullptr : m_ptr;
    }

protected:
    virtual void InitDelegatePtr(Ptr_Type& ptr) { UNUSED(ptr);  }

private:
    Ptr_Type m_ptr;
    bool m_zombie { false };
};

template<typename I>
void SpxTermAndClearDelegate(CSpxDelegateToSharedPtrHelper<I>& delegateHelper)
{
    auto ptr = delegateHelper.GetDelegate();
    delegateHelper.Zombie(true);
    delegateHelper.Clear();
    SpxTermAndClear(ptr);
}

template<typename I, typename U>
std::shared_ptr<I> SpxQueryInterfaceFromDelegate(CSpxDelegateToSharedPtrHelper<U>& delegateHelper)
{
    auto ptr = delegateHelper.GetDelegate();
    return SpxQueryInterface<I>(ptr);
}

template<typename I, typename U>
const std::shared_ptr<I> SpxQueryInterfaceFromDelegate(const CSpxDelegateToSharedPtrHelper<U>& delegateHelper)
{
    auto ptr = delegateHelper.GetConstDelegate();
    return SpxQueryInterface<I>(ptr);
}


template <class T, bool allowMutableInit = false>
class CSpxDelegateToWeakPtrHelper
{
public:

    CSpxDelegateToWeakPtrHelper() = default;
    virtual ~CSpxDelegateToWeakPtrHelper() = default;

    bool IsZombie() const { return m_zombie; }
    void Zombie(bool zombie = true) { m_zombie = zombie; }

    bool IsClear() { return m_ptr.expired(); }
    void Clear() { m_ptr.reset(); }

    bool IsReady() { return !IsZombie() && !m_ptr.expired(); }

protected:

    using Ptr_Type = std::weak_ptr<T>;
    using Delegate_Type = std::shared_ptr<T>;
    using Const_Delegate_Type = std::shared_ptr<T>;

    void SetDelegate(Delegate_Type ptr)
    {
        m_ptr = ptr;
    }

    Delegate_Type GetDelegate()
    {
        if (!m_zombie && m_ptr.expired())
        {
            Zombie(true);
            InitDelegatePtr(m_ptr);
            Zombie(m_ptr.expired());
        }

        return m_zombie ? nullptr : m_ptr.lock();
    }

    Const_Delegate_Type GetConstDelegate() const
    {
        if (!m_zombie && !m_ptr.expired() && allowMutableInit)
        {
            auto nonconst = const_cast<CSpxDelegateToWeakPtrHelper<T, allowMutableInit>*>(this);
            nonconst->GetDelegate();
        }

        return (m_zombie || m_ptr.expired())
            ? nullptr
            : m_ptr.lock();
    }

    virtual void InitDelegatePtr(Ptr_Type& /* ptr */) { }

private:

    Ptr_Type m_ptr;
    bool m_zombie { false };
};

template <class T, class ObjectWithSiteT, bool allowMutableInit = false>
class CSpxDelegateToSiteSharedPtrHelper : public CSpxDelegateToSharedPtrHelper<T, allowMutableInit>
{
private:

    using Base = CSpxDelegateToSharedPtrHelper<T, allowMutableInit>;

protected:

    using Ptr_Type = std::shared_ptr<T>;
    using Delegate_Type = std::shared_ptr<T>;

    void InitDelegatePtr(Ptr_Type& /* ptr */) override
    {
        Base::SetDelegate(GetDelegatePtrFromSite());
    }

private:

    Delegate_Type GetDelegatePtrFromSite()
    {
        auto site = static_cast<ObjectWithSiteT*>(this)->GetSite();
        auto ptr = SpxQueryInterface<T>(site);
        return ptr != nullptr ? ptr : SpxQueryService<T>(site);
    }
};

template <class T, class ObjectWithSiteT, bool allowMutableInit = false>
class CSpxDelegateToSiteWeakPtrHelper : public CSpxDelegateToWeakPtrHelper<T, allowMutableInit>
{
private:

    using Base = CSpxDelegateToWeakPtrHelper<T, allowMutableInit>;

protected:

    using Ptr_Type = std::weak_ptr<T>;
    using Delegate_Type = std::shared_ptr<T>;

    void InitDelegatePtr(Ptr_Type& /* ptr */) override
    {
        Base::SetDelegate(GetDelegatePtrFromSite());
    }

private:

    Delegate_Type GetDelegatePtrFromSite()
    {
        auto site = static_cast<ObjectWithSiteT*>(this)->GetSite();
        auto ptr = SpxQueryInterface<T>(site);
        return ptr != nullptr ? ptr : SpxQueryService<T>(site);
    }
};

template<typename T, typename F, typename... Ts>
void InvokeOnDelegate(const std::shared_ptr<T>& ptr, F f, Ts&&... args)
{
    SPX_IFTRUE(ptr, ((ptr.get())->*f)(std::forward<Ts>(args)...));
}

template<typename T, typename F, typename... Ts, typename U>
auto InvokeOnDelegateR(const std::shared_ptr<T>& ptr, F f, U default_value, Ts&&... args) -> decltype(((std::declval<T*>())->*f)(std::forward<Ts>(args)...)) // NOLINT
{
    using return_type = decltype(((std::declval<T*>())->*f)(std::forward<Ts>(args)...));
    /* When we have c++17 we could collapse these 2 functions */
    if (ptr)
    {
        return ((ptr.get())->*f)(std::forward<Ts>(args)...);
    }
    return static_cast<return_type>(default_value);
}

#define SPX_DELEGATE_ACCESSORS(Name, D, I)                                                              \
    using Delegate_Type = std::shared_ptr< I >;                                                         \
    inline Delegate_Type Get ## Name ## Delegate () { return D::GetDelegate(); }                        \
    using ConstDelegate_Type = const std::shared_ptr< I >;                                              \
    inline ConstDelegate_Type GetConst ## Name ## Delegate () const { return D::GetConstDelegate(); }   \
    inline void Set ## Name ## Delegate (Delegate_Type ptr) { D::SetDelegate(ptr); }                    \
    inline bool Is ## Name ## DelegateZombie() { return D::IsZombie(); }                                \
    inline void Zombie ## Name ## Delegate(bool zombie = true) { D::Zombie(zombie); }                   \
    inline bool Is ## Name ## DelegateClear() { return D::IsClear(); }                                  \
    inline void Clear ## Name ## Delegate() { D::Clear(); }                                             \
    inline void ZombieTermAndClear ## Name ## Delegate() { D::ZombieTermAndClear(); }                   \
    inline bool Is ## Name ## DelegateReady() { return D::IsReady(); }

} } } } // Microsoft::CognitiveServices::Speech::Impl
