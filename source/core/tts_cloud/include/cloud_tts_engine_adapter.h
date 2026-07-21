//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// cloud_tts_engine_adapter.h: Implementation declarations for CSpxCloudTtsEngineAdapter C++ class
//

#pragma once
#include <memory>
#include "spxcore_common.h"
#include "ispxinterfaces.h"
#include "interface_helpers.h"
#include "property_bag_impl.h"
#include "http_endpoint_info.h"
#include <ajv.h>
#include <object_with_site_init_impl.h>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

namespace VoiceJson {
    constexpr auto locale = "Locale";
    constexpr auto name = "Name";
    constexpr auto localName = "LocalName";
    constexpr auto shortName = "ShortName";
    constexpr auto styleList = "StyleList";
    constexpr auto voiceType = "VoiceType";
}

class CSpxCloudTtsEngineAdapter :
    public ISpxObjectWithSiteInitImpl<ISpxTtsEngineAdapterSite>,
    public ISpxTtsEngineAdapter,
    public ISpxPropertyBagImpl
{
public:

    CSpxCloudTtsEngineAdapter() = default;
    virtual ~CSpxCloudTtsEngineAdapter() = default;

    SPX_INTERFACE_MAP_BEGIN()
        SPX_INTERFACE_MAP_ENTRY(ISpxObjectWithSite)
        SPX_INTERFACE_MAP_ENTRY(ISpxObjectInit)
        SPX_INTERFACE_MAP_ENTRY(ISpxTtsEngineAdapter)
    SPX_INTERFACE_MAP_END()

    // --- ISpxTtsEngineAdapter ---
    std::shared_ptr<ISpxSynthesisVoicesResult> GetVoices(const std::string& locale) override;
    void SetOutput(const std::shared_ptr<ISpxAudioOutput>& output) override;


protected:

    enum class RequestType {
        Synthesize = 1,
        VoicesList = 2
    };

    std::shared_ptr<ISpxNamedProperties> GetParentProperties() const override;

    static HttpEndpointInfo GetRequestEndpoint(std::shared_ptr<ISpxNamedProperties> properties, RequestType requestType);
    std::string ConstructUserAgent();


private:

    std::shared_ptr<ISpxVoiceInfo> CreateVoiceInfo(const ajv::JsonReader& obj) const;


protected:

    std::shared_ptr<ISpxAudioOutput> m_audioOutput;
    std::string m_requestFormatStr;
};

} } } } // Microsoft::CognitiveServices::Speech::Impl
