//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// mock_vad_engine_adapter.cpp: Implementation definitions for CSpxMockVadEngineAdapter C++ class
//

#include "stdafx.h"
#include <cstring>
#include "mock_vad_engine_adapter.h"
#include "service_helpers.h"
#include "try_catch_helpers.h"
#include "mock_controller.h"
#include "../common/file_utils.cpp"
#include <property_id_2_name_map.h>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

CSpxMockVadEngineAdapter::CSpxMockVadEngineAdapter() :
    m_cbAudioProcessed(0),
    m_cbFireVAD(0)
{
    SPX_DBG_TRACE_FUNCTION();
}

CSpxMockVadEngineAdapter::~CSpxMockVadEngineAdapter()
{
    SPX_DBG_TRACE_FUNCTION();
}

void CSpxMockVadEngineAdapter::Init()
{
    SPX_DBG_TRACE_FUNCTION();
    SPX_THROW_HR_IF(SPXERR_UNINITIALIZED, GetSite() == nullptr);

    auto properties = SpxQueryService<ISpxNamedProperties>(GetSite());
    properties->SetStringValue(KeywordConfig_EnableKeywordVerification, "false");
}

void CSpxMockVadEngineAdapter::Term()
{
    SPX_DBG_TRACE_FUNCTION();
}

void CSpxMockVadEngineAdapter::SetFormat(const SPXWAVEFORMATEX* pformat)
{
    SPX_TRACE_VERBOSE_IF(pformat == nullptr, "%s - pformat == nullptr", __FUNCTION__);
    SPX_TRACE_VERBOSE_IF(pformat != nullptr, "%s\n  wFormatTag:      %s\n  nChannels:       %d\n  nSamplesPerSec:  %d\n  nAvgBytesPerSec: %d\n  nBlockAlign:     %d\n  wBitsPerSample:  %d\n  cbSize:          %d",
        __FUNCTION__,
        pformat->wFormatTag == WAVE_FORMAT_PCM ? "PCM" : std::to_string(pformat->wFormatTag).c_str(),
        pformat->nChannels,
        pformat->nSamplesPerSec,
        pformat->nAvgBytesPerSec,
        pformat->nBlockAlign,
        pformat->wBitsPerSample,
        pformat->cbSize);

    SPX_THROW_HR_IF(SPXERR_ALREADY_INITIALIZED, pformat != nullptr && HasFormat());

    if (pformat != nullptr)
    {
        InitFormat(pformat);
    }
    else
    {
        TermFormat();
        End();
    }
}

void CSpxMockVadEngineAdapter::ProcessAudio(const DataChunkPtr& audioChunk)
{
    SPX_DBG_TRACE_VERBOSE_IF(0, "%s(..., size=%d)", __FUNCTION__, audioChunk->size);
    SPX_THROW_HR_IF(SPXERR_UNINITIALIZED, !HasFormat());

    m_audio.push_front(audioChunk);
    m_cbAudioProcessed += audioChunk->size;
    SPX_IFTRUE(m_cbAudioProcessed >= m_cbFireVAD, FireVoiceActivityDetected());
}

void CSpxMockVadEngineAdapter::InitFormat(const SPXWAVEFORMATEX* pformat)
{
    SPX_THROW_HR_IF(SPXERR_ALREADY_INITIALIZED, HasFormat());

    auto sizeOfFormat = sizeof(SPXWAVEFORMATEX) + pformat->cbSize;
    m_format = SpxAllocWAVEFORMATEX(sizeOfFormat);
    memcpy(m_format.get(), pformat, sizeOfFormat);

    m_cbAudioProcessed = 0;
    m_cbFireVAD = (uint64_t)(m_numMsVADDetected) * m_format->nAvgBytesPerSec / 1000;

    SPX_TRACE_INFO("%s: m_cbFireVAD=%" PRIu64, __FUNCTION__, m_cbFireVAD);
    SPX_TRACE_INFO("%s: nAvgBytesPerSec=%u", __FUNCTION__, m_format->nAvgBytesPerSec);
}

void CSpxMockVadEngineAdapter::TermFormat()
{
    m_format = nullptr;
}

void CSpxMockVadEngineAdapter::End()
{
    SPX_DBG_ASSERT(GetSite());
    GetSite()->AdapterCompletedSetFormatStop(this);
}

void CSpxMockVadEngineAdapter::FireVoiceActivityDetected()
{
    SPX_DBG_TRACE_FUNCTION();
    std::string error;

    SPXAPI_TRY()
    {
        auto offset = ticksPerSecond * m_numMsVADOffset / 1000 ;
        auto duration = ticksPerSecond * m_numMsVADDuration/ 1000;

        auto site = GetSite();
        auto properties = SpxQueryInterface<ISpxNamedProperties>(site);
        auto keyword = (std::string)"";
        SPX_TRACE_INFO("%s: text='%s', offset=%" PRIu64 ", duration=%" PRIu64, __FUNCTION__, keyword.c_str(), offset, duration);

        auto size = (size_t)(m_numMsVADDuration * m_format->nAvgBytesPerSec / 1000);
        auto buffer = SpxAllocSharedAudioBuffer(size);
        auto write = buffer.get() + size;

        size_t remaining = size;
        for (auto chunk : m_audio)
        {
            if (remaining == 0) break;

            if (remaining >= chunk->size)
            {
                write = write - chunk->size;
                memcpy(write, chunk->data.get(), chunk->size);
                remaining -= chunk->size;
            }
            else
            {
                write = write - remaining;
                memcpy(write, chunk->data.get() + chunk->size - remaining, remaining);
                remaining -= remaining;
            }
        }

        auto chunk = std::make_shared<DataChunk>(buffer, (uint32_t)size);
        site->OnDetected(this, offset, duration, 1.0, keyword, chunk);
    }
    SPXAPI_CATCH_ONLY()
}

} } } } // Microsoft::CognitiveServices::Speech::Impl
