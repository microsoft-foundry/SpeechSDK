//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// speech_synthesis_api_factory.h: Implementation declarations for CSpxSpeechSynthesisApiFactory C++ class
//

#pragma once
#include <spxdebug.h>
#include <spxerror.h>
#include "service_helpers.h"
#include "interface_helpers.h"
#include "property_bag_impl.h"
#include <object_with_site_init_impl.h>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

class CSpxSpeechSynthesisApiFactory :
    public ISpxObjectWithSiteInitImpl<ISpxGenericSite>,
    public ISpxServiceProvider,
    public ISpxSpeechSynthesisApiFactory,
    public ISpxGenericSite,
    public ISpxPropertyBagImpl
{
public:

    // debugging to make sure CSpxSpeechSynthesisApiFactory is released.
#if _DEBUG
    ~CSpxSpeechSynthesisApiFactory()
    {
        SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    }
#endif

    SPX_INTERFACE_MAP_BEGIN()
        SPX_INTERFACE_MAP_ENTRY(ISpxInterfaceBase)
        SPX_INTERFACE_MAP_ENTRY(ISpxObjectWithSite)
        SPX_INTERFACE_MAP_ENTRY(ISpxObjectInit)
        SPX_INTERFACE_MAP_ENTRY(ISpxServiceProvider)
        SPX_INTERFACE_MAP_ENTRY(ISpxSpeechSynthesisApiFactory)
        SPX_INTERFACE_MAP_ENTRY(ISpxGenericSite)
        SPX_INTERFACE_MAP_ENTRY(ISpxNamedProperties)
    SPX_INTERFACE_MAP_END()

    // --- ISpxSpeechSynthesisApiFactory
    std::shared_ptr<ISpxSynthesizer> CreateSpeechSynthesizerFromConfig(std::shared_ptr<ISpxAudioConfig> audioConfig) override;

    // --- IServiceProvider
    SPX_SERVICE_MAP_BEGIN()
    SPX_SERVICE_MAP_ENTRY(ISpxNamedProperties)
    SPX_SERVICE_MAP_ENTRY_SITE(GetSite())
    SPX_SERVICE_MAP_END()

protected:

    std::shared_ptr<ISpxNamedProperties> GetParentProperties() const override { return SpxQueryService<ISpxNamedProperties>(GetSite()); }
};


} } } } // Microsoft::CognitiveServices::Speech::Impl
