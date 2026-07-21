//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// factory_helpers.h: Helper methods related to object factories
//

#pragma once
#include "spxcore_common.h"
#include "platform.h"
#include "string_utils.h"


namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

#define SPX_FACTORY_MAP_BEGIN()         \
    UNUSED(className);                  \
    UNUSED(interfaceTypeId);

#define SPX_FACTORY_MAP_ENTRY(x, y)                                             \
    SPX_FACTORY_MAP_ENTRY_IF(true, x, y, x)

#define SPX_FACTORY_MAP_ENTRY_REPLACE(x, y, replaceWith)                        \
    SPX_FACTORY_MAP_ENTRY_IF(true, x, y, replaceWith)

#define SPX_FACTORY_MAP_ENTRY_IF(condition, x, y, create)                       \
    if (PAL::stricmp(className, #x) == 0)                                       \
    {                                                                           \
        if (condition && Type<y>::Id == interfaceTypeId)                   \
        {                                                                       \
            return SpxFactoryEntryCreateObject<create, y>();                    \
        }                                                                       \
    }

#define SPX_FACTORY_MAP_ENTRY_FUNC(x)                                           \
    {                                                                           \
        auto factory = x(className, interfaceTypeId);                           \
        if (factory != nullptr)                                                 \
        {                                                                       \
            return factory;                                                     \
        }                                                                       \
    }

#define SPX_FACTORY_MAP_END()   \
    return nullptr;

template <class T, class I>
void* SpxFactoryEntryCreateObject()
{
    auto ptr = new T();
    auto _interface_ = static_cast<I*>(ptr);
    return _interface_;
}

#ifdef __linux__
#define _LIB_PREFIX_ "lib"
#define _LIB_EXT_ ".so"
#elif __MACH__
#define _LIB_PREFIX_ "lib"
#define _LIB_EXT_ ".dylib"
#else
#define _LIB_PREFIX_
#define _LIB_EXT_ ".dll"
#endif

} } } } // Microsoft::CognitiveServices::Speech::Impl
