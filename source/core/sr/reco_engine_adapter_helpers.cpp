//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include <vector>
#include <array>
#include "stdafx.h"
#include "reco_engine_adapter_helpers.h"
#include <chrono>
#include "property_id_2_name_map.h"

#include <ajv.h>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

using namespace std;

static void UpdateWordOffsets(ajv::JsonBuilder::JsonWriter& words, uint64_t offsetFixup)
{
    auto wordListSize = words.ValueCount();
    for (int wordIndex = 0; wordIndex < wordListSize; wordIndex++)
    {
        auto word = words[wordIndex];
        auto findOffset = word.ValueAt("Offset");
        if (findOffset.IsNumber())
        {
            uint64_t wordOffset = findOffset.AsUint<uint64_t>();
            words[wordIndex]["Offset"] = wordOffset + offsetFixup;
        }
    }
}

void CSpxRecoEngineAdapterHelpers::UpdateServiceResponseJsonResult(shared_ptr<ISpxRecognitionResult> result, uint64_t offsetFixup)
{
    auto namedProperties = SpxQueryInterface<ISpxNamedProperties>(result);
    auto jsonResult = namedProperties->GetOr(PropertyId::SpeechServiceResponse_JsonResult, "");
    if (jsonResult.empty())
    {
        return;
    }

    SPX_DBG_TRACE_VERBOSE("%s: before update: json='%s'", __FUNCTION__, jsonResult.c_str());
    auto root = ajv::json::Build(jsonResult);
    bool valueChanged = false;

    auto findOffset = root.ValueAt("Offset");
    if (!findOffset.IsEnd())
    {
        uint64_t oldOffset = findOffset.AsUint<uint64_t>();
        uint64_t newOffset = oldOffset + offsetFixup;
        if (oldOffset != newOffset)
        {
            root["Offset"] = newOffset;
            valueChanged = true;
        }
    }
    auto findNBest = root.ValueAt("NBest");
    if (findNBest.IsArray())
    {
        auto nBestSize = findNBest.ValueCount();
        for (int index = 0; index < nBestSize; index++)
        {
            auto item = findNBest[index];
            if (index == 0)
            {
                auto findITN = item.ValueAt("ITN");
                if (findITN.IsString())
                {
                    namedProperties->SetStringValue("ITN", findITN.AsString().c_str());
                }
                auto findLexical = item.ValueAt("Lexical");
                if (findLexical.IsString())
                {
                    namedProperties->SetStringValue("Lexical", findLexical.AsString().c_str());
                }

                auto findPron = item.ValueAt("PronunciationAssessment");
                if (findPron.IsObject())
                {
                    for (auto iter = findPron.FirstValue(); !iter.IsEnd(); iter++)
                    {
                        if (iter.IsNumber())
                        {
                            auto name = iter.Name().AsString();
                            auto value = iter.AsJson();
                            namedProperties->SetStringValue(name.c_str(), value.c_str());
                        }
                    }
                }
            }
            auto indexesToUpdate = { "Words", "DisplayWords" };

            for_each(indexesToUpdate.begin(), indexesToUpdate.end(), [&item, &valueChanged, offsetFixup](const char* oneIndex)
                {
                    auto findWords = item.ValueAt(oneIndex);
                    if (findWords.IsArray())
                    {
                        valueChanged = true;
                        UpdateWordOffsets(findWords, offsetFixup);
                    }
                });
        }
    }

    if (!namedProperties->HasStringValue("Lexical"))
    {
        auto findLexical = root.ValueAt("Lexical");
        if (findLexical.IsString())
        {
            namedProperties->SetStringValue("Lexical", findLexical.AsString().c_str());
        }
    }

    if (!namedProperties->HasStringValue("ITN"))
    {
        auto findITN = root.ValueAt("ITN");
        if (findITN.IsString())
        {
            namedProperties->SetStringValue("ITN", findITN.AsString().c_str());
        }
    }

    auto findSNR = root.ValueAt("SNR");
    if (findSNR.IsNumber())
    {
        namedProperties->SetStringValue("SNR", findSNR.AsJson().c_str());
    }

    // Translation result
    auto findTranslation = root.ValueAt("Translation");
    if (findTranslation.IsContainer())
    {
        auto findTranscribedWords = root.ValueAt("Words");
        if (findTranscribedWords.IsArray())
        {
            valueChanged = true;
            UpdateWordOffsets(findTranscribedWords, offsetFixup);
        }

        auto findTranslations = findTranslation.ValueAt("Translations");
        if (findTranslations.IsArray())
        {
            auto numTranslations = findTranslations.ValueCount();
            for (int index = 0; index < numTranslations; index++)
            {
                auto findWords = findTranslations[index].ValueAt("Words");
                if (findWords.IsArray())
                {
                    valueChanged = true;
                    UpdateWordOffsets(findWords, offsetFixup);
                }
            }
        }
    }

    if (valueChanged)
    {
        string updatedJsonStr = root.AsJson();
        SPX_DBG_TRACE_VERBOSE("%s: after update: json='%s'", __FUNCTION__, updatedJsonStr.c_str());
        namedProperties->Set(PropertyId::SpeechServiceResponse_JsonResult, updatedJsonStr.c_str());
    }
}

template<> bool EnumHelpers::TryParse<SegmentationStrategy>(const char* string, SegmentationStrategy& value)
{
    ENUM_PARSE(SegmentationStrategy::Default, g_segmentationStrategyDefault);
    ENUM_PARSE(SegmentationStrategy::Time, g_segmentationStrategyTime);
    ENUM_PARSE(SegmentationStrategy::Semantic, g_segmentationStrategySemantic);
    return false;
}

} } } } // Microsoft::CognitiveServices::Speech::Impl
