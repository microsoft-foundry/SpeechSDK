//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include <array>
#include "spxcore_common.h"
#include "interface_helpers.h"
#include "service_helpers.h"
#include "property_bag_impl.h"
#include "ispxinterfaces.h"
#include <object_with_site_init_impl.h>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

class CSpxPronunciationAssessmentConfig :
    public ISpxObjectWithSiteInitImpl<ISpxGenericSite>,
    public ISpxServiceProvider,
    public ISpxGenericSite,
    public ISpxPropertyBagImpl,
    public ISpxPronunciationAssessmentConfig
{
public:
    CSpxPronunciationAssessmentConfig() {}

    SPX_INTERFACE_MAP_BEGIN()
        SPX_INTERFACE_MAP_ENTRY(ISpxObjectWithSite)
        SPX_INTERFACE_MAP_ENTRY(ISpxObjectInit)
        SPX_INTERFACE_MAP_ENTRY(ISpxServiceProvider)
        SPX_INTERFACE_MAP_ENTRY(ISpxNamedProperties)
        SPX_INTERFACE_MAP_ENTRY(ISpxPronunciationAssessmentConfig)
        SPX_INTERFACE_MAP_ENTRY(ISpxGenericSite)
    SPX_INTERFACE_MAP_END()

    // --- ISpxPronunciationAssessmentConfig ---
    void InitWithParameters(const char* referenceText, PronunciationAssessmentGradingSystem gradingSystem,
              PronunciationAssessmentGranularity granularity, bool enableMiscue) override;
    void InitFromJson(const char* json) override;
    void UpdateJson() override;

    // --- IServiceProvider
    SPX_SERVICE_MAP_BEGIN()
        SPX_SERVICE_MAP_ENTRY(ISpxNamedProperties)
        SPX_SERVICE_MAP_ENTRY_SITE(GetSite())
    SPX_SERVICE_MAP_END()

private:
    bool m_init{ false };
    DISABLE_COPY_AND_MOVE(CSpxPronunciationAssessmentConfig);
};

struct PronunciationAssessment {

    static constexpr const char* invalidGradingSystem = "INVALID_GRADING_SYSTEM";
    static constexpr const char* fivePoint = "FivePoint";
    static constexpr const char* hundredMark = "HundredMark";
    static constexpr const char* invalidGranularity = "INVALID_GRANULARITY";
    static constexpr const char* phoneme = "Phoneme";
    static constexpr const char* word = "Word";
    static constexpr const char* fullText = "FullText";

    static constexpr std::array<const char*, 3> GradingSystem {{
        invalidGradingSystem,
        // PronunciationAssessmentGradingSystem::FivePoint
        fivePoint,
        // PronunciationAssessmentGradingSystem::HundredMark
        hundredMark
    }};

    static constexpr std::array<const char*, 4> Granularity {{
        invalidGranularity,
        // PronunciationAssessmentGranularity::Phoneme
        phoneme,
        // PronunciationAssessmentGranularity::Word
        word,
        // PronunciationAssessmentGranularity::FullText
        fullText
    }};
};

}}}} // Microsoft::CognitiveServices::Speech::Impl
