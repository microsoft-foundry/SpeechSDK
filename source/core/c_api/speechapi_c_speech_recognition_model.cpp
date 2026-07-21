//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// speechapi_c_speech_recognition_model.cpp: Definitions for SpeechRecognitionModel related C methods
//

#include "stdafx.h"
#include "speechapi_c_speech_recognition_model.h"
#include "result_helpers.h" // for get_char_property

using namespace Microsoft::CognitiveServices::Speech::Impl;

SPXAPI__(const char*) speech_recognition_model_get_name(SPXSPEECHRECOMODELHANDLE hmodel)
{
    return result_get_char_property<ISpxSpeechRecognitionModel>(hmodel, &ISpxSpeechRecognitionModel::GetName);
}

SPXAPI__(const char*) speech_recognition_model_get_locales(SPXSPEECHRECOMODELHANDLE hmodel)
{
    char* locales = nullptr;

    if (hmodel == nullptr)
    {
        return locales;
    }

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto modelInfo = SpxGetPtrFromHandle<ISpxSpeechRecognitionModel>(hmodel);
        const auto tempStr = PAL::Join(modelInfo->GetLocales(), "|");
        const auto size = tempStr.size() + 1;
        locales = new char[size];
        PAL::strcpy(locales, size, tempStr.c_str(), size, true);
    }
    SPXAPI_CATCH_AND_RETURN(hr, locales);
}

SPXAPI__(const char*) speech_recognition_model_get_path(SPXSPEECHRECOMODELHANDLE hmodel)
{
    return result_get_char_property<ISpxSpeechRecognitionModel>(hmodel, &ISpxSpeechRecognitionModel::GetPath);
}

SPXAPI__(const char*) speech_recognition_model_get_version(SPXSPEECHRECOMODELHANDLE hmodel)
{
    return result_get_char_property<ISpxSpeechRecognitionModel>(hmodel, &ISpxSpeechRecognitionModel::GetVersion);
}

SPXAPI speech_recognition_model_handle_release(SPXSPEECHRECOMODELHANDLE hmodel)
{
    return CSpxApiManager::ReleaseAlwaysNoError<SPXSPEECHRECOMODELHANDLE, ISpxSpeechRecognitionModel>(hmodel);
}
