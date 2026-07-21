//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"

#include <ostream>
#include <sstream>
#include "usp_reco_engine_adapter_retry.h"
#include "create_object_helpers.h"
#include "interfaces/i_web_socket_state.h"
#include "error_info.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

void CSpxUspRecoEngineAdapterRetry::InitDelegatePtr(std::shared_ptr<ISpxRecoEngineAdapter>& ptr)
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);

    ptr = SpxCreateObjectWithSite<ISpxRecoEngineAdapter>("CSpxUspRecoEngineAdapter", this);
    if (ptr != nullptr)
    {
        return;
    }

    ZombieRecoEngineAdapterDelegate(true);
    SPX_DBG_TRACE_WARNING("Couldn't create engine adapter; zombified...");

    ExceptionWithCallStack ex(SPXERR_COULD_NOT_CREATE_ENGINE_ADAPTER);

    throw ex;
}

void CSpxUspRecoEngineAdapterRetry::Init()
{
    m_numMaxRetries = GetOr<uint16_t>("SPEECH-Error-MaxRetryCount", 4);
    m_retryDurationMS = GetOr<std::chrono::milliseconds>("SPEECH-Error-RetryDurationMS", std::chrono::milliseconds(250));
    m_retryConnectionFailures = GetOr<bool>("SPEECH-Error-RetryConnectionFailures", true);
}

void CSpxUspRecoEngineAdapterRetry::ShrinkReplayBuffer(uint64_t newBaseOffset)
{
    // Forward progress has been made on recognitions, re-set all counters.
    m_retriesDone = 0;
    DelegateShrinkReplayBuffer(newBaseOffset);
}

void CSpxUspRecoEngineAdapterRetry::SetFormat(const SPXWAVEFORMATEX* pformat)
{
    bool compressedPassThroughEnabled = GetOr<bool>("SPEECH-CompressedPassthrough", false);

    // Currently our service support compressed decoding only for OPUS and SILK
    // So right now we will only do it for OPUS
    // TODO: This decision is being made in 2 places, search for this guid for the other one: {40E0A8B6-57D5-4BD5-85A0-74B6F1652F24}
    m_compressedPassThrough = compressedPassThroughEnabled && pformat && pformat->wFormatTag == WAVE_FORMAT_OGG_OPUS;
    m_currentFormat = SpxCopyWAVEFORMATEX(pformat);
    DelegateSetFormat(pformat);
}

void CSpxUspRecoEngineAdapterRetry::AdapterDisconnected(std::shared_ptr<ISpxErrorInformation> payload)
{
    SPX_DBG_TRACE_VERBOSE("[%p]CSpxUspRecoEngineAdapterRetry::AdapterDisconnected", (void*)this);

    DelegateAdapterDisconnected(payload);

    // The only conditions to check reconnect in the disconnect handler is for when the remote service closed
    // the connection normally. Then we need to check and see if we need to reconnect.
    // All error conditions are handled by the ::Error(...) method.
    if (payload->GetCancellationCode() == CancellationErrorCode::ConnectionFailure &&
        payload->GetCategoryCode() == static_cast<int>(WebSocketError::REMOTE_CLOSED) &&
        payload->GetStatusCode() == static_cast<int>(WebSocketDisconnectReason::Normal) &&
        ShouldReconnect(payload))
    {
        StartReconnect(payload);
    }
    /// This is to catch timeouts where the service closes us. In that case, we should cleanup the adapter so
    /// the projection object can still be used.
    else if (payload->GetStatusCode() == static_cast<int>(WebSocketDisconnectReason::Normal) &&
        payload->GetCancellationCode() == CancellationErrorCode::ConnectionFailure)
    {
        CleanupAdapterAndAudio(payload, false);
    }
}

void CSpxUspRecoEngineAdapterRetry::StartReconnect(const std::shared_ptr<ISpxErrorInformation>& payload)
{
    m_retriesDone++;

    // The retry will be delayed by m_retryDurationMS milliseconds
    std::this_thread::sleep_for(m_retryDurationMS);

    CleanupAdapterAndAudio(payload, true);

    // Prep the new one.
    DelegateSetFormat(m_currentFormat.get());

    // Tell our site we hiccuped. This will cause the rewind to happen.
    bool inTurn = false;

    auto site = SpxQueryInterface<ISpxRecoEngineAdapterSite>(GetSite());
    SPX_THROW_HR_IF(SPXERR_UNEXPECTED_USP_SITE_FAILURE, site == nullptr);

    inTurn = site->IsExpectingAdapterStoppedTurn(this);
    if (inTurn)
    {
        AdapterStoppedTurn(this, true);
    }

    m_pendingReconnect = false;
}

void CSpxUspRecoEngineAdapterRetry::CleanupAdapterAndAudio(const std::shared_ptr<ISpxErrorInformation>& payload, bool pendingReconnect)
{
    UNUSED(payload);

    SPX_DBG_TRACE_VERBOSE("%s: Trying to reset the engine adapter", __FUNCTION__);
    auto finalResult = DiscardAudioUnderTransportErrors();
    if (finalResult != nullptr)
    {
        DelegateFireAdapterResult_FinalResult(finalResult->GetOffset(), finalResult);
    }

    if (pendingReconnect)
    {
        m_pendingReconnect = true;
    }

    // Clear the "old" adapter.
    ZombieTermAndClearRecoEngineAdapterDelegate();
    REA_Child::Zombie(false);
}

bool CSpxUspRecoEngineAdapterRetry::ShouldReconnect(const std::shared_ptr<ISpxErrorInformation>& payload)
{
    // Retry for Continuous and SingleShot recognition where the error is retryable (the default)
    bool shouldReconnect = (m_currentFormat
        && payload->GetRetryMode() == ISpxErrorInformation::RetryMode::Allowed
        && (m_retryConnectionFailures || payload->GetCancellationCode() != CancellationErrorCode::ConnectionFailure)
        && m_retriesDone < m_numMaxRetries)
        && !m_compressedPassThrough;

    SPX_DBG_TRACE_VERBOSE("%s: Should Reconnect: %i", __FUNCTION__, shouldReconnect);

    return shouldReconnect;
}

void CSpxUspRecoEngineAdapterRetry::Error(ISpxRecoEngineAdapter* adapter, std::shared_ptr<ISpxErrorInformation> payload)
{
    if (ShouldReconnect(payload))
    {
        StartReconnect(payload);
    }
    else
    {
        // Check for a saved token fetch error...
        if (HasStringValue("service.auth.token.lasterror"))
        {
            auto tokenError = GetStringValue("service.auth.token.lasterror");
            std::stringstream reportedError;
            reportedError << std::endl << "Last token fetch attempt resulted in error: " << std::endl << tokenError << std::endl;
            auto newError = ErrorInfo::FromErrorWithAppendedDetails(payload, reportedError.str().c_str());
            DelegateError(adapter, newError);
        }
        else
        {
            DelegateError(adapter, payload);
        }
    }
}

std::shared_ptr<ISpxRecognitionResult> CSpxUspRecoEngineAdapterRetry::DiscardAudioUnderTransportErrors()
{
    // The property string "DiscardAudioFromIntermediateRecoResult" was designed to be used by the Teams app
    // (with the ConversationTranscriber APIs). But it has never been shared with them.
    // The requirement for Teams live transcription was to discard all buffered audio in the client in case of a network error
    // in order to quickly catch up to real-time (do NOT re-send all unacknowledged audio after reconnecting, which is the
    // default behavior). Furthermore, repurpose the last hypothesis before the network error and send it to the app as a
    // final reco result.
    // See further notes in the relevant C++ test "ConversationTranscriber::online_pull_stream_internal_error".
    bool discardAudio = GetOr<bool>("DiscardAudioFromIntermediateRecoResult", false);

    if (discardAudio == false)
    {
        return nullptr;
    }
    if (m_mostRecentIntermediateRecoResult == nullptr)
    {
        return nullptr;
    }
    SPX_DBG_TRACE_VERBOSE("Discarding audio after transportErrors");
    uint64_t offset = m_mostRecentIntermediateRecoResult->GetOffset();
    uint64_t duration = m_mostRecentIntermediateRecoResult->GetDuration();
    uint64_t new_offset = offset + duration;

    SPX_DBG_TRACE_VERBOSE("%s: this=0x%8p Service acknowledging to offset %" PRIu64 " (100ns).", __FUNCTION__, (void*)this, new_offset);
    SetStringValue(g_audioContinuationOffset, std::to_string(new_offset).c_str());

    SPX_DBG_TRACE_VERBOSE("%s: ShrinkReplayBuffer=%" PRIu64 "", __FUNCTION__, new_offset);
    DelegateShrinkReplayBuffer(new_offset);

    return CreateFakeFinalResult(m_mostRecentIntermediateRecoResult);
}

std::shared_ptr<ISpxRecognitionResult> CSpxUspRecoEngineAdapterRetry::CreateFakeFinalResult(const std::shared_ptr<ISpxRecognitionResult>& intermediate)
{
    if (intermediate == nullptr)
    {
        return nullptr;
    }

    std::string text = intermediate->GetText();
    uint64_t offset = intermediate->GetOffset();
    uint64_t duration = intermediate->GetDuration();
    std::string userId{};
    auto ctsResult = SpxQueryInterface<ISpxConversationTranscriptionResult>(intermediate);
    if (ctsResult != nullptr)
    {
        userId = ctsResult->GetUserId();
    }

    auto factory = SpxQueryService<ISpxRecoResultFactory>(GetSite());
    if (factory == nullptr)
    {
        return nullptr;
    }

    auto result = factory->CreateFinalResult(ResultReason::RecognizedSpeech, NO_MATCH_REASON_NONE, text.c_str(), offset, duration, userId.c_str());
    return result;
}

void CSpxUspRecoEngineAdapterRetry::FireAdapterResult_Intermediate(uint64_t offset, std::shared_ptr<ISpxRecognitionResult> result)
{
    m_mostRecentIntermediateRecoResult = result;
    DelegateFireAdapterResult_Intermediate(offset, result);
}

void CSpxUspRecoEngineAdapterRetry::FireAdapterResult_FinalResult(uint64_t offset, std::shared_ptr<ISpxRecognitionResult> result)
{
    m_mostRecentIntermediateRecoResult = nullptr;
    DelegateFireAdapterResult_FinalResult(offset, result);
}

void CSpxUspRecoEngineAdapterRetry::AdapterCompletedSetFormatStop(ISpxRecoEngineAdapter* adapter)
{
    if (!m_pendingReconnect)
    {
        DelegateAdapterCompletedSetFormatStop(adapter);
    }
}

void CSpxUspRecoEngineAdapterRetry::SetStringValue(const char* name, const char* value)
{
    // Set string values in the session as there is a dependency on them being available to the Recognizer object, and that's where it is.
    auto session = SpxQueryService<ISpxSession>(GetSite());
    SPX_THROW_HR_IF(SPXERR_UNEXPECTED_USP_SITE_FAILURE, session == nullptr);

    SpxQueryInterface<ISpxNamedProperties>(session)->SetStringValue(name, value);
}

void CSpxUspRecoEngineAdapterRetry::SetBinaryValue(const char* name, std::shared_ptr<uint8_t> value, size_t size)
{
    // Set string values in the session as there is a dependency on them being available to the Recognizer object, and that's where it is.
    auto session = SpxQueryService<ISpxSession>(GetSite());
    SPX_THROW_HR_IF(SPXERR_UNEXPECTED_USP_SITE_FAILURE, session == nullptr);

    SpxQueryInterface<ISpxNamedProperties>(session)->SetBinaryValue(name, value, size);
}

}}}}
