//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#pragma once

#include <string>
#include <cstdint>
#include "interfaces/utils.h"
#include "util/scope_guard.h"
#include "typedefs.h"

namespace ajv {
    class JsonBuilder;
    class JsonParser;
} // namespace ajv

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {

namespace USP {
    class ISpxUspCallbacks;
    class CSpxUspConnection;
} // namespace USP

namespace Impl {

namespace detail {
    constexpr uint64_t hash(const char* s)
    {
        if (s == nullptr)
        {
            return 0;
        }
        constexpr uint64_t p = 31;
        constexpr uint64_t m = 1000000009;
        uint64_t hashValue = 0;
        uint64_t pPow = 1;
        for (std::size_t idx = 0; s[idx] != 0; idx++)
        {
            uint64_t c = s[idx];
            hashValue = (hashValue + c * pPow) % m;
            pPow = (pPow * p) % m;
        }
        return hashValue;
    }
}

// Type information is retrieved via a templated Type<> struct
//  - Type<T>::Id is a unique and stable identifier based on the name
//  - Type<T>::Name is the friendly string

template<typename I>
struct Type {};

// Standard use:
//  - When defining an interface, define it with SPX_INTERFACE(ISpxNewInterface) to automatically include its
//      Type information along with its ISpxInterfaceBaseFor<T> and AddPtrDef<T> derivations
//  - If a non-interface type needs Type information:
//      - Review and verify this is the right thing -- it usually isn't
//      - If it is, add it to the list of special cases below

#define _SPX_DEFINE_TYPE_WITH_NAME(N, ...) \
    template<> \
    struct Type< __VA_ARGS__ > \
    { \
        static constexpr auto Id = detail::hash(N); \
        static constexpr auto Name = N; \
    };

#define _SPX_DEFINE_TYPE(T) _SPX_DEFINE_TYPE_WITH_NAME( #T, T )

#define SPX_INTERFACE_NO_TYPE(I) \
    class I : public ISpxInterfaceBaseFor<I>

#define SPX_INTERFACE_NO_BASE(I) \
    class I; \
    _SPX_DEFINE_TYPE(I) \
    class I

///<summary>
/// Inherits interfaces to provide usage in CreateModuleObject and SpxQueryInterface.
/// This is because we need stronger typing with RTTI disabled.
///</summary>
#define SPX_INTERFACE(I) \
    SPX_INTERFACE_NO_BASE(I) : public ISpxInterfaceBaseFor<I>

// Special cases:
//  - Types that aren't interfaces (and whose usages should likely be revisited)
//  - Templated types (templates resolve well after macros)
//      - This could alternatively be solved by defining and using "real" non-templated derived types
//  - External types and types external to the namespace

template <typename T>
class CSpxAsyncOp;
template <class T, bool allowMutableInit>
class CSpxDelegateToSharedPtrHelper;
template <typename T>
class ISpxEventSignals;
template <typename... Ts>
class ISpxNotifyMe;

class CSpxAudioProcessorWriteToAudioSourceBuffer;
class CSpxBufferData;
class CSpxInteractiveMicrophone;
class CSpxNamedProperties;
class ExceptionWithCallStack;
class ISpxAudioProcessor;
class ISpxAudioSource;
class ISpxBufferData;
class ISpxRecognitionResult;
class ISpxNamedProperties;
class ISpxSynthesisResult;
class ISpxSynthesisVoicesResult;
struct SPXWAVEFORMATEX;
using ISpxNotifyMeAudioSourceBufferData = ISpxNotifyMe<const std::shared_ptr<ISpxAudioSource>&, const std::shared_ptr<ISpxBufferData>&>;

_SPX_DEFINE_TYPE(ajv::JsonBuilder)
_SPX_DEFINE_TYPE(ajv::JsonParser)

_SPX_DEFINE_TYPE(CSpxAsyncOp<bool>)
_SPX_DEFINE_TYPE(CSpxAsyncOp<std::shared_ptr<ISpxRecognitionResult>>)
_SPX_DEFINE_TYPE(CSpxAsyncOp<std::shared_ptr<ISpxSynthesisResult>>)
_SPX_DEFINE_TYPE(CSpxAsyncOp<std::shared_ptr<ISpxSynthesisVoicesResult>>)
_SPX_DEFINE_TYPE(CSpxAsyncOp<std::shared_ptr<ISpxNamedProperties>>)
_SPX_DEFINE_TYPE(CSpxAsyncOp<std::string>)
_SPX_DEFINE_TYPE(CSpxAsyncOp<void>)
_SPX_DEFINE_TYPE(CSpxAudioProcessorWriteToAudioSourceBuffer)
_SPX_DEFINE_TYPE(CSpxBufferData)
_SPX_DEFINE_TYPE(CSpxInteractiveMicrophone)
_SPX_DEFINE_TYPE(CSpxNamedProperties)
_SPX_DEFINE_TYPE(ExceptionWithCallStack)
_SPX_DEFINE_TYPE(ISpxEventSignals<ISpxNamedProperties>)
_SPX_DEFINE_TYPE(ISpxNotifyMe<const std::shared_ptr<ISpxAudioProcessor>&>)
_SPX_DEFINE_TYPE(ISpxNotifyMeAudioSourceBufferData)
_SPX_DEFINE_TYPE(SPXWAVEFORMATEX)
_SPX_DEFINE_TYPE(Speech::USP::ISpxUspCallbacks)
_SPX_DEFINE_TYPE(Speech::USP::CSpxUspConnection)
_SPX_DEFINE_TYPE(ScopeGuard)

} } } }
