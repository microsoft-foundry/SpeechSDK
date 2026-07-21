//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// speechapi_c_speech_translation_model.cpp: Definitions for SpeechTranslationModel related C methods
//

#include "stdafx.h"
#include "speechapi_c_speech_translation_model.h"
#include "result_helpers.h" // for get_char_property

using namespace Microsoft::CognitiveServices::Speech::Impl;

SPXAPI__(const char*) speech_translation_model_get_name(SPXSPEECHRECOMODELHANDLE hmodel)
{
    return result_get_char_property<ISpxSpeechTranslationModel>(hmodel, &ISpxSpeechTranslationModel::GetName);
}

SPXAPI__(const char*) speech_translation_model_get_source_languages(SPXSPEECHRECOMODELHANDLE hmodel)
{
    char* sourceLanguages = nullptr;

    if (hmodel == nullptr)
    {
        return sourceLanguages;
    }

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto modelInfo = SpxGetPtrFromHandle<ISpxSpeechTranslationModel>(hmodel);
        const auto tempStr = PAL::Join(modelInfo->GetSourceLanguages(), "|");
        const auto size = tempStr.size() + 1;
        sourceLanguages = new char[size];
        PAL::strcpy(sourceLanguages, size, tempStr.c_str(), size, true);
    }
    SPXAPI_CATCH_AND_RETURN(hr, sourceLanguages);
}

SPXAPI__(const char*) speech_translation_model_get_target_languages(SPXSPEECHRECOMODELHANDLE hmodel)
{
    char* targetLanguages = nullptr;

    if (hmodel == nullptr)
    {
        return targetLanguages;
    }

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto modelInfo = SpxGetPtrFromHandle<ISpxSpeechTranslationModel>(hmodel);
        const auto tempStr = PAL::Join(modelInfo->GetTargetLanguages(), "|");
        const auto size = tempStr.size() + 1;
        targetLanguages = new char[size];
        PAL::strcpy(targetLanguages, size, tempStr.c_str(), size, true);
    }
    SPXAPI_CATCH_AND_RETURN(hr, targetLanguages);
}

SPXAPI__(const char*) speech_translation_model_get_path(SPXSPEECHRECOMODELHANDLE hmodel)
{
    return result_get_char_property<ISpxSpeechTranslationModel>(hmodel, &ISpxSpeechTranslationModel::GetPath);
}

SPXAPI__(const char*) speech_translation_model_get_version(SPXSPEECHRECOMODELHANDLE hmodel)
{
    return result_get_char_property<ISpxSpeechTranslationModel>(hmodel, &ISpxSpeechTranslationModel::GetVersion);
}

SPXAPI speech_translation_model_handle_release(SPXSPEECHRECOMODELHANDLE hmodel)
{
    return CSpxApiManager::ReleaseAlwaysNoError<SPXSPEECHRECOMODELHANDLE, ISpxSpeechTranslationModel>(hmodel);
}
