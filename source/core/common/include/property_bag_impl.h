//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// property_bag_impl.h: Implementation definitions for ISpxNamedPropertiesImpl C++ class
//

#pragma once
#include <map>
#include "ispxinterfaces.h"
#include "property_id_2_name_map.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

class ISpxPropertyBagImpl : public ISpxNamedProperties
{
public:

    // --- ISpxNamedProperties
    void SetStringValue(const char* name, const char* value) override;
    void SetBinaryValue(const char* name, std::shared_ptr<uint8_t> value, size_t size) override;
    bool Match(const char* name, bool fullMatch,const std::regex* pattern, VariantValue* output1, std::multimap<std::string, VariantValue>* outputAll, NoMatchContinueStrategy strategy, const ISpxNamedProperties* context) const override;

protected:

    virtual std::shared_ptr<ISpxNamedProperties> GetParentProperties() const { return nullptr; }

    bool MatchPropertyBagProperties(const char* name, bool fullMatch,const std::regex* pattern, VariantValue* output1, std::multimap<std::string, VariantValue>* outputAll, NoMatchContinueStrategy strategy, const ISpxNamedProperties* context) const;
    bool MatchParentProperties(const char* name, bool fullMatch,const std::regex* pattern, VariantValue* output1, std::multimap<std::string, VariantValue>* outputAll, NoMatchContinueStrategy strategy, const ISpxNamedProperties* context) const;

private:
    mutable std::mutex m_mutexProperties;
    std::map<std::string, VariantValue> m_propertyMap;

};

} } } } // Microsoft::CognitiveServices::Speech::Impl
