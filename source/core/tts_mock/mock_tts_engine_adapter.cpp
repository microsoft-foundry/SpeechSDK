//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// mock_tts_engine_adapter.cpp: Implementation definitions for CSpxMockTtsEngineAdapter C++ class
//

#include "stdafx.h"
#include <cmath>
#include <thread>
#include "synthesis_helper.h"
#include "mock_tts_engine_adapter.h"
#include "create_object_helpers.h"
#include "service_helpers.h"
#include "shared_ptr_helpers.h"
#include "property_bag_impl.h"
#include "property_id_2_name_map.h"
#include "create_object_helpers.h"
#include "site_helpers.h"

#define SPX_DBG_TRACE_MOCK_TTS 1


namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {


CSpxMockTtsEngineAdapter::CSpxMockTtsEngineAdapter()
{
    SPX_DBG_TRACE_VERBOSE_IF(SPX_DBG_TRACE_MOCK_TTS, __FUNCTION__);
}

CSpxMockTtsEngineAdapter::~CSpxMockTtsEngineAdapter()
{
    SPX_DBG_TRACE_VERBOSE_IF(SPX_DBG_TRACE_MOCK_TTS, __FUNCTION__);
}

void CSpxMockTtsEngineAdapter::SetOutput(const std::shared_ptr<ISpxAudioOutput>& output)
{
    SPX_DBG_TRACE_VERBOSE_IF(SPX_DBG_TRACE_MOCK_TTS, __FUNCTION__);
    m_audioOutput = output;
}

std::shared_ptr<ISpxSynthesisResult> CSpxMockTtsEngineAdapter::Speak(const std::string& text, bool isSsml, const std::string& requestId, bool retry)
{
    SPX_DBG_TRACE_VERBOSE_IF(SPX_DBG_TRACE_MOCK_TTS, __FUNCTION__);
    UNUSED(retry);

    auto outputFormat = SpxQueryInterface<ISpxAudioOutputFormat>(m_audioOutput);
    const auto formatString = outputFormat->GetFormatString();
    GetSite()->SetAdapterFormat(this, CSpxSynthesisHelper::GetSpeechSynthesisOutputFormatFromString(formatString));

    std::shared_ptr<ISpxSynthesisResult> result;

    InvokeOnSite([this, text, isSsml, requestId, &result](SitePtr p) {
        auto audioOutputStream = SpxCreateObjectWithSite<ISpxAudioOutput>("CSpxPullAudioOutputStream", SpxGetRootSite());

        auto ssml = text;
        std::shared_ptr<ISpxErrorInformation> ssmlError;
        if (!isSsml)
        {
            const auto properties = SpxQueryService<ISpxNamedProperties>(GetSite());
            std::tie(ssml, ssmlError) = CSpxSynthesisHelper::BuildSsml(text, properties);
        }
        result = p->CreateEmptySynthesisResult();
        auto resultInit = SpxQueryInterface<ISpxSynthesisResultInit>(result);

        if (ssmlError != nullptr)
        {
            resultInit->InitSynthesisResult(requestId, ResultReason::Canceled, ssmlError);
            return;
        }

        for (int i = 0; i < 10; ++i)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));

            // Generate sine wave as the mock audio
            auto audioBuffer = new uint8_t[3200];
            for (int j = 0; j < 1600; ++j)
            {
                auto x = double(i * 1600 + j) * 3.14159265359 * 2 * 400 / 16000; // 400Hz
                reinterpret_cast<int16_t*>(audioBuffer)[j] = static_cast<int16_t>(sin(x) * 16384);
            }

            p->Write(this, requestId, audioBuffer, 3200, nullptr);
            audioOutputStream->Write(audioBuffer, 3200);

            delete[] audioBuffer;
        }

        // Append SSML to the end of the audio, this is part of the mock strategy
        p->Write(this, requestId, (uint8_t *)(ssml.data()), static_cast<uint32_t>(ssml.length()), nullptr);
        audioOutputStream->Write((uint8_t *)(ssml.data()), static_cast<uint32_t>(ssml.length()));

        InvokeOnSite([this](const SitePtr& p) { p->EndOfTurn(this); });

        auto totalAudio = SpxAllocSharedAudioBuffer(32000 + static_cast<uint32_t>(ssml.length()));
        auto audioDataStream = SpxQueryInterface<ISpxAudioOutputReader>(audioOutputStream);
        SPX_DBG_ASSERT(audioDataStream != nullptr);
        audioDataStream->Read(totalAudio.get(), 32000 + static_cast<uint32_t>(ssml.length()));

        resultInit->InitSynthesisResult(requestId, ResultReason::SynthesizingAudioCompleted, nullptr);
    });

    return result;
}

std::shared_ptr<ISpxSynthesisResult> CSpxMockTtsEngineAdapter::Speak(std::shared_ptr<ISpxSynthesisRequestReader> request, bool retry)
{
    std::shared_ptr<ISpxSynthesisResult> result;
    switch (request->GetInputType())
    {
        case SynthesisRequestInputType::Text:
            return Speak(request->GetInputContent(), false, request->GetRequestId(), retry);
        case SynthesisRequestInputType::SSML:
            return Speak(request->GetInputContent(), true, request->GetRequestId(), retry);
        case SynthesisRequestInputType::TextStream:
            InvokeOnSite([request, &result](const SitePtr &p)
            {
                auto error = ErrorInfo::FromExplicitError(CancellationErrorCode::BadRequest,
                                                          "Text steam is not supported by mock TTS.");
                result = p->CreateEmptySynthesisResult();
                auto resultInit = SpxQueryInterface<ISpxSynthesisResultInit>(result);
                resultInit->InitSynthesisResult(request->GetRequestId(), ResultReason::Canceled, error);
            });
            return result;
        default:
            SPX_THROW_HR(SPXERR_INVALID_ARG);
    }
}

void CSpxMockTtsEngineAdapter::StopSpeaking(const std::shared_ptr<ISpxErrorInformation> &)
{
    SPX_DBG_TRACE_VERBOSE_IF(SPX_DBG_TRACE_MOCK_TTS, __FUNCTION__);
}

void CSpxMockTtsEngineAdapter::Connect()
{
    SPX_DBG_TRACE_VERBOSE_IF(SPX_DBG_TRACE_MOCK_TTS, __FUNCTION__);
    InvokeOnSite([this](const SitePtr &p)
                 { p->FireAdapterResult_ConnectionChanged(this, true); });
}

void CSpxMockTtsEngineAdapter::Disconnect(bool)
{
    SPX_DBG_TRACE_VERBOSE_IF(SPX_DBG_TRACE_MOCK_TTS, __FUNCTION__);
    InvokeOnSite([this](const SitePtr &p)
                 { p->FireAdapterResult_ConnectionChanged(this, false); });
}

std::shared_ptr<ISpxSynthesisVoicesResult> CSpxMockTtsEngineAdapter::GetVoices(const std::string& locale)
{
    UNUSED(locale);
    return nullptr;
}

std::shared_ptr<ISpxNamedProperties> CSpxMockTtsEngineAdapter::GetParentProperties() const
{
    return SpxQueryService<ISpxNamedProperties>(GetSite());
}

} } } } // Microsoft::CognitiveServices::Speech::Impl
