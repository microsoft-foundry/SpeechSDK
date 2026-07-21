//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"
#include "pronunciation_assessment_config.h"
#include "property_id_2_name_map.h"
#include "usp.h"
#include <ajv.h>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

using namespace std;

constexpr std::array<const char *, 3> PronunciationAssessment::GradingSystem;
constexpr std::array<const char *, 4> PronunciationAssessment::Granularity;

void CSpxPronunciationAssessmentConfig::InitWithParameters(const char* referenceText, PronunciationAssessmentGradingSystem gradingSystem,
                          PronunciationAssessmentGranularity granularity, bool enableMiscue)
{
    SPX_THROW_HR_IF(SPXERR_ALREADY_INITIALIZED, m_init);
    m_init = true;
    auto gradingSystemValue = static_cast<size_t>(gradingSystem);
    SPX_THROW_HR_IF(SPXERR_INVALID_ARG, (gradingSystemValue > PronunciationAssessment::GradingSystem.size()));
    auto granularityValue = static_cast<size_t>(granularity);
    SPX_THROW_HR_IF(SPXERR_INVALID_ARG, (granularityValue > PronunciationAssessment::Granularity.size()));
    Set(PropertyId::PronunciationAssessment_ReferenceText, referenceText);

    try
    {
        Set(PropertyId::PronunciationAssessment_GradingSystem, PronunciationAssessment::GradingSystem[gradingSystemValue]);
        Set(PropertyId::PronunciationAssessment_Granularity, PronunciationAssessment::Granularity[granularityValue]);
    }
    catch(const std::exception& e)
    {
        SPX_TRACE_ERROR("%s", e.what());
        SPX_THROW_HR(SPXERR_INVALID_ARG);
    }

    if (enableMiscue)
    {
        Set(PropertyId::PronunciationAssessment_EnableMiscue, "true");
    }
}

void CSpxPronunciationAssessmentConfig::InitFromJson(const char* json)
{
    SPX_THROW_HR_IF(SPXERR_ALREADY_INITIALIZED, m_init);
    m_init = true;

    if (!ajv::json::Parse(json, strlen(json)).IsOk())
    {
        SPX_TRACE_ERROR("invalid json");
        SPX_THROW_HR(SPXERR_INVALID_ARG);
    }

    Set(PropertyId::PronunciationAssessment_Json, json);
}

void CSpxPronunciationAssessmentConfig::UpdateJson()
{
    const auto jsonString = GetOr(PropertyId::PronunciationAssessment_Json, "");
    auto parsed = ajv::json::Build(jsonString);
    auto paramsJson = parsed.Writer();

    // always set dimension to Comprehensive
    paramsJson["dimension"] = "Comprehensive";
    // n.b.: <<= means "assign the right to the left if and only if the right isn't empty/0/false/etc."
    paramsJson["enableMiscue"] <<= GetOr<bool>(PropertyId::PronunciationAssessment_EnableMiscue, false);
    paramsJson["referenceText"] <<= GetOr(PropertyId::PronunciationAssessment_ReferenceText, "");
    paramsJson["gradingSystem"] <<= GetOr(PropertyId::PronunciationAssessment_GradingSystem, "");
    paramsJson["granularity"] <<= GetOr(PropertyId::PronunciationAssessment_Granularity, "");
    paramsJson["enableProsodyAssessment"] <<= GetOr<bool>(PropertyId::PronunciationAssessment_EnableProsodyAssessment, false);
    paramsJson["phonemeAlphabet"] <<= GetOr(PropertyId::PronunciationAssessment_PhonemeAlphabet, "");
    paramsJson["nbestPhonemeCount"] <<= GetOr<int>(PropertyId::PronunciationAssessment_NBestPhonemeCount, 0);
    paramsJson["scenarioId"] <<= GetOr("PronunciationAssessment_ScenarioId", "");

    Set(PropertyId::PronunciationAssessment_Params, paramsJson.AsJson().c_str());
}

} } } } // Microsoft::CognitiveServices::Speech::Impl
