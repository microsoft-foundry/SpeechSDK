//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include <assert.h>
#include <cstring>
#include "stdafx.h"
#include "handle_table.h"
#include "handle_helpers.h"
#include "string_utils.h"
#include "speechapi_cxx_translation_result.h"
#include <interface_helpers.h>

using namespace Microsoft::CognitiveServices::Speech::Impl;

SPXAPI translation_text_result_get_translation_count(SPXRESULTHANDLE handle, size_t * size)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, size == nullptr);
    SPXAPI_INIT_HR_TRY(hr)
    {
        auto textResult = SpxHandleQueryInterface<ISpxRecognitionResult, ISpxTranslationRecognitionResult>(handle);
        auto& phrases = textResult->GetTranslationText();
        *size = phrases.size();
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI translation_text_result_get_translation(SPXRESULTHANDLE handle, size_t index, char * language, char * text, size_t * language_size, size_t * text_size)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, language_size == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, text_size == nullptr);
    SPXAPI_INIT_HR_TRY(hr)
    {
        auto textResult = SpxHandleQueryInterface<ISpxRecognitionResult, ISpxTranslationRecognitionResult>(handle);
        auto& phrases = textResult->GetTranslationText();
        SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, index > (phrases.size() + 1));
        const auto& entry = phrases.at(index);
        const auto& lang = std::get<0>(entry);
        const auto& txt = std::get<1>(entry);
        if ((language != nullptr) && (text != nullptr))
        {
            SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, *language_size == 0);
            SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, *text_size == 0);
            PAL::strcpy(language, *language_size, lang.c_str(), lang.size(), true);
            PAL::strcpy(text, *text_size, txt.c_str(), txt.size(), true);
        }
        else
        {
            *language_size = lang.size() + 1;
            *text_size = txt.size() + 1;
        }
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI translation_synthesis_result_get_audio_data(SPXRESULTHANDLE handle, uint8_t* audioBuffer, size_t* lengthPointer)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, lengthPointer == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto resulthandle = CSpxSharedPtrHandleTableManager::Get<ISpxRecognitionResult, SPXRESULTHANDLE>();
        auto result = (*resulthandle)[handle];

        auto audioResult = SpxQueryInterface<ISpxTranslationSynthesisResult>(result);
        auto audioLength = audioResult->GetLength();

        if (audioLength == 0)
        {
            *lengthPointer = audioLength;
            SPX_RETURN_HR(hr);
        }

        if ((audioBuffer == nullptr) || (*lengthPointer < audioLength))
        {
            *lengthPointer = audioLength;
            return SPXERR_BUFFER_TOO_SMALL;
        }

        SPX_THROW_HR_IF(SPXERR_RUNTIME_ERROR, audioResult->GetAudio() == nullptr);
        std::memcpy(audioBuffer, audioResult->GetAudio(), audioLength);
        *lengthPointer = audioLength;
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}
