//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#pragma once

#include <chrono>
#include <limits>
#include <regex>
#include <string>
#include <type_traits>

#include "interfaces/base.h"
#include "interfaces/containers.h"
#include "interfaces/utils.h"
#include "property_id_2_name_map.h"
#include "spxdebug.h"
#include "string_utils.h"
#include "util/traits.h"
#include "util/maybe.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

enum class VariantKind { Empty = 0, Binary = 1, String = 2 };
struct VariantValue
{
    std::shared_ptr<uint8_t> data;
    VariantKind kind;
    size_t dataSize;

    bool IsEmpty() const
    {
        return dataSize == 0;
    }

    static VariantValue Empty()
    {
        return VariantValue{ SpxAllocSharedUint8Buffer(0), VariantKind::Binary, 0 };
    }

    static VariantValue From(const char* psz)
    {
        auto len = strlen(psz);
        auto ptr = SpxAllocSharedUint8Buffer(len + 1);
        auto dest = ptr.get();
        auto size = len + 1;
        memcpy(dest, psz, size);
        return VariantValue{ ptr, VariantKind::String, size };
    }

    static VariantValue From(const uint8_t* binary, size_t size)
    {
        auto ptr = SpxAllocSharedUint8Buffer(size);
        auto dest = ptr.get();
        memcpy(dest, binary, size);
        return VariantValue{ ptr, VariantKind::Binary, size };
    }

    static VariantValue From(std::shared_ptr<uint8_t> binary, size_t size)
    {
        return VariantValue{ binary, VariantKind::Binary, size };
    }

    const char* AsString() const
    {
        auto ptr = data.get();
        return kind == VariantKind::String && ptr != nullptr
            ? (const char*)ptr
            : nullptr;
    }

    uint8_t* AsPtr() const
    {
        auto ptr = data.get();
        return kind == VariantKind::Binary && ptr != nullptr
            ? ptr
            : nullptr;
    }
};

SPX_INTERFACE(ISpxNamedProperties)
{
public:

    enum NoMatchContinueStrategy { Down = -1, Up = 1, None = 0 };

    virtual void SetStringValue(const char* name, const char* value) = 0;
    virtual void SetBinaryValue(const char* name, std::shared_ptr<uint8_t> value, size_t size) = 0;
    virtual bool Match(const char* name, bool fullMatch, const std::regex * pattern, VariantValue * output1, std::multimap<std::string, VariantValue>* outputAll, NoMatchContinueStrategy strategy = NoMatchContinueStrategy::Up, const ISpxNamedProperties * context = nullptr) const = 0;

public: // public helper methods

    bool HasStringValue(const char* name) const
    {
        SPX_THROW_HR_IF(SPXERR_INVALID_ARG, name == nullptr);
        return Match(name, true, nullptr, nullptr, nullptr, NoMatchContinueStrategy::Up);
    }

    std::string GetStringValue(const char* name, const char* defaultValue = "") const
    {
        return GetStringValue(name, defaultValue, this);
    }

    std::string GetStringValue(const char* name, const char* defaultValue, const ISpxNamedProperties * context) const
    {
        SPX_THROW_HR_IF(SPXERR_INVALID_ARG, name == nullptr);
        SPX_THROW_HR_IF(SPXERR_INVALID_ARG, defaultValue == nullptr);

        auto value = VariantValue::From(defaultValue);
        Match(name, true, nullptr, &value, nullptr, NoMatchContinueStrategy::Up, context);

        auto psz = value.AsString();
        if (psz == nullptr) psz = defaultValue;

        LogPropertyAndValue(name, psz, "ISpxNamedProperties::GetStringValue");
        return psz;
    }

    bool GetValue(const char* name, VariantValue* value = nullptr) const
    {
        return GetValue(name, value, this);
    }

    bool GetValue(const char* name, VariantValue* value, const ISpxNamedProperties * context) const
    {
        SPX_THROW_HR_IF(SPXERR_INVALID_ARG, name == nullptr);
        return Match(name, true, nullptr, value, nullptr, NoMatchContinueStrategy::Up, context);
    }

    void Copy(const std::shared_ptr<ISpxNamedProperties>& from, bool overwriteIfExists, const char* targetNamespace = nullptr)
    {
        SPX_DBG_TRACE_VERBOSE("ISpxNamedProperties::Copy from=%p to=%p", (void*)from.get(), (void*)this);

        auto prependStr = targetNamespace ? std::string(targetNamespace) : std::string("");;
        SPX_DBG_TRACE_VERBOSE_IF(!prependStr.empty(), "ISpxNamedProperties::Copy prepending '%s' to copied values", prependStr.c_str());

        std::multimap<std::string, VariantValue> outputAll;
        from->Match(nullptr, false, nullptr, nullptr, &outputAll, NoMatchContinueStrategy::None);
        for (const auto& it : outputAll)
        {
            const auto newName = prependStr.empty() ? it.first : prependStr + it.first;
            auto ok = overwriteIfExists || !HasStringValue(newName.c_str());
            if (ok)
            {
                auto iterFirstInMultiMap = outputAll.find(it.first);
                if (iterFirstInMultiMap->second.kind == VariantKind::String)
                {
                    SetStringValue(newName.c_str(), iterFirstInMultiMap->second.AsString());
                }
                else
                {
                    SetBinaryValue(newName.c_str(), iterFirstInMultiMap->second.data, iterFirstInMultiMap->second.dataSize);
                }
            }
        }
    }

    CSpxStringMap FindPrefix(const char* prefix_) const
    {
        // STEP 1: find the prefix matches, searching our parent
        std::multimap<std::string, VariantValue> outputAll;
        Match(prefix_, false, nullptr, nullptr, &outputAll, NoMatchContinueStrategy::Up);

        // STEP 2: strep off the prefix/separator
        CSpxStringMap wanted;
        for (const auto& it : outputAll)
        {
            auto iterFirstInMultiMap = outputAll.find(it.first);
            auto keyWithoutPrefix = iterFirstInMultiMap->first;
            auto seperator = keyWithoutPrefix.find(g_propertyNameSeparator);
            if (seperator != std::string::npos)
            {
                // +1 to move over the seperator
                keyWithoutPrefix.erase(0, seperator + 1);
            }
            auto psz = iterFirstInMultiMap->second.AsString();
            if (psz != nullptr)
            {
                wanted[keyWithoutPrefix] = psz;
                LogPropertyAndValue(keyWithoutPrefix, psz, "ISpxNamedProperties::FindPrefix");
            }
        }

        return wanted;
    }

    template<typename T = std::string, std::enable_if_t<std::is_same<std::string, T>::value, int> = 0>
    Maybe<T> Get(const char* name) const
    {
        if (!HasStringValue(name))
        {
            return Maybe<T>{};
        }
        return GetStringValue(name);
    }

    template<typename T, std::enable_if_t<std::is_signed<T>::value && !std::is_same<bool, T>::value && !std::is_floating_point<T>::value, int> = 0>
    Maybe<T> Get(const char* name) const
    {
        return Get<std::string>(name).Map<T>([&name](const std::string& stringValue) -> Maybe<T>
            {
                #ifdef ANDROID
                UNUSED(name); // Andsroid logging may not be enabled and hence name isn't used.
                #endif

                if (!stringValue.empty())
                {
                    try
                    {
                        const auto v = std::stoll(stringValue);
                        constexpr auto minV = static_cast<int64_t>((std::numeric_limits<T>::min)());
                        constexpr auto maxV = static_cast<int64_t>((std::numeric_limits<T>::max)());
                        if ((v >= minV) && (v <= maxV))
                        {
                            return static_cast<T>(v);
                        }
                    }
                    catch (std::invalid_argument&)
                    {
                    }
                    catch (std::out_of_range&)
                    {
                    }
                    SPX_DBG_TRACE_VERBOSE("Error parsing property %s (value=%s)", name, stringValue.c_str());
                }

                return Maybe<T>{};
            });
    }

    template<typename T, std::enable_if_t<std::is_unsigned<T>::value && !std::is_same<bool, T>::value, int> = 0>
    Maybe<T> Get(const char* name) const
    {
        return Get<std::string>(name).Map<T>([&](const std::string& stringValue) -> Maybe<T>
            {
                try
                {
                    // Suppress conversion of input beginning with '-', as C++ conversion functions by spec will accept
                    auto firstValidPos = stringValue.find_first_of("+-0123456789");
                    if (firstValidPos != std::string::npos && stringValue[firstValidPos] != '-')
                    {
                        const auto v = std::stoull(stringValue);
                        constexpr auto minV = static_cast<uint64_t>((std::numeric_limits<T>::min)());
                        constexpr auto maxV = static_cast<uint64_t>((std::numeric_limits<T>::max)());
                        if ((v >= minV) && (v <= maxV))
                        {
                            return static_cast<T>(v);
                        }
                    }
                }
                catch (std::invalid_argument&)
                {
                }
                catch (std::out_of_range&)
                {
                }
                SPX_DBG_TRACE_VERBOSE("Error parsing property %s (value=%s)", name, stringValue.c_str());
                return Maybe<T>{};
            });
    }

    template<typename T, std::enable_if_t<std::is_floating_point<T>::value, int> = 0>
    Maybe<T> Get(const char* name) const
    {
        return Get<std::string>(name).Map<T>([&](auto& stringValue) -> Maybe<T>
            {
                if (!stringValue.empty())
                {
                    try
                    {
                        auto doubleVal = std::stod(stringValue);
                        return static_cast<T>(doubleVal);
                    }
                    catch(const std::invalid_argument&) {}
                    catch(const std::out_of_range&) {}
                    SPX_DBG_TRACE_VERBOSE("Error parsing property %s (value=%s)", name, stringValue.c_str());
                }

                return Maybe<T>{};
            });
    }

    template<typename T, std::enable_if_t<std::is_same<bool, T>::value, int> = 0>
    Maybe<T> Get(const char* name) const
    {
        return Get<std::string>(name).Map<T>([](const std::string& stringValue) -> Maybe<T>
            {
                return PAL::ToBool(stringValue);
            });
    }

    template<typename T, std::enable_if_t<IsSpecialization<T, std::chrono::duration>::value, int> = 0>
    Maybe<T> Get(const char* name) const
    {
        return Get<int64_t>(name).Map<T>([](int64_t value) -> Maybe<T>
            {
                return T{ value };
            });
    }

    template<typename T, std::enable_if_t<std::is_enum<T>::value, int> = 0>
    Maybe<T> Get(const char* name) const
    {
        using U = typename std::underlying_type_t<T>;
        return Get<U>(name).template Map<T>([](const auto& val) -> Maybe<T>
        {
            return static_cast<T>(val);
        });
    }

    template<typename T = std::string>
    Maybe<T> Get(PropertyId propertyId) const
    {
        return Get<T>(GetPropertyName(propertyId));
    }

    template<typename T = std::string, typename U>
    T GetOr(const char* name, U && defaultValue) const
    {
        return Get<T>(name).GetOr(std::forward<U>(defaultValue));
    }

    template<typename T = std::string, typename U>
    T GetOr(PropertyId propertyId, U && defaultValue) const
    {
        return GetOr<T>(GetPropertyName(propertyId), std::forward<U>(defaultValue));
    }

    Maybe<std::vector<std::string>> GetList(const char* propertyName, const char delimiter)
    {
        if (auto maybePropertyValue = Get<std::string>(propertyName))
        {
            return PAL::split(maybePropertyValue.Get(), delimiter);
        }
        return Maybe<std::vector<std::string>>{};
    }

    Maybe<std::vector<std::string>> GetList(PropertyId propertyId, const char delimiter)
    {
        return GetList(GetPropertyName(propertyId), delimiter);
    }

    void Set(const char* propertyName, const char* value)
    {
        SetStringValue(propertyName, value);
    }

    void Set(const char* propertyName, const std::string& value)
    {
        SetStringValue(propertyName, value.c_str());
    }

    template<typename T, std::enable_if_t<!std::is_enum<std::decay_t<T>>::value && !std::is_class<std::decay_t<T>>::value, int> = 0>
    void Set(const char* propertyName, T&& value)
    {
        auto valueAsString = std::to_string(value);
        SetStringValue(propertyName, valueAsString.c_str());
    }

    template<typename T, std::enable_if_t<IsSpecialization<std::decay_t<T>, std::chrono::duration>::value, int> = 0>
    void Set(const char* propertyName, T&& value)
    {
        Set(propertyName, value.count());
    }

    template<typename T, std::enable_if_t<std::is_enum<std::decay_t<T>>::value, int> = 0>
    void Set(const char* propertyName, T&& value)
    {
        using U = typename std::underlying_type_t<std::decay_t<T>>;
        Set<U>(propertyName, static_cast<U>(value));
    }

    template<typename T>
    void Set(PropertyId propertyId, T&& value)
    {
        auto propertyName = GetPropertyName(propertyId);
        Set(propertyName, std::forward<T>(value));
    }

    void SetAsDefault(PropertyId propertyId, const char* value)
    {
        auto maybeExistingValue = Get<std::string>(propertyId);
        if (!maybeExistingValue || maybeExistingValue.Get().empty())
        {
            Set(propertyId, value);
        }
    }

    void SetAsDefault(const char* propertyName, const char* value)
    {
        auto maybeExistingValue = Get<std::string>(propertyName);
        if (!maybeExistingValue || maybeExistingValue.Get().empty())
        {
            SetStringValue(propertyName, value);
        }
    }

protected:

    static bool IsMatch(const char* name, bool fullMatch, const std::regex * pattern, const char* check)
    {
        auto matchEverything = name == nullptr && pattern == nullptr;
        if (matchEverything) return true;

        auto matchName = name && (fullMatch
            ? strcmp(name, check) == 0
            : strncmp(name, check, strlen(name)) == 0);
        if (matchName) return true;

        auto checkLen = strlen(check);
        auto matchPattern = pattern && (fullMatch
            ? std::regex_match(check, check + checkLen, *pattern, std::regex_constants::match_any)
            : std::regex_search(check, *pattern, std::regex_constants::match_any));
        if (matchPattern) return true;

        return false;
    }

    static bool ContinueMatching(const char* name, const std::string& value, VariantValue* output1, std::multimap<std::string, VariantValue>* outputAll)
    {
        return ContinueMatching(name, VariantValue::From(value.c_str()), output1, outputAll);
    }

    static bool ContinueMatching(const char* name, const VariantValue& value, VariantValue* output1, std::multimap<std::string, VariantValue>* outputAll)
    {
        if (output1 != nullptr)
        {
            *output1 = value;
        }

        if (outputAll != nullptr)
        {
            outputAll->emplace(name, value);
            return true;
        }

        return false;
    }

    void LogPropertyAndValue(std::string name, std::string value, const char* function) const
    {
        if (!value.empty())
        {
            std::vector<std::string> maskedPropertyNames =
            {
                GetPropertyName(PropertyId::SpeechServiceConnection_Key),
                GetPropertyName(PropertyId::SpeechServiceAuthorization_Token),
                GetPropertyName(PropertyId::Conversation_ApplicationId),
                GetPropertyName(PropertyId::SpeechServiceConnection_RecoModelKey),
                GetPropertyName(PropertyId::SpeechServiceConnection_SynthModelKey),
                GetPropertyName(PropertyId::SpeechTranslation_ModelKey),
                GetPropertyName(PropertyId::KeywordRecognition_ModelKey),
                GetPropertyName(PropertyId::AudioProcessing_PersonalizedNoiseSuppressionModelLicense),
                "service.auth.key",
                "service.auth.token",
                "embedded.ocrmodelkey",
            };

            if (maskedPropertyNames.end() != std::find(maskedPropertyNames.begin(), maskedPropertyNames.end(), name))
            {
                // Mask some sensitive property values, by leaving only the last two chars visible
                int l = value.length() > 2 ? 2 : 0;
                value.replace(value.begin(), value.end() - l, value.length() - l, '*');
            }
            else
            {
                std::vector<std::string> hiddenPropertyNames =
                {
                    GetPropertyName(PropertyId::SpeechServiceConnection_ProxyPassword),
                    GetPropertyName(PropertyId::SpeechServiceConnection_ProxyUserName)
                };

                if (hiddenPropertyNames.end() != std::find(hiddenPropertyNames.begin(), hiddenPropertyNames.end(), name))
                {
                    // Completely hide other property values
                    value = std::string("set to non-empty string");
                }
            }
        }

        SPX_DBG_TRACE_VERBOSE("%s: this=%p; name='%s'; value='%s'", function, (void*)this, name.c_str(), value.c_str()); UNUSED(function);
    }
};

#define SPX_NAMED_PROPERTY_MAP_BEGIN() bool Match(                                                                                          \
        const char* name, bool fullMatch,const std::regex* pattern,                                                                         \
        VariantValue* output1, std::multimap<std::string, VariantValue>* outputAll, ISpxNamedProperties::NoMatchContinueStrategy strategy,    \
        const ISpxNamedProperties* context) const override {                                                                                \
            UNUSED(name); UNUSED(fullMatch); UNUSED(pattern);                                                                               \
            UNUSED(output1); UNUSED(outputAll); UNUSED(strategy);                                                                           \
            UNUSED(context);                                                                                                                \
            bool found = false;

#define SPX_NAMED_PROPERTY_MAP_ENTRY_IF(entryName, cond, expr)                                                                      \
        if (cond && IsMatch(name, fullMatch, pattern, entryName)) {                                                                 \
            if (!ContinueMatching(name, expr, output1, outputAll)) {                                                                \
                return true;                                                                                                        \
            }                                                                                                                       \
            found = true;                                                                                                           \
        }

#define SPX_NAMED_PROPERTY_MAP_ENTRY_REGEX(regexPtr, expr)           \
        if (IsMatch(nullptr, fullMatch, regexPtr, name)) {           \
            if (!ContinueMatching(name, expr, output1, outputAll)) { \
                return true;                                         \
            }                                                        \
            found = true;                                            \
        }

#define SPX_NAMED_PROPERTY_MAP_ENTRY(entryName, expr)               SPX_NAMED_PROPERTY_MAP_ENTRY_IF(entryName, true, expr)
#define SPX_NAMED_PROPERTY_MAP_ENTRY_ID(id, expr)                   SPX_NAMED_PROPERTY_MAP_ENTRY_IF(GetPropertyName(id), true, expr)
#define SPX_NAMED_PROPERTY_MAP_ENTRY_PROPID(id, expr)               SPX_NAMED_PROPERTY_MAP_ENTRY_IF(GetPropertyName(PropertyId::id), true, expr)

#define SPX_NAMED_PROPERTY_MAP_FUNC_STRATEGY(func, strategy2) {                                                         \
            if (func(name, fullMatch, pattern, output1, outputAll, strategy2, context)) {                               \
                if (outputAll == nullptr) return true;                                                                  \
                found = true;                                                                                           \
        } }

#define SPX_NAMED_PROPERTY_MAP_FUNC_ENTRY_STRATEGY(entryName, func, strategy2) {                                        \
        if (strcmp(name, entryName) == 0) {                                                                             \
            SPX_NAMED_PROPERTY_MAP_FUNC_STRATEGY(func, strategy2)                                                       \
        } }

#define SPX_NAMED_PROPERTY_MAP_FUNC_ENTRY(entryName, func)          SPX_NAMED_PROPERTY_MAP_FUNC_ENTRY_STRATEGY(entryName, func, strategy)
#define SPX_NAMED_PROPERTY_MAP_FUNC(func)                           SPX_NAMED_PROPERTY_MAP_FUNC_STRATEGY(func, strategy)
#define SPX_NAMED_PROPERTY_MAP_FUNC_CHILD(func)                     SPX_NAMED_PROPERTY_MAP_FUNC_STRATEGY(func, ISpxNamedProperties::NoMatchContinueStrategy::Down)
#define SPX_NAMED_PROPERTY_MAP_FUNC_ENTRY_CHILD(entryName, func)    SPX_NAMED_PROPERTY_MAP_FUNC_ENTRY_STRATEGY(entryName, func, ISpxNamedProperties::NoMatchContinueStrategy::Down)

#define SPX_NAMED_PROPERTY_MAP_OBJECT_STRATEGY(obj, strategy2) {                                                        \
            if (obj != nullptr && obj->Match(name, fullMatch, pattern, output1, outputAll, strategy2, context)) {       \
                if (outputAll == nullptr) return true;                                                                  \
                found = true;                                                                                           \
        } }

#define SPX_NAMED_PROPERTY_MAP_OBJECT_ENTRY_STRATEGY(entryName, obj, strategy2) {                                       \
        if (entryName == nullptr || strcmp(name, entryName) == 0) {                                                     \
            SPX_NAMED_PROPERTY_MAP_OBJECT_STRATEGY(obj, strategy2)                                                      \
        } }

#define SPX_NAMED_PROPERTY_MAP_OBJECT_ENTRY(entryName, obj)          SPX_NAMED_PROPERTY_MAP_OBJECT_ENTRY_STRATEGY(entryName, obj, strategy)
#define SPX_NAMED_PROPERTY_MAP_OBJECT(obj)                           SPX_NAMED_PROPERTY_MAP_OBJECT_STRATEGY(obj, strategy)

#define SPX_NAMED_PROPERTY_MAP_OBJECT_CHILD(obj) {                                                                      \
        std::shared_ptr<ISpxNamedProperties> ptr = obj;                                                                 \
        if (context != ptr.get()) {                                                                                     \
            SPX_NAMED_PROPERTY_MAP_OBJECT_STRATEGY(ptr, ISpxNamedProperties::NoMatchContinueStrategy::Down)             \
        } }

#define SPX_NAMED_PROPERTY_MAP_OBJECT_ENTRY_CHILD(entryName, obj) {                                                     \
        std::shared_ptr<ISpxNamedProperties> ptr = obj;                                                                 \
        if (context != ptr.get()) {                                                                                     \
            SPX_NAMED_PROPERTY_MAP_OBJECT_ENTRY_STRATEGY(entryName, ptr, ISpxNamedProperties::NoMatchContinueStrategy::Down) \
        } }

#define SPX_NAMED_PROPERTY_MAP_END()                                                                                    \
        return found;                                                                                                   \
    }

}}}}
