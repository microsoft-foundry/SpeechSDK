//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// property_bag_impl.cpp: Implementation definitions for ISpxNamedPropertiesImpl C++ class
//

#include "stdafx.h"
#include "property_bag_impl.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

void ISpxPropertyBagImpl::SetStringValue(const char* name, const char* value)
{
    std::unique_lock<std::mutex> lock(m_mutexProperties);
    SPX_THROW_HR_IF(SPXERR_INVALID_ARG, name == nullptr);
    SPX_THROW_HR_IF(SPXERR_INVALID_ARG, value == nullptr);

    m_propertyMap[std::string(name)] = VariantValue::From(value);
    LogPropertyAndValue(name, value, "ISpxPropertyBagImpl::SetStringValue");
}

void ISpxPropertyBagImpl::SetBinaryValue(const char* name, std::shared_ptr<uint8_t> value, size_t size)
{
    std::unique_lock<std::mutex> lock(m_mutexProperties);
    SPX_THROW_HR_IF(SPXERR_INVALID_ARG, name == nullptr);
    SPX_THROW_HR_IF(SPXERR_INVALID_ARG, value == nullptr);

    m_propertyMap[std::string(name)] = VariantValue::From(value, size);
    LogPropertyAndValue(name, "BINARY (" + std::to_string(size) + " bytes)", "ISpxPropertyBagImpl::SetBinaryValue");
}

bool ISpxPropertyBagImpl::Match(const char* name, bool fullMatch, const std::regex* pattern, VariantValue* output1, std::multimap<std::string, VariantValue>* outputAll, NoMatchContinueStrategy strategy, const ISpxNamedProperties* context) const
{
    // Check the property bag first... if found, AND we don't need to find all the outputAll, return true
    bool foundInBag = MatchPropertyBagProperties(name, fullMatch, pattern, output1, outputAll, strategy, context);
    if (foundInBag && outputAll == nullptr) return true;

    // Check the parent... (even if we found it in the bag, and we're looking for all the outputAll)
    bool foundInParent = MatchParentProperties(name, fullMatch, pattern, output1, outputAll, strategy, context);

    // We're successful if we found it in the bag or the parent
    return foundInBag || foundInParent;
}

bool ISpxPropertyBagImpl::MatchPropertyBagProperties(const char* name, bool fullMatch, const std::regex* pattern, VariantValue* output1, std::multimap<std::string, VariantValue>* outputAll, NoMatchContinueStrategy strategy, const ISpxNamedProperties* context) const
{
    UNUSED(strategy); UNUSED(context);
    std::unique_lock<std::mutex> lock(m_mutexProperties);

    // STEP 1: if we're only looking for the first full match by name and not pattern...
    if (name && fullMatch && pattern == nullptr && output1 != nullptr && outputAll == nullptr)
    {
        auto item = m_propertyMap.find(std::string(name));
        if (item != m_propertyMap.end())
        {
            *output1 = item->second;
            return true;
        }
    }

    // STEP 2: loop thru the map, checking each entry
    bool foundInBag = false;
    for (const auto& item : m_propertyMap)
    {
        auto matched = IsMatch(name, fullMatch, pattern, item.first.c_str());
        if (!matched) continue;

        // record the matched item and continue, if we're looking for more than one match
        if (ContinueMatching(item.first.c_str(), item.second, output1, outputAll))
        {
            foundInBag = true;
            continue;
        }

        return true;
    }

    return foundInBag;
}

bool ISpxPropertyBagImpl::MatchParentProperties(const char* name, bool fullMatch,const std::regex* pattern, VariantValue* output1, std::multimap<std::string, VariantValue>* outputAll, NoMatchContinueStrategy strategy, const ISpxNamedProperties* context) const
{
    if (strategy == NoMatchContinueStrategy::Up)
    {
        auto parent = GetParentProperties();
        if (parent != nullptr && parent.get() != context)
        {
            return parent->Match(name, fullMatch, pattern, output1, outputAll, strategy, context);
        }
    }

    return false;
}

} } } } // Microsoft::CognitiveServices::Speech::Impl
