//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include <memory>

#include "types.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

SPX_INTERFACE_NO_BASE(ISpxInterfaceBase) : public std::enable_shared_from_this<ISpxInterfaceBase>
{
    
public:

    virtual ~ISpxInterfaceBase() = default;

    template <class I>
    std::shared_ptr<I> QueryInterface()
    {
        return QueryInterfaceInternal<I>();
    }

protected:

    virtual void* QueryInterface(uint64_t /*typeId*/) { return nullptr; }

    typedef std::enable_shared_from_this<ISpxInterfaceBase> base_type;

    std::shared_ptr<ISpxInterfaceBase> shared_from_this()
    {
        return base_type::shared_from_this();
    }

    std::shared_ptr<const ISpxInterfaceBase> shared_from_this() const
    {
        return base_type::shared_from_this();
    }

private:

    template <class I>
    std::shared_ptr<I> QueryInterfaceInternal(uint64_t typeId)
    {
        // try to query for the interface via our virtual method...
        auto ptr = QueryInterface(typeId);
        if (ptr != nullptr)
        {
            auto interfacePtr = reinterpret_cast<I*>(ptr);
            return interfacePtr->shared_from_this();
        }

        // if that fails, let the caller know
        return nullptr;
    }

    template <class I>
    std::shared_ptr<I> QueryInterfaceInternal()
    {
        return QueryInterfaceInternal<I>(Type<I>::Id);
    }
};

template<typename T>
struct ISpxInterfaceBaseFor : virtual public ISpxInterfaceBase
{
    public:

    using Ptr = std::shared_ptr<T>;
    using WkPtr = std::weak_ptr<T>;
    using ConstPtr = std::shared_ptr<const T>;

    virtual ~ISpxInterfaceBaseFor() = default;

    std::shared_ptr<T> shared_from_this()
    {
        return std::shared_ptr<T>(base_type::shared_from_this(), static_cast<T*>(this));
    }

    std::shared_ptr<const T> shared_from_this() const
    {
        return std::shared_ptr<const T>(base_type::shared_from_this(), static_cast<const T*>(this));
    }

private:
    typedef ISpxInterfaceBase base_type;

    ISpxInterfaceBaseFor&& operator =(const ISpxInterfaceBaseFor&&) = delete;
};

} } } }
