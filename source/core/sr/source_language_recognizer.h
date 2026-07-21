//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
//

#pragma once
#include "ispxinterfaces.h"
#include "interface_helpers.h"
#include "recognizer.h"


namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

class CSpxSourceLanguageRecognizer : public CSpxRecognizer, public ISpxSourceLanguageRecognizer
{
public:

    CSpxSourceLanguageRecognizer();
    ~CSpxSourceLanguageRecognizer();

    SPX_INTERFACE_MAP_BEGIN()
        SPX_INTERFACE_MAP_ENTRY(ISpxObjectWithSite)
        SPX_INTERFACE_MAP_ENTRY(ISpxObjectInit)
        SPX_INTERFACE_MAP_ENTRY(ISpxSessionFromRecognizer)
        SPX_INTERFACE_MAP_ENTRY(ISpxNamedProperties)
        SPX_INTERFACE_MAP_ENTRY(ISpxRecognizerEvents)
        SPX_INTERFACE_MAP_ENTRY(ISpxRecognizer)
        SPX_INTERFACE_MAP_ENTRY(ISpxSourceLanguageRecognizer)
        SPX_INTERFACE_MAP_ENTRY(ISpxServiceProvider)
        SPX_INTERFACE_MAP_ENTRY(ISpxConnectionFromRecognizer)
        SPX_INTERFACE_MAP_ENTRY(ISpxMessageParamFromUser)
        SPX_INTERFACE_MAP_ENTRY(ISpxGetUspMessageParamsFromUser)
        SPX_INTERFACE_MAP_ENTRY(ISpxGrammarList)
        SPX_INTERFACE_MAP_ENTRY(ISpxGrammar)
        SPX_INTERFACE_MAP_ENTRY(ISpxGenericSite)
    SPX_INTERFACE_MAP_END()

    // --- ISpxObjectInit (overrides) ---
    void Init() override;

    // --- IServiceProvider
    SPX_SERVICE_MAP_BEGIN()
        SPX_SERVICE_MAP_ENTRY(ISpxNamedProperties)
        SPX_SERVICE_MAP_ENTRY_SITE(GetSite())
        SPX_SERVICE_MAP_END()

private:

    CSpxSourceLanguageRecognizer(CSpxSourceLanguageRecognizer&&) = delete;
    CSpxSourceLanguageRecognizer(const CSpxSourceLanguageRecognizer&) = delete;
    CSpxSourceLanguageRecognizer& operator=(CSpxSourceLanguageRecognizer&&) = delete;
    CSpxSourceLanguageRecognizer& operator=(const CSpxSourceLanguageRecognizer&) = delete;
};

} } } } // Microsoft::CognitiveServices::Speech::Impl
