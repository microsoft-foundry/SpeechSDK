//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"

#include "hybrid_reco_engine_adapter.h"
#include "create_object_helpers.h"
#include "interfaces/i_web_socket_state.h"
#include "error_info.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

CSpxHybridRecoEngineAdapter::CSpxHybridRecoEngineAdapter()
{
    SPX_DBG_TRACE_FUNCTION();
}

CSpxHybridRecoEngineAdapter::~CSpxHybridRecoEngineAdapter()
{
    SPX_DBG_TRACE_FUNCTION();

    if (m_probeUspRecoEngineAdapter != nullptr)
    {
        m_probeUspRecoEngineAdapter->CloseConnection();
        SpxTermAndClear(m_probeUspRecoEngineAdapter);
    }
}

void CSpxHybridRecoEngineAdapter::InitDelegatePtr(std::shared_ptr<ISpxRecoEngineAdapter>& ptr)
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);

    if (m_retriesDone == 0)
    {
        SPX_DBG_TRACE_VERBOSE("%s: Create CSpxUspRecoEngineAdapterRetry", __FUNCTION__);
        ptr = SpxCreateObjectWithSite<ISpxRecoEngineAdapter>("CSpxUspRecoEngineAdapterRetry_OffsetFixupWrapper", this);
        m_checkingUspConnection = false;
    }
    else
    {
        // Fallback to Rnnt adapter when network error
        SPX_DBG_TRACE_VERBOSE("%s: Create CSpxRnntRecoEngineAdapter and start connectivity probe", __FUNCTION__);
        ptr = SpxCreateObjectWithSite<ISpxRecoEngineAdapter>("CSpxRnntRecoEngineAdapter", this);

        // Start probe to check connectivity of cloud SR
        if (!m_checkingUspConnection && m_uspRetryAllowed)
        {
            m_checkingUspConnection = true;
            m_probeUspRecoEngineAdapter = SpxCreateObjectWithSite<ISpxRecoEngineAdapter>("CSpxUspRecoEngineAdapter", this);
            m_probeUspRecoEngineAdapter->OpenConnection(m_singleShot);
        }
    }

    if (ptr != nullptr)
    {
        return;
    }

    ZombieRecoEngineAdapterDelegate(true);
    SPX_DBG_TRACE_WARNING("Couldn't create engine adapter; zombified...");

    ExceptionWithCallStack ex(SPXERR_COULD_NOT_CREATE_ENGINE_ADAPTER);

    throw ex;
}

void CSpxHybridRecoEngineAdapter::Init()
{
    SPX_DBG_TRACE_FUNCTION();

    m_numMaxRetries = GetOr<uint16_t>("SPEECH-Error-MaxRetryCount", 4);
    m_retryDurationMS = GetOr<std::chrono::milliseconds>("SPEECH-Error-RetryDurationMS", std::chrono::milliseconds(250));
    m_checkingUspConnectionInterval = GetOr<std::chrono::milliseconds>("SPEECH-Error-CheckingUspConnectionInterval", std::chrono::milliseconds(30000));
}

void CSpxHybridRecoEngineAdapter::ShrinkReplayBuffer(uint64_t newBaseOffset)
{
    DelegateShrinkReplayBuffer(newBaseOffset);
}

void CSpxHybridRecoEngineAdapter::GetCurrentAudioBufferOffset(uint64_t* offsetInTicks, uint64_t* offsetInBytes)
{
    DelegateGetCurrentAudioBufferOffset(offsetInTicks, offsetInBytes);
}

void CSpxHybridRecoEngineAdapter::GetCurrentAudioContinuationOffset(uint64_t* offsetInTicks)
{
    DelegateGetCurrentAudioContinuationOffset(offsetInTicks);
}

void CSpxHybridRecoEngineAdapter::GetMultiChannelProcessingMode(bool* useMultiChannelProcessing)
{
    DelegateGetMultiChannelProcessingMode(useMultiChannelProcessing);
}

void CSpxHybridRecoEngineAdapter::SetAdapterMode(bool singleShot)
{
    SPX_DBG_TRACE_VERBOSE("%s: singleShot=%d", __FUNCTION__, singleShot);
    m_singleShot = singleShot;
    DelegateSetAdapterMode(singleShot);
}

void CSpxHybridRecoEngineAdapter::SetFormat(const SPXWAVEFORMATEX* pformat)
{
    SPX_DBG_TRACE_VERBOSE("%s: pformat %s nullptr", __FUNCTION__, pformat != nullptr ? "!=" : "==");

    bool compressedPassThroughEnabled = GetOr<bool>("SPEECH-CompressedPassthrough", false);

    // Currently our service support compressed decoding only for OPUS and SILK
    // So right now we will only do it for OPUS
    // TODO: This decision is being made in 2 places, search for this guid for the other one: {40E0A8B6-57D5-4BD5-85A0-74B6F1652F24}
    m_compressedPassThrough = compressedPassThroughEnabled && pformat && pformat->wFormatTag == WAVE_FORMAT_OGG_OPUS;
    m_currentFormat = SpxCopyWAVEFORMATEX(pformat);
    DelegateSetFormat(pformat);
}

void CSpxHybridRecoEngineAdapter::AdapterConnected(const std::string& url)
{
    SPX_DBG_TRACE_FUNCTION();

    // When checking usp connection recovery, the probe adapter connected callback indicates network recovers
    // Clean up the probe and start reconnect with Usp adapter
    if (m_checkingUspConnection)
    {
        SPX_DBG_TRACE_VERBOSE("%s: Connectivity probe detects network recovers. Reconnect to cloud. ", __FUNCTION__);
        m_probeUspRecoEngineAdapter->CloseConnection();
        SpxTermAndClear(m_probeUspRecoEngineAdapter);
        // reset retry count to switch back to cloud. Use -1 because there is ++ operation at the beginning of StartReconnect
        m_retriesDone = -1;
        auto error = ErrorInfo::FromExplicitError(CancellationErrorCode::NoError, "Reset to cloud SR");
        StartReconnect(error);
    }
    else
    {
        SPX_DBG_TRACE_VERBOSE("[%p]CSpxHybridRecoEngineAdapter::AdapterConnected to %s", (void*)this, url.empty() ? "<Not Supplied>":url.c_str());
        DelegateAdapterConnected(url);
    }
}

void CSpxHybridRecoEngineAdapter::AdapterDisconnected(std::shared_ptr<ISpxErrorInformation> payload)
{
    SPX_DBG_TRACE_FUNCTION();

    // When checking usp connection recovery, ignore the probe adapter disconnected callback
    // Probe network error is handled in "Error" method
    if (m_checkingUspConnection)
    {
        return;
    }

    SPX_DBG_TRACE_VERBOSE("[%p]CSpxHybridRecoEngineAdapter::AdapterDisconnected", (void*)this);

    DelegateAdapterDisconnected(payload);
}

void CSpxHybridRecoEngineAdapter::StartReconnect(const std::shared_ptr<ISpxErrorInformation>& payload)
{
    UNUSED(payload);

    m_retriesDone++;

    // The retry will be delayed by m_retryDurationMS milliseconds
    std::this_thread::sleep_for(m_retryDurationMS);

    SPX_DBG_TRACE_VERBOSE("%s: Trying to reset the engine adapter", __FUNCTION__);

    auto finalResult = DiscardAudioUnderTransportErrors();
    if (finalResult != nullptr)
    {
        DelegateFireAdapterResult_FinalResult(finalResult->GetOffset(), finalResult);
    }

    m_pendingReconnect = true;

    // Clear the "old" adapter.
    ZombieTermAndClearRecoEngineAdapterDelegate();
    REA_Child::Zombie(false);

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

bool CSpxHybridRecoEngineAdapter::ShouldReconnect(const std::shared_ptr<ISpxErrorInformation>& payload)
{
    // Retry for Continuous and SingleShot recognition where the error is retryable (the default).
    // Not check m_currentFormat here, as when network disconnected at first place, m_currentFormat is empty.
    // We should allow reconnect & fallback to embedded SR.
    // Then AudioStreamSession will SetFormat again when AudioSourceStarted.
    bool shouldReconnect = m_retriesDone < m_numMaxRetries && !m_compressedPassThrough;
    UNUSED(payload);

    SPX_DBG_TRACE_VERBOSE("%s: Should Reconnect: %i", __FUNCTION__, shouldReconnect);

    return shouldReconnect;
}

void CSpxHybridRecoEngineAdapter::Error(ISpxRecoEngineAdapter* adapter, std::shared_ptr<ISpxErrorInformation> payload)
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);

    const std::string message = payload->GetDetails();
    bool isUspError = message.find("embedded") == std::string::npos;

    if (isUspError)
    {
        // In case of a non-retryable USP error, stick to embedded SR and do not try USP anymore.
        bool isUspFatalError = message.find("Internal server error") != std::string::npos;
        m_uspRetryAllowed = (payload->GetRetryMode() == ISpxErrorInformation::RetryMode::Allowed) && !isUspFatalError;
        SPX_DBG_TRACE_VERBOSE("%s: USP retry allowed: %i", __FUNCTION__, m_uspRetryAllowed);
    }

    // When checking usp connection recovery, the network error is triggered by probe
    // Close and recreate probe here, using a delay-start thread
    if (isUspError && m_checkingUspConnection)
    {
        SPX_DBG_TRACE_VERBOSE("%s: Connectivity probe detects network is still down.", __FUNCTION__);
        m_probeUspRecoEngineAdapter->CloseConnection();
        SpxTermAndClear(m_probeUspRecoEngineAdapter);

        if (m_uspRetryAllowed)
        {
            SPX_DBG_TRACE_VERBOSE("%s: Start next round probe.", __FUNCTION__);
            auto threadService = SpxQueryService<ISpxThreadService>(GetSite());
            threadService->ExecuteAsync(
                std::packaged_task<void()>([this]
                    {
                        m_probeUspRecoEngineAdapter = SpxCreateObjectWithSite<ISpxRecoEngineAdapter>("CSpxUspRecoEngineAdapter", this);
                        m_probeUspRecoEngineAdapter->OpenConnection(m_singleShot);
                    }), m_checkingUspConnectionInterval);
        }
        else
        {
            m_checkingUspConnection = false;
        }
    }
    else
    {
        if (isUspError && ShouldReconnect(payload))
        {
            StartReconnect(payload);
        }
        else
        {
            DelegateError(adapter, payload);
        }
    }
}

std::shared_ptr<ISpxRecognitionResult> CSpxHybridRecoEngineAdapter::DiscardAudioUnderTransportErrors()
{
    // Force discard audio to return current reco adapter result asap.
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
    ShrinkReplayBuffer(new_offset);

    auto result = CreateFakeFinalResult(m_mostRecentIntermediateRecoResult);
    m_mostRecentIntermediateRecoResult = nullptr; // prevent duplicate fakes
    return result;
}

std::shared_ptr<ISpxRecognitionResult> CSpxHybridRecoEngineAdapter::CreateFakeFinalResult(const std::shared_ptr<ISpxRecognitionResult>& intermediate)
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

    // Copy the online/offline backend info.
    auto propertiesIntermediate = SpxQueryInterface<ISpxNamedProperties>(intermediate);
    auto recognitionBackend = propertiesIntermediate->GetOr(PropertyId::SpeechServiceResponse_RecognitionBackend, "");
    auto propertiesFinal = SpxQueryInterface<ISpxNamedProperties>(result);
    propertiesFinal->Set(PropertyId::SpeechServiceResponse_RecognitionBackend, recognitionBackend.c_str());

    return result;
}

void CSpxHybridRecoEngineAdapter::FireAdapterResult_Intermediate(uint64_t offset, std::shared_ptr<ISpxRecognitionResult> result)
{
    m_mostRecentIntermediateRecoResult = result;
    DelegateFireAdapterResult_Intermediate(offset, result);
}

void CSpxHybridRecoEngineAdapter::FireAdapterResult_FinalResult(uint64_t offset, std::shared_ptr<ISpxRecognitionResult> result)
{
    m_mostRecentIntermediateRecoResult = nullptr;
    DelegateFireAdapterResult_FinalResult(offset, result);
}

void CSpxHybridRecoEngineAdapter::AdapterCompletedSetFormatStop(ISpxRecoEngineAdapter* adapter)
{
    SPX_DBG_TRACE_FUNCTION();

    if (!m_pendingReconnect)
    {
        DelegateAdapterCompletedSetFormatStop(adapter);
    }
}

void CSpxHybridRecoEngineAdapter::SetStringValue(const char* name, const char* value)
{
    // Set string values in the session as there is a dependency on them being available to the Recognizer object, and that's where it is.
    auto session = SpxQueryService<ISpxSession>(GetSite());
    SPX_THROW_HR_IF(SPXERR_UNEXPECTED_USP_SITE_FAILURE, session == nullptr);

    SpxQueryInterface<ISpxNamedProperties>(session)->SetStringValue(name, value);
}

}}}}
