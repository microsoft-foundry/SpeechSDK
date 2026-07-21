//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#pragma once

#include <memory>
#include <type_traits>

#include "interface_helpers.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

template<typename... Ts>
class InterfaceWrapper
{
public:
    InterfaceWrapper() {}
    InterfaceWrapper(std::shared_ptr<ISpxInterfaceBase>) {}
    void operator=(std::shared_ptr<ISpxInterfaceBase>) {}

    /* Should never (ever) be called */
    template<typename U>
    U& I() { std::abort(); }
};

template<typename T, typename...Ts>
class InterfaceWrapper<T, Ts...>: private InterfaceWrapper<Ts...>
{
public:
    InterfaceWrapper() {}

    InterfaceWrapper(std::shared_ptr<ISpxInterfaceBase> obj): InterfaceWrapper<Ts...>{ obj }
    {
        if (obj)
        {
            m_ptr = SpxQueryInterface<T>(obj);
            SPX_DBG_ASSERT(m_ptr);
        }
    }

    void operator=(std::shared_ptr<ISpxInterfaceBase> obj)
    {
        if (obj)
        {
            m_ptr = SpxQueryInterface<T>(obj);
            SPX_DBG_ASSERT(m_ptr);
        }
        InterfaceWrapper<Ts...>::operator=(obj);
    }

    operator bool() const
    {
        return m_ptr != nullptr;
    }

    template<typename U, std::enable_if_t<std::is_same<U, T>::value, int> = 0>
    T& I()
    {
        return *m_ptr;
    }

    template<typename U, std::enable_if_t<!std::is_same<U, T>::value, int> = 0>
    U& I()
    {
        return InterfaceWrapper<Ts...>::template I<U>();
    }
private:
    std::shared_ptr<T> m_ptr;
};

} } } }
