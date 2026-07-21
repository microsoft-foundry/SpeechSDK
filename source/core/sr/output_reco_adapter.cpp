//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"

#include <algorithm>

#include "output_reco_adapter.h"
#include "service_helpers.h"
#include "create_object_helpers.h"
#include "site_helpers.h"
#include "buffer_helpers.h"

using namespace Microsoft::CognitiveServices::Speech::Impl;

void CSpxOutputRecoEngineAdapter::SetAdapterMode(bool singleShot)
{
    /* This should only be used for single shot */
    SPX_THROW_HR_IF(SPXERR_SWITCH_MODE_NOT_ALLOWED, !singleShot);
}

void CSpxOutputRecoEngineAdapter::SetFormat(const SPXWAVEFORMATEX* format)
{
    SPX_DBG_TRACE_VERBOSE("%s: %d", __FUNCTION__, format != nullptr);

    SPX_THROW_HR_IF(SPXERR_ALREADY_INITIALIZED, (format != nullptr) && (m_stream != nullptr));
    if (format != nullptr)
    {
        m_stream = SpxCreateObjectWithSite<ISpxAudioDataStream>("CSpxAudioDataStream", SpxGetRootSite());
        auto streamInit = SpxQueryInterface<ISpxAudioDataStreamInit>(m_stream);
        streamInit->InitFromFormat(format, true);
        m_stream->SetStatus(StreamStatus::NoData);
        m_sink = SpxQueryInterface<ISpxAudioOutput>(m_stream);
        m_bytesPerSecond = (format->wBitsPerSample * format->nChannels * format->nSamplesPerSec) / 8;
        InvokeOnSite([this](const SitePtr& site)
        {
            site->AdapterStartingTurn(this);
            site->AdapterStartedTurn(this, "");
        });
        if (m_expectedInTicks == 0)
        {
            UpdateStatus(StreamStatus::PartialData);
        }
    }
    else
    {
        UpdateStatus(StreamStatus::AllData);
    }

}

void CSpxOutputRecoEngineAdapter::SetMinInputSize(const uint64_t sizeInTicks)
{
    SPX_DBG_TRACE_VERBOSE("%s: %" PRIu64 " ticks", __FUNCTION__, sizeInTicks);

    std::lock_guard<std::mutex> lk{ m_stateMutex };
    if (!m_stream || (m_stream->GetStatus() == StreamStatus::NoData))
    {
        m_expectedInTicks = sizeInTicks;
        m_minInputSizeTicks = sizeInTicks; // to preserve the original value
    }
}

void CSpxOutputRecoEngineAdapter::ProcessAudio(const DataChunkPtr& audioChunk)
{
    SPX_DBG_TRACE_VERBOSE("%s: size %d", __FUNCTION__, audioChunk->size);

    std::lock_guard<std::mutex> lk{ m_stateMutex };
    if (GetStatus() == StreamStatus::AllData)
    {
        /* We can receive the 0 byte chunk after detaching */
        SPX_THROW_HR_IF(SPXERR_INVALID_STATE, audioChunk->size != 0);
        return;
    }
    m_size += audioChunk->size;
    m_sink->Write(audioChunk->data.get(), audioChunk->size);
    if (m_expectedInTicks != 0)
    {
        uint64_t sizeInTicks = BytesToDuration<tick>(audioChunk->size, m_bytesPerSecond).count();
        m_expectedInTicks -= std::min(m_expectedInTicks, sizeInTicks);
    }
    else
    {
        SetStatus(StreamStatus::PartialData);
    }
    m_cv.notify_all();
}

void CSpxOutputRecoEngineAdapter::DetachInput()
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);

    if (m_detaching.exchange(true))
    {
        return;
    }
    /* We wait until there is some data (state is either PartialData or AllData) to detach */
    WaitForStatus(StreamStatus::PartialData);
    InvokeOnSite([this](const SitePtr& site)
    {
        auto duration = BytesToDuration<tick>(m_size, m_bytesPerSecond);

        // Determine the length of audio to discard from the input audio buffer.
        // If the minimum size (of detected keyword audio) was set, discard at
        // least as much, to ensure that if keyword detection is run using the
        // same input stream again, it will not receive the same keyword audio
        // that it has already processed.
        uint64_t offset = std::max(m_minInputSizeTicks, duration.count());

        // If our site has been holding bytes to replay them, tell it that it can discard.
        auto replayer = SpxQueryInterface<ISpxAudioReplayer>(site);
        if (nullptr != replayer)
        {
            SPX_DBG_TRACE_VERBOSE(
                "CSpxOutputRecoEngineAdapter::DetachInput: ShrinkReplayBuffer offset %" PRIu64 " ms",
                offset / 10000);
            replayer->ShrinkReplayBuffer(offset);

            // Update the global audio continuation offset.
            auto properties = SpxQueryService<ISpxNamedProperties>(site);
            uint64_t audioContinuationOffset = properties->GetOr<uint64_t>(g_audioContinuationOffset, 0);

            SPX_DBG_TRACE_VERBOSE(
                "CSpxOutputRecoEngineAdapter::DetachInput: g_audioContinuationOffset = %" PRIu64 " ms",
                (audioContinuationOffset + offset) / 10000);
            SetStringValue(g_audioContinuationOffset, std::to_string(audioContinuationOffset + offset).c_str());
        }

        auto factory = SpxQueryService<ISpxRecoResultFactory>(site);
        auto result = factory->CreateFinalResult(ResultReason::RecognizedSpeech, NO_MATCH_REASON_NONE, "", 0, 0, "");
        site->FireAdapterResult_FinalResult(offset, result);
        site->AdapterStoppedTurn(this);
    });
    WaitForStatus(StreamStatus::AllData);
    InvokeOnSite([this](const SitePtr& site)
    {
        site->AdapterCompletedSetFormatStop(this);
    });
}

void CSpxOutputRecoEngineAdapter::SetStringValue(const char* name, const char* value)
{
    auto session = SpxQueryService<ISpxSession>(GetSite());
    SPX_THROW_HR_IF(SPXERR_UNEXPECTED_USP_SITE_FAILURE, session == nullptr);

    SpxQueryInterface<ISpxNamedProperties>(session)->SetStringValue(name, value);
}
