//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// audio_stream_session.cpp: Implementation definitions for CSpxAudioStreamSession C++ class
//
#include "stdafx.h"

#define NOMINMAX
#include <future>
#include <list>
#include <initializer_list>
#include <memory>
#include <inttypes.h>
#include <limits.h>
#include <ajv.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <TraceLoggingProvider.h>

#define MICROSOFT_KEYWORD_MEASURES 0x0000400000000000 // Bit 46
#define MICROSOFT_KEYWORD_TELEMETRY 0x0000200000000000 // Bit 45
#define TraceLoggingOptionMicrosoftTelemetry() TraceLoggingOptionGroup(0x4f50731a, 0x89cf, 0x4782, 0xb3, 0xe0, 0xdc, 0xe8, 0xc9, 0x4, 0x76, 0xba)

// forward-declare
TRACELOGGING_DECLARE_PROVIDER(tracingEventProvider);
#endif

#ifdef _WIN32
TRACELOGGING_DEFINE_PROVIDER(tracingEventProvider,
    "Microsoft.CognitiveServices.Speech.SDK",
    (0xbbdb52c5, 0xeb5c, 0x4616, 0x83, 0x1d, 0x90, 0x12, 0x13, 0xd3, 0x79, 0x1c),
    TraceLoggingOptionMicrosoftTelemetry());
#endif

#include "audio_stream_session.h"
#include "speech_audio_processor.h"
#include "spxcore_common.h"
#include "asyncop.h"
#include "create_object_helpers.h"
#include "guid_utils.h"
#include "string_utils.h"
#include "ispxinterfaces.h"
#include "interface_helpers.h"
#include "site_helpers.h"
#include "service_helpers.h"
#include "property_id_2_name_map.h"
#include "try_catch_helpers.h"
#include "time_utils.h"
#include "error_info.h"
#include "buffer_helpers.h"
#include "thread_service_delegate_helper.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

using namespace std;
using namespace std::chrono;
using namespace std::chrono_literals;

constexpr std::chrono::seconds c_defaultStopRecognitionTimeout{ 10 };
constexpr std::chrono::seconds c_stopRecognitionTimeoutOnError{ 1 };
// The offline default timeout is used with embedded SR when there are no audio
// leftovers to process, otherwise the value depends on the amount of leftovers.
constexpr std::chrono::seconds c_offlineStopRecognitionTimeout{ 1 };

atomic<int64_t> CSpxAudioStreamSession::Operation::OperationId;
const minutes CSpxAudioStreamSession::Operation::Timeout = 1min;

const uint32_t MAX_REPLAY_BUFFER_SIZE_FOR_DETECTION_BYTE = 64000;

CSpxAudioStreamSession::CSpxAudioStreamSession() :
    m_sessionId(PAL::CreateGuidWithoutDashes()),
    m_recoKind(RecognitionKind::Idle),
    m_sessionState(SessionState::Idle),
    m_sawEndOfStream(false),
    m_fireEndOfStreamAtSessionStop(false),
    m_adapterResetPending(false),
    m_adapterStreamingAudio(false),
    m_expectFirstHypothesis(true),
    m_adapterAudioMuted(false),
    m_audioPumpStoppedBeforeHotSwap(false),
    m_turnEndStopKind(RecognitionKind::Idle),
    m_detectionProcessorMode{ DetectionProcessorMode::None },
    m_isReliableDelivery{ false },
    m_lastErrorGlobalOffset{ 0 },
    m_currentTurnGlobalOffset{ 0 },
    m_bytesTransited(0),
    m_sessionActive(false),
    m_isDisposing(false),
    m_sessionStarted(false),
    m_sessionStopped(false),
    m_canceledOnError(false),
    m_isMultiKeywordRecognition(false)
{
    SPX_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::CSpxAudioStreamSession", (void*)this);
}

CSpxAudioStreamSession::~CSpxAudioStreamSession()
{
    SPX_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::~CSpxAudioStreamSession", (void*)this);
    SPX_DBG_ASSERT(m_detectionAdapter == nullptr);
    SPX_DBG_ASSERT(m_recoAdapter == nullptr);
    SPX_DBG_ASSERT(m_multiKeywordRecoAdapter == nullptr);
}

void CSpxAudioStreamSession::Init()
{
    SPX_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::Init:... ", (void*)this);

    // NOTE: Due to current ownership model, and our late-into-the-cycle changes for SpeechConfig objects
    // the CSpxAudioStreamSession is sited to the CSpxApiFactory. This ApiFactory is not held by the
    // dev user at or above the CAPI. Thus ... we must hold it alive in order for the properties to be
    // obtainable via the standard ISpxNamedProperties mechanisms... It will be released on ::Term()
    m_siteKeepAlive = GetSite();

    // Currently we do not perform lazy initialization in order not to introduce latency in the
    // future calls (i.e. RecognizeAsync).
    m_threadService = SpxCreateObjectWithSite<ISpxThreadService>("CSpxThreadService", this);

    // Setup file logging, if requested
    auto dumpAudioToDir = GetStringValue("CARBON-INTERNAL-DumpAudioToDir");
    m_saveToWavEverything.SetFolder(dumpAudioToDir);
    m_saveToWavDetectedKeyword.SetFolder(dumpAudioToDir);
}

void CSpxAudioStreamSession::Term()
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    SPX_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::Term:... ", (void*)this);

    if (m_compressedAudioAdapter != nullptr)
    {
        m_compressedAudioAdapter->StopCompressedPump();
    }

    // No other threads after this point can access internal structures,
    // so it is safe to clean them up.
    // Let's check to see if we're still processing audio ...
    // We can be here in some extreme cases where the StartPump did not complete yet so we are waiting for that case.

    // The pump start was called in both of these states so we force the issue and stop it regardless if the transition to processing was complete
    if (TryChangeState({ SessionState::WaitForPumpSetFormatStart, SessionState::ProcessingAudio }, SessionState::StoppingPump))
    {
        // We're terminating, and we were still processing audio ... So ... Let's shut down the pump
        SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::Term: Now StoppingPump ...", (void*)this);

        if (InvokeMemberIfNotNull(m_audioShim, &ISpxAudioSessionShim::StopAudio))
        {
            SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::Term: StopPump[%p]", (void*)this, (void*)m_audioShim.get());
        }

        InvokeMemberIfNotNull(m_codecAdapter, &ISpxAudioStreamReader::Close);
    }
    else
    {
        SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::Term: **NOT CALLED** StopPump[%p] - state: %d", (void*)this, (void*)m_audioShim.get(), (int)m_sessionState);
    }

    // Stopping all threads. Must complete before any member the threadservice
    // workers may call into (audio shim, codec adapter, recognizer adapter,
    // etc.) is destroyed below.
    SpxTerm(m_threadService);

    // Make sure there is nobody waiting on a single shot.
    CancelPendingSingleShot(RecognitionKind::SingleShot);
    {
        unique_lock<mutex> lock(m_recognizersLock);
        m_recognizers.clear();
    }

    SpxTermAndClear(m_audioShim);
    SpxTermAndClear(m_detectionAdapter);
    SpxTermAndClear(m_recoAdapter);
    SpxTermAndClear(m_siteKeepAlive);
    SpxTermAndClear(m_codecAdapter);
    SpxTermAndClear(m_multiKeywordRecoAdapter);
    m_audioProcessor = nullptr;
    m_speechProcessor = nullptr;
    m_audioBuffer = nullptr;
    m_throttleLogic = nullptr;
}

void CSpxAudioStreamSession::CancelPendingSingleShot(RecognitionKind)
{
    SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::CancelPendingSingleShot", (void*)this);

    auto& currentSingleShot = m_singleShotInFlight;

    // Make sure there is nobody waiting on a single shot.
    if (currentSingleShot &&
        currentSingleShot->m_future.wait_for(0s) == future_status::timeout)
    {
        auto error = ErrorInfo::FromRuntimeMessage("Shutdown while waiting on result.");
        auto result = CreateErrorResult(error);

        currentSingleShot->m_promise.set_value(result);
        currentSingleShot->m_spottedKeywordResult = nullptr;
        currentSingleShot = nullptr;
    }

    if (m_singleTextInFlight &&
        m_singleTextInFlight->m_future.wait_for(0s) == future_status::timeout)
    {
        auto error = ErrorInfo::FromRuntimeMessage("Shutdown while waiting on result.");
        auto result = CreateErrorResult(error);

        m_singleTextInFlight->m_promise.set_value(result);
        m_singleTextInFlight->m_spottedKeywordResult = nullptr;
        m_singleTextInFlight = nullptr;
    }
}

packaged_task<void()> CSpxAudioStreamSession::CreateTask(function<void()> func, bool catchAll)
{
    // Creates a packaged task that propagates all exceptions
    // to the user thread and then user callback.
    auto keepAlive = SpxSharedPtrFromThis<ISpxSession>(this);
    if (catchAll)
    {
        // Catches all exceptions and sends them to the user thread.
        return packaged_task<void()>([this, keepAlive, func]() {
            string error;
            SPXAPI_TRY()
            {
                func();
                return;
            }
            SPXAPI_CATCH_ONLY()
            CheckError(error);
        });
    }
    else
    {
        return packaged_task<void()>([this, keepAlive, func]() {
            // Make Android compiler happy.
            UNUSED(this);
            UNUSED(keepAlive);
            func();
        });
    }
}

void CSpxAudioStreamSession::InitFromFile(const char * filename)
{
    SPX_THROW_HR_IF(SPXERR_ALREADY_INITIALIZED, m_audioShim != nullptr);
    SPX_DBG_ASSERT(SessionIsIdle());
    SPX_DBG_TRACE_VERBOSE("%s: Now Idle ...", __FUNCTION__);

    m_audioShim = SpxCreateObjectWithSite<ISpxAudioSessionShim>("CSpxAudioSessionShim", this);

    QueryAndInvokeMember<ISpxAudioSourceInit>(m_audioShim, &ISpxAudioSourceInit::InitFromFile, filename);

    Set(PropertyId::AudioConfig_AudioSource, g_audioSourceFile);
    SetAudioConfigurationInProperties();

    m_isReliableDelivery = true;
}

void CSpxAudioStreamSession::InitFromMicrophone()
{
    SPX_THROW_HR_IF(SPXERR_ALREADY_INITIALIZED, m_audioShim != nullptr);
    SPX_DBG_ASSERT(SessionIsIdle());
    SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::InitFromMicrophone: Now Idle", (void*)this);

    // Create the microphone pump
    auto site = SpxSiteFromThis(this);
    m_audioShim = SpxCreateObjectWithSite<ISpxAudioSessionShim>("CSpxAudioSessionShim", this);
    SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::InitFromMicrophone: Pump from microphone:[%p]", (void*)this, (void*)m_audioShim.get());

    QueryAndInvokeMember<ISpxAudioSourceInit>(m_audioShim, &ISpxAudioSourceInit::InitFromMicrophone);

    Set(PropertyId::AudioConfig_AudioSource, g_audioSourceMicrophone);
    SetAudioConfigurationInProperties();
    // Write microphone device name and session id
    WriteTracingEvent();
}

void CSpxAudioStreamSession::InitFromStream(std::shared_ptr<ISpxAudioStream> stream)
{
    SPX_THROW_HR_IF(SPXERR_ALREADY_INITIALIZED, m_audioShim != nullptr);
    SPX_DBG_ASSERT(SessionIsIdle());
    SPX_DBG_TRACE_VERBOSE("%s: Now Idle ...", __FUNCTION__);

    // Create the stream pump
    auto site = SpxSiteFromThis(this);
    m_audioShim = SpxCreateObjectWithSite<ISpxAudioSessionShim>("CSpxAudioSessionShim", this);
    /* TODO: We could change the Stream interface to return SpxWaveFormatEx */
    auto waveFormat = [](auto& stream)
    {
        auto size = stream->GetFormat(nullptr, 0);
        auto waveFormat = SpxAllocWAVEFORMATEX(size);
        stream->GetFormat(waveFormat.get(), size);
        return waveFormat;
    }(stream);

    // In case of PCM, ALAW, MULAW, G.722 send the data directly to service by default.
    if (!(waveFormat->wFormatTag == WAVE_FORMAT_PCM ||
          waveFormat->wFormatTag == WAVE_FORMAT_ALAW ||
          waveFormat->wFormatTag == WAVE_FORMAT_MULAW ||
          waveFormat->wFormatTag == WAVE_FORMAT_G722))
    {
        auto reader = SpxQueryInterface<ISpxAudioStreamReader>(stream);
        bool ifPassThrough = GetOr<bool>("SPEECH-CompressedPassthrough", false);
        // Currently our service support compressed decoding only for OPUS and SILK
        // So right now we will only do it for OPUS
        // TODO: This decision is being made in 2 places, search for this guid for the other one: {40E0A8B6-57D5-4BD5-85A0-74B6F1652F24}
        if (waveFormat->wFormatTag == WAVE_FORMAT_OGG_OPUS && ifPassThrough)
        {
            m_compressedAudioAdapter = make_shared<CSpxCompressedAudioAdapter>(reader);
        }
        else
        {
            m_codecAdapter = SpxCreateObjectWithSite<ISpxAudioStreamReader>("CSpxCodecAdapter", GetSite());
            SPX_THROW_HR_IF(SPXERR_GSTREAMER_NOT_FOUND_ERROR, m_codecAdapter == nullptr);

            auto initCallbacks = SpxQueryInterface<ISpxAudioStreamReaderInitCallbacks>(m_codecAdapter);
            initCallbacks->SetCallbacks(
                [=](uint8_t* buffer, uint32_t size) { return reader->Read(buffer, size); },
                [=]() { { reader->Close(); } });

            initCallbacks->SetPropertyCallback2(
                [=](PropertyId propertyId) {
                    return reader->GetProperty(propertyId);
                });

            auto adapterAsSetFormat = SpxQueryInterface<ISpxAudioStreamInitFormat>(m_codecAdapter);

            waveFormat->nChannels = GetOr<uint16_t>("OutputPCMChannelCount", 1);
            waveFormat->wBitsPerSample = GetOr<uint16_t>("OutputPCMNumBitsPerSample", 16);
            waveFormat->nSamplesPerSec = GetOr<uint16_t>("OutputPCMSamplerate", 16000);

            adapterAsSetFormat->SetFormat(waveFormat.get());
        }
    }

    if (m_compressedAudioAdapter == nullptr)
    {
        /* Select which stream to use (use codec if available) */
        auto streamToUse = [&]()
        {
            if (m_codecAdapter)
            {
                return SpxQueryInterface<ISpxAudioStream>(m_codecAdapter);
            }
            return stream;
        }();

        // Attach the stream to the pump
        QueryAndInvokeMember<ISpxAudioSourceInit>(m_audioShim, &ISpxAudioSourceInit::InitFromStream, streamToUse);

        Set(PropertyId::AudioConfig_AudioSource, g_audioSourceStream);
        SetAudioConfigurationInProperties();
        m_isReliableDelivery = true;
    }
}

void CSpxAudioStreamSession::SetAudioConfigurationInProperties()
{
    auto waveFormat = m_audioShim->GetFormat();

    // Make sure channel count is consistent if it has been set.
    if (auto maybeChannelCountProperty = Get(PropertyId::AudioConfig_NumberOfChannelsForCapture))
    {
        SPX_THROW_HR_IF(SPXERR_RUNTIME_ERROR, std::stoi(maybeChannelCountProperty.Get()) != waveFormat->nChannels);
    }
    else
    {
        Set(PropertyId::AudioConfig_NumberOfChannelsForCapture, to_string(waveFormat->nChannels).c_str());
    }

    Set(PropertyId::AudioConfig_SampleRateForCapture, to_string(waveFormat->nSamplesPerSec).c_str());
    Set(PropertyId::AudioConfig_BitsPerSampleForCapture, to_string(waveFormat->wBitsPerSample).c_str());
}

void CSpxAudioStreamSession::WriteTracingEvent()
{
#ifdef _WIN32
    auto sessionId = PAL::ToString(m_sessionId);
    auto micName = GetStringValue("SPEECH-MicrophoneNiceName", "");

    TraceLoggingWrite(tracingEventProvider, "RecognizerCreationEvent",
        TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
        TraceLoggingString(sessionId.c_str(), "SessionId"),
        TraceLoggingString(micName.c_str(), "Microphone"));
#endif
}

void CSpxAudioStreamSession::SetFormat(const SPXWAVEFORMATEX* pformat)
{
    uint16_t sizeOfFormat = 0;
    SpxWAVEFORMATEX_Type format = nullptr;
    if (pformat)
    {
        sizeOfFormat = sizeof(SPXWAVEFORMATEX) + pformat->cbSize;
        format = SpxAllocWAVEFORMATEX(sizeOfFormat);
        memcpy(format.get(), pformat, sizeOfFormat);
    }

    auto task = CreateTask([this, format]() {
        SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::SetFormat: format %s nullptr", (void*)this, format == nullptr ? "==" : "!=");
        EnsureValidToken();
        if (format != nullptr && TryChangeState(SessionState::WaitForPumpSetFormatStart, SessionState::ProcessingAudio))
        {
            // The pump started successfully, we have a live running session now!
            SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::SetFormat: Now ProcessingAudio ...", (void*)this);
            m_saveToWavEverything.OpenWav("everything-audio-", format.get());

            InformAdapterSetFormatStarting(format.get());
            // If there was stop requested during the audio start-up, let it proceed.
            m_stopCondVar.notify_one();
        }
        else if (format == nullptr && TryChangeState(SessionState::StoppingPump, SessionState::WaitForAdapterCompletedSetFormatStop))
        {
            // Our stop pump request has been satisfied... Let's wait for the adapter to finish...
            SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::SetFormat: Now WaitForAdapterCompletedSetFormatStop (from StoppingPump)...", (void*)this);
            InformAdapterSetFormatStopping(SessionState::StoppingPump);
        }
        else if (format == nullptr && TryChangeState(SessionState::ProcessingAudio, SessionState::ProcessingAudioLeftovers))
        {
            // The pump stopped itself... That's possible when WAV files reach EOS. Let's wait for the adapter to finish...
            SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::SetFormat: Have seen the end of the stream on the client, processing audio leftovers ...", (void*)this);

            auto pumpStoppedOnError = GetOr<bool>("SPEECH-PumpStoppedOnError", false);
            if (pumpStoppedOnError)
            {
                // Ensure that the adapter gets an indication of the end of audio.
                if (m_audioProcessor)
                {
                    SPX_TRACE_INFO("[%p]CSpxAudioStreamSession::SetFormat - Send zero size audio, processor=%p", (void*)this, (void*)m_audioProcessor.get());
                    m_audioProcessor->ProcessAudio(std::make_shared<DataChunk>(nullptr, 0));
                }
                SetStringValue("SPEECH-PumpStoppedOnError", "");
                m_canceledOnError = true;
            }

            if (m_compressedAudioAdapter == nullptr)
            {
                // Currently there are no sufficient tests for KWS, so we preserve the old logic,
                // For single shot and continuous we are processing the last pieces of data without indicating the end of the stream.
                // In VAD detection mode: the state Detection/ProcessingAudioLeftovers (1/6) in continuous recognition,
                // has no trigger like adapter turn end to change states, So here it needs to go to WaitForAdapterCompletedSetFormatStop for setformat null for completing current session.
                if ((CurrentStateMatches({ RecognitionKind::Detection }) && GetOr<bool>(g_Detection_ProcessingVAD, false))
                    || !m_audioBuffer->NonAcknowledgedSizeInBytes() || !CurrentStateMatches({ RecognitionKind::SingleShot, RecognitionKind::Continuous }))
                {
                    SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::SetFormat: Now WaitForAdapterCompletedSetFormatStop (from ProcessingAudio) ...", (void*)this);

                    (void)TryChangeState(SessionState::ProcessingAudioLeftovers, SessionState::WaitForAdapterCompletedSetFormatStop);
                    InformAdapterSetFormatStopping(SessionState::ProcessingAudio);
                    EncounteredEndOfStream();
                }
            }
        }
        else if (format == nullptr && CurrentStateMatches({ RecognitionKind::DetectionSingleShot, RecognitionKind::Detection, RecognitionKind::DetectionOnceSingleShot }, { SessionState::HotSwapPaused }))
        {
            // The pump stopped itself, probably due to the end of stream.
            // Wait until the hot swap is done before taking action.
            SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::SetFormat: AudioPump thread has stopped!", (void*)this);
            m_audioPumpStoppedBeforeHotSwap = true;
        }
        else
        {
            SPX_THROW_HR(SPXERR_SETFORMAT_UNEXPECTED_STATE_TRANSITION);
        }
    });

    m_threadService->ExecuteAsync(std::move(task));
}

void CSpxAudioStreamSession::SetStringValue(const char* name, const char* value)
{
    ISpxPropertyBagImpl::SetStringValue(name, value);
    if (name != nullptr && value != nullptr && PAL::stricmp(name, "service.auth.token.expirems") == 0 && PAL::stricmp(value, "infinite") != 0)
    {
        ScheduleTokenRefresh();
    }
}

void CSpxAudioStreamSession::ProcessAudio(const DataChunkPtr& audioChunk)
{
    SPX_TRACE_VERBOSE("Received audio chunk: time: %s, size:%d.", PAL::GetTimeInString(audioChunk->receivedTime).c_str(), audioChunk->size);
    uint64_t nonAcknowledgedSizeInBytes = 0;
    milliseconds nonAcknowledgedSizeInMsec{};
    if (m_compressedAudioAdapter == nullptr)
    {
        if (!m_audioBuffer || m_isDisposing)
        {
            SPX_DBG_TRACE_VERBOSE("%s: [1] Session has been shutdown while processing was in flight, buffer has already been destroyed", __FUNCTION__);
            return;
        }

        nonAcknowledgedSizeInBytes = m_audioBuffer->NonAcknowledgedSizeInBytes();
        nonAcknowledgedSizeInMsec = BytesToDuration<milliseconds>(nonAcknowledgedSizeInBytes, m_throttleLogic->GetAverageBytesPerSecond());

        SPX_DBG_TRACE_VERBOSE("%s: Non-acknowledged size = %" PRIu64 " bytes (%" PRIu64 " msec)", __FUNCTION__, nonAcknowledgedSizeInBytes, static_cast<uint64_t>(nonAcknowledgedSizeInMsec.count()));

        SlowDownThreadIfNecessary(audioChunk->size, nonAcknowledgedSizeInMsec);
    }
    else
    {
        // We need to find a way to throttle. Since this is compressed stream.
        // one option is to let the application throttle it at the pull stream read or push stream write.
        // other option is to parse the stream a bit to find the kbps of the stream and find the average number of bytes per seconds
        // throttle to make it 2x faster.
        // open for discussion.
    }

    auto task = CreateTask([=]() {
        if (m_compressedAudioAdapter == nullptr)
        {
            if (!m_audioBuffer || m_isDisposing)
            {
                SPX_DBG_TRACE_VERBOSE("%s: [2] Session has been shutdown while processing was in flight, buffer has already been destroyed", 
                    __FUNCTION__);
                return;
            }

            if (nonAcknowledgedSizeInMsec > m_throttleLogic->GetMaxDuration())
            {
                // Drop everything from the buffer
                SPX_DBG_TRACE_VERBOSE("%s: Overflow happened, dropping the buffer and resetting the adapter, non-acknowledged duration %" PRIu64 " msec.", 
                    __FUNCTION__, 
                    static_cast<uint64_t>(nonAcknowledgedSizeInMsec.count()));
                m_audioBuffer->Drop();

                if (m_recoAdapter)
                {
                    // Flush telemetry before resetting the adapter.
                    m_recoAdapter->FlushTelemetry();
                }

                // Send the error and reset the adapter
                constexpr auto message = "Due to service inactivity, the client buffer exceeded maximum size. Resetting the buffer.";
                auto error = ErrorInfo::FromExplicitError(CancellationErrorCode::ServiceTimeout, message);
                Error(m_recoAdapter.get(), error);
                return;
            }

            // Add the chunk to the buffer.
            if (!m_audioBuffer || !m_audioProcessor || m_isDisposing)
            {
                SPX_DBG_TRACE_VERBOSE("%s: [3] Session has been shutdown while processing was in flight, buffer/processor has already been destroyed", 
                    __FUNCTION__);
                return;
            }

            m_saveToWavEverything.SaveToWav(audioChunk->data.get(), audioChunk->size);
            m_audioBuffer->Add(audioChunk);

            // In the common case, the single audio chunk we added above gets processed (sent to the cloud)
            // in a single round of the while loop below. Note however that there is no attempt to slow down the while loop if there are multiple rounds.
            // For example, if we have unacknowledged audio and we get a new turn, all that unacknowledged audio is now marked as stashed audio,
            // and it will be resent to the cloud using multiple rounds of this while loop without slowdown.
            while (ProcessNextAudio())
            {
            }
        }
        else
        {
            m_audioProcessor->ProcessAudio(audioChunk);
        }
    });

    // Synchronously execute on the background thread.
    m_threadService->ExecuteAsync(std::move(task));
}

void CSpxAudioStreamSession::SlowDownThreadIfNecessary(uint32_t dataSize, std::chrono::milliseconds nonAcknowledgedSizeInMsec)
{
    m_bytesTransited += dataSize;

    auto audioPacketDelay = m_throttleLogic->GetPacketAudioDelay(m_bytesTransited, dataSize, nonAcknowledgedSizeInMsec);
    auto sleepDuration = m_useDurationBasedThrottle
        ? audioPacketDelay
        : duration_cast<milliseconds>(m_nextAudioProcessTime - steady_clock::now());

    if (sleepDuration > milliseconds::zero())
    {
        SPX_DBG_TRACE_VERBOSE(
            "[%p]CSpxAudioStreamSession::SlowDownThreadIfNecessary: Stashing ... sleeping for %" PRIu64 " ms",
            (void*)this,
            static_cast<uint64_t>(sleepDuration.count()));
        m_audioProcessSleepWrapper.sleep_for(sleepDuration);
    }

    m_nextAudioProcessTime = steady_clock::now() + audioPacketDelay;
}

bool CSpxAudioStreamSession::ProcessNextAudio()
{
    if ((m_sessionState == SessionState::ProcessingAudio || m_sessionState == SessionState::ProcessingAudioLeftovers)
        && !m_adapterAudioMuted)
    {
        AudioBufferPtr buffer;
        std::shared_ptr<ISpxAudioProcessor> processor;
        std::shared_ptr<ISpxAudioProcessor> speechProcessor;
        DetectionProcessorMode detectionProcessorMode = DetectionProcessorMode::None;

        {
            buffer = m_audioBuffer;
            processor = m_audioProcessor;
            speechProcessor = m_speechProcessor;
            detectionProcessorMode = m_detectionProcessorMode;
        }

        if (!buffer || !processor || m_isDisposing)
        {
            SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::ProcessNextAudio: Session has been shutdown while processing was in flight, buffer/processor has already been destroyed", (void*)this);
            return false;
        }

        auto item = buffer->GetNext();
        if (item)
        {
            if (detectionProcessorMode != DetectionProcessorMode::None && !m_isMultiKeywordRecognition)
            {
                // In KWS and VAD (Detection Mode), session chunks are discarded, because currently
                // there is no ACKING logic from KWS/VAD adapter. (In multi-keyword recognition the
                // adapter acknowledges and discards processed audio normally.)
                buffer->DiscardBytes(item->size);
                if (GetOr<bool>(g_Detection_ProcessingVAD, false))
                {
                    // In detection mode, discarded bytes are added to the m_bufferStartOffsetInBytesAbsolute in function DiscardBytesUnlocked.
                    // So in VAD processing, substract the discarded bytes from m_bufferStartOffsetInBytesAbsolute to get a correct offset for sr adapters.
                    // The sr adapters count offset starting from the first audio chunk it received for each utterance, it should not include the discarded bytes by VAD.
                    buffer->SetCurrentOffset(buffer->GetCurrentOffset() - item->size);
                }
            }

            processor->ProcessAudio(item);

            if (speechProcessor)
            {
                speechProcessor->ProcessAudio(item);
            }

            if (item->size > 0)
            {
                m_fireEndOfStreamAtSessionStop = false;
            }

            return true;
        }
        else
        {
            SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::ProcessNextAudio: done processing all audio chunks", (void*)this);
        }
    }
    else if (m_sessionState == SessionState::HotSwapPaused || m_adapterAudioMuted)
    {
        // Don't process this data, if we're paused, it has been buffered...
        SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::ProcessNextAudio Saving for later ... sessionState %d; adapterRequestedIdle %s", (void*)this, static_cast<int>(m_sessionState), m_adapterAudioMuted ? "true" : "false");
    }
    else if (m_sessionState == SessionState::StoppingPump)
    {
        // Don't process this data if we're actively stopping, it is buffered and
        // we will process it if the source is resilient...
        SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::ProcessNextAudio: Stopping pump, not processing data", (void*)this);
    }
    else
    {
        SPX_TRACE_ERROR("[%p]CSpxAudioStreamSession::ProcessNextAudio: Unexpected SessionState: recoKind %d; sessionState %d", (void*)this, static_cast<int>(m_recoKind), static_cast<int>(m_sessionState));
    }

    return false;
}

const std::wstring& CSpxAudioStreamSession::GetSessionId() const
{
    return m_sessionId;
}

void CSpxAudioStreamSession::AddRecognizer(std::shared_ptr<ISpxRecognizer> recognizer)
{
    SPX_DBG_TRACE_FUNCTION();

    // Create offline RNN-T adapter now to avoid start-up latency with recognition later
    bool useRnnt = IsUsingRecoEngineRnnt();
    if (useRnnt)
    {
        InitRecoEngineAdapter();
    }

    unique_lock<mutex> lock(m_recognizersLock);
    m_recognizers.push_back(recognizer);
}

void CSpxAudioStreamSession::SetConversation(std::shared_ptr<ISpxConversation> conversation)
{
    SPX_DBG_TRACE_FUNCTION();

    unique_lock<mutex> lock(m_conversationLock);
    m_conversation = conversation;
}

void CSpxAudioStreamSession::SetMeeting(std::shared_ptr<ISpxMeeting> meeting)
{
    SPX_DBG_TRACE_FUNCTION();

    unique_lock<mutex> lock(m_meetingLock);
    m_meeting = meeting;
}

void CSpxAudioStreamSession::SetDisposing()
{
    SPX_DBG_TRACE_FUNCTION();
    m_isDisposing = true;
}

void CSpxAudioStreamSession::RemoveRecognizer(ISpxRecognizer* recognizer)
{
    SPX_DBG_TRACE_FUNCTION();

    unique_lock<mutex> lock(m_recognizersLock);
    m_recognizers.remove_if([&](weak_ptr<ISpxRecognizer>& item)
    {
        return item.lock().get() == recognizer;
    });
}

void CSpxAudioStreamSession::OpenConnection(bool forContinuousRecognition)
{
    auto task = CreateTask([=]() {
        SPX_THROW_HR_IF(SPXERR_CHANGE_CONNECTION_STATUS_NOT_ALLOWED, !CanChangeConnection());
        EnsureInitRecoEngineAdapter();
        EnsureValidToken();
        m_recoAdapter->OpenConnection(forContinuousRecognition ? false : true);
    }, false);

    shared_future<void> taskFuture(task.get_future());
    promise<bool> executed;
    shared_future<bool> executedFuture(executed.get_future());
    m_threadService->ExecuteAsync(std::move(task), ISpxThreadService::Affinity::Background, std::move(executed));
    if (executedFuture.get())
    {
        taskFuture.get();
    }
}

void CSpxAudioStreamSession::CloseConnection()
{
    auto task = CreateTask([=]() {
        SPX_THROW_HR_IF(SPXERR_CHANGE_CONNECTION_STATUS_NOT_ALLOWED, !CanChangeConnection());
        if (m_recoAdapter != nullptr)
        {
            m_recoAdapter->CloseConnection();
        }
    }, false);

    shared_future<void> taskFuture(task.get_future());
    promise<bool> executed;
    shared_future<bool> executedFuture(executed.get_future());
    m_threadService->ExecuteAsync(std::move(task), ISpxThreadService::Affinity::Background, std::move(executed));
    if (executedFuture.get())
    {
        taskFuture.get();
    }
}

// Thread safe method used on the boundary of the API, gets called by incoming threads.
void CSpxAudioStreamSession::WaitForIdle(std::chrono::milliseconds timeout)
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::WaitForIdle timeout: %" PRIu64 " msec", (void*)this, static_cast<uint64_t>(timeout.count()));
    unique_lock<mutex> lock(m_stateMutex);

    auto success = m_cv.wait_for(lock, timeout, [&]
    {
        SPX_DBG_TRACE_VERBOSE("CSpxAudioStreamSession::WaitForIdle m_cv.wait_for, m_sessionState: %d, m_recoKind: %d, m_sessionStarted: %d, m_sessionStopped: %d", 
            static_cast<int>(m_sessionState), static_cast<int>(m_recoKind), m_sessionStarted.load(), m_sessionStopped.load());

        // End waiting if session state goes to Idle and session stop event gets raised (if session was ever started) OR
        // if we are doing keyword and processing audio
        bool endWaiting = (m_sessionState == SessionState::Idle && (!m_sessionStarted || m_sessionStopped)) ||
                          (m_recoKind == RecognitionKind::Detection && m_sessionState == SessionState::ProcessingAudio);

        return endWaiting;
    });
    if (success)
    {
        return;
    }
    SPX_TRACE_WARNING("[%p]CSpxAudioStreamSession::WaitForIdle: Timeout happened during waiting for Idle", (void*)this);
    /* If we had a timeout waiting for idle, we should cleanup and set for the adapter to be reset */
    /********************** NOTE ***********************\
     * THIS IS A POINT IN TIME FIX, THE NEW SESSION WILL
     * OFFLOAD HANDLING OF THIS TYPE OF RELIABILITY
     * PROBLEMS TO A DIFFERENT LAYER
    \***************************************************/
    if (m_adapterStreamingAudio)
    {
        // If the user is expecting session stop fire it along with a cancellation
        SPX_DBG_TRACE_VERBOSE("CSpxAudioStreamSession::WaitForIdle set m_adapterStreamingAudio to false");
        m_adapterStreamingAudio = false;
        auto result = CreateErrorResult(
            ErrorInfo::FromExplicitError(
                CancellationErrorCode::ServiceTimeout,
                "Timeout while waiting for service to stop"));
        EnsureFireSessionStopped();
        FireResultEvent(m_sessionId, result);
    }
    SPX_DBG_TRACE_VERBOSE("CSpxAudioStreamSession::WaitForIdle set m_adapterResetPending true");
    m_adapterResetPending = true;
    SPX_DBG_TRACE_VERBOSE("CSpxAudioStreamSession::WaitForIdle EnsureResetEngineEngineAdapterComplete");
    EnsureResetEngineEngineAdapterComplete();
    lock.unlock();
    AdapterCompletedSetFormatStop(AdapterDoneProcessingAudio::Speech);
}

CSpxAsyncOp<std::string> CSpxAudioStreamSession::SendActivityAsync(std::string activity)
{
    SPX_DBG_TRACE_FUNCTION();

    auto keep_alive = SpxSharedPtrFromThis<ISpxSession>(this);

    /* Need to change thread service to support generic tasks */
    std::shared_future<std::string> taskFuture = std::async(std::launch::async, [this, activity{ std::move(activity) }, keep_alive]()
    {
        auto interaction_id = CSpxInteractionIdProvider::NextInteractionId(InteractionIdPurpose::Activity);

        auto task = CreateTask([&]() {
            ajv::JsonBuilder message;
            message["version"] = "0.5";
            message["context"]["interactionId"] = interaction_id;
            message["messagePayload"] = ajv::json::Parse(activity);
            EnsureInitRecoEngineAdapter();
            m_recoAdapter->SendAgentMessage(message.AsJson());
        });
        std::promise<bool> executed;
        std::shared_future<bool> executedFuture{ executed.get_future() };
        m_threadService->ExecuteAsync(std::move(task), ISpxThreadService::Affinity::Background, std::move(executed));
        if (!executedFuture.get())
        {
            // The task has not been even executed, throwing.
            SPX_THROW_HR(SPXERR_UNEXPECTED_CREATE_OBJECT_FAILURE);
        }

        return interaction_id;
    });
    return CSpxAsyncOp<std::string>{ taskFuture, AOS_Started};
}

CSpxAsyncOp<std::shared_ptr<ISpxRecognitionResult>> CSpxAudioStreamSession::RecognizeAsync()
{
    SPX_DBG_TRACE_FUNCTION();

    // Wait till single shot result has been received and report to the user.
    // Because we are blocking here, we do this on std::async.
    auto keepAlive = SpxSharedPtrFromThis<ISpxSession>(this);
    shared_future<shared_ptr<ISpxRecognitionResult>> waitForResult(async(launch::async, [this, keepAlive]() {

        auto singleShotInFlight = make_shared<Operation>(RecognitionKind::SingleShot);
        auto task = CreateTask([=]() { RecognizeOnceAsync(singleShotInFlight); });

        // Wait for the task execution.
        promise<bool> executed;
        shared_future<bool> executedFuture(executed.get_future());
        m_threadService->ExecuteAsync(std::move(task), ISpxThreadService::Affinity::Background, std::move(executed));
        if (!executedFuture.get())
        {
            // The task has not been even executed, throwing.
            // This can happen i.e. if the user disposing the recognizer while
            // async is still in flight.
            SPX_THROW_HR(SPXERR_UNEXPECTED_CREATE_OBJECT_FAILURE);
        }

        auto result = singleShotInFlight->m_future.get();
        // Make sure we are in Idle state due to end turn.
        WaitForIdle(GetStopRecognitionTimeout());
        return result;
    }));

    return CSpxAsyncOp<shared_ptr<ISpxRecognitionResult>>(waitForResult, AOS_Started);
}

void CSpxAudioStreamSession::RecognizeOnceAsync(const shared_ptr<Operation>& singleShot, std::shared_ptr<ISpxKwsModel> model)
{
    SPX_DBG_TRACE_SCOPE("*** CSpxAudioStreamSession::RecognizeAsync kicked-off THREAD started ***", "*** CSpxAudioStreamSession::RecognizeAsync kicked-off THREAD stopped ***");

    auto& currentSingleShot = m_singleShotInFlight;

    // Keep track of the fact that we have a thread pending waiting to hear
    // what the final recognition result is, and then start/stop recognizing...
    if (currentSingleShot != nullptr)
    {
        // There is another single shot in flight. Report an error.
        singleShot->m_promise.set_exception(
            make_exception_ptr(ExceptionWithCallStack(SPXERR_START_RECOGNIZING_INVALID_STATE_TRANSITION)));
        return;
    }

    // Start recognizing.
    currentSingleShot = singleShot;
    StartRecognizing(singleShot->m_kind, model);

    // In VAD processor mode, the operation timeout also need to be applied here. This is to avoid VAD waiting for voice activity and never stop.
    // Keep VAD having the same behaviour as vad off in singleshot mode.
    if (singleShot->m_kind == RecognitionKind::SingleShot || (singleShot->m_kind == RecognitionKind::DetectionOnce && GetOr<bool>(g_Detection_VadModeOn, false)))
    {
        // Schedule timeout for the operation.
        auto cancelTimer = CreateTask([this, singleShot]() {
            auto status = singleShot->m_future.wait_for(0ms);
            if (status != future_status::ready &&
                m_singleShotInFlight &&
                m_singleShotInFlight->m_operationId == singleShot->m_operationId)
            {
                // No result, timeout it.
                EnsureFireResultEvent();
            }
        });

        auto operationTimeout = GetOr<std::chrono::milliseconds>("SPEECH-OneShotRecognition-OperationTimeoutMs", Operation::Timeout);
        m_threadService->ExecuteAsync(std::move(cancelTimer), operationTimeout);
    }
}

CSpxAsyncOp<void> CSpxAudioStreamSession::StartContinuousRecognitionAsync()
{
    return StartRecognitionAsync(RecognitionKind::Continuous);
}

CSpxAsyncOp<void> CSpxAudioStreamSession::StopContinuousRecognitionAsync()
{
    return StopRecognitionAsync(RecognitionKind::Continuous);
}

CSpxAsyncOp<std::shared_ptr<ISpxRecognitionResult>> CSpxAudioStreamSession::RecognizeAsyncWithVAD()
{
    SPX_DBG_TRACE_FUNCTION();

    auto keepAlive = SpxSharedPtrFromThis<ISpxSession>(this);
    std::shared_future<std::shared_ptr<ISpxRecognitionResult>> waitForResult(std::async(std::launch::async, [this, keepAlive]()
        {
            /* First we check that we are in fact idle to catch the case when the user didn't
             * obtain the stream (so it didn't detach explicitly).
             */
            if (m_recoKind == RecognitionKind::DetectionOnceSingleShot)
            {
                auto retrievable = SpxQueryInterface<ISpxRetrievable>(m_recoAdapter);
                if (retrievable && !retrievable->WasRetrieved())
                {
                    auto streamWrapper = SpxQueryInterface<ISpxAudioDataStreamWrapper>(m_recoAdapter);
                    streamWrapper->DetachInput();
                }
            }
            auto singleShotInFlight = std::make_shared<Operation>(RecognitionKind::DetectionOnce);
            // Use m_originalSingleShotInFlight to store the pass in singleShotInFlight for current session.
            // After adapter swap, a new singleShotInFlight will be created, so recognized results need to fulfill the original singleShotInFlight.
            m_originalSingleShotInFlight = singleShotInFlight;
            m_GatedOffset = 0;
            auto task = CreateTask([=]()
                {
                    RecognizeOnceAsync(singleShotInFlight);
                });

            // Wait for the task execution.
            std::promise<bool> executed;
            std::shared_future<bool> executedFuture(executed.get_future());
            m_threadService->ExecuteAsync(std::move(task), ISpxThreadService::Affinity::Background, std::move(executed));
            if (!executedFuture.get())
            {
                // The task has not been even executed, throwing.
                // This can happen i.e. if the user disposing the recognizer while
                // async is still in flight.
                SPX_THROW_HR(SPXERR_UNEXPECTED_CREATE_OBJECT_FAILURE);
            }

            auto result = singleShotInFlight->m_future.get();
            WaitForIdle(GetStopRecognitionTimeout());
            return result;
        }));

    return CSpxAsyncOp<shared_ptr<ISpxRecognitionResult>>(waitForResult, AOS_Started);
}

CSpxAsyncOp<std::shared_ptr<ISpxRecognitionResult>> CSpxAudioStreamSession::RecognizeAsync(std::shared_ptr<ISpxKwsModel> model)
{
    SPX_DBG_TRACE_FUNCTION();

    // throw exception if vad is enabled for kws
    SPX_THROW_HR_IF(SPXERR_VAD_CANNOT_BE_USED_WITH_KEYWORD_RECOGNIZER, GetOr<bool>(g_Detection_VadModeOn, false));

    // Wait till single shot result has been received and report to the user.
    // Because we are blocking here, we do this on std::async.
    auto keepAlive = SpxSharedPtrFromThis<ISpxSession>(this);
    std::shared_future<std::shared_ptr<ISpxRecognitionResult>> waitForResult(std::async(std::launch::async, [this, model, keepAlive]()
    {
        /* First we check that we are in fact idle to catch the case when the user didn't
         * obtain the stream (so it didn't detach explicitly).
         */
        if (m_recoKind == RecognitionKind::DetectionOnceSingleShot)
        {
            auto retrievable = SpxQueryInterface<ISpxRetrievable>(m_recoAdapter);
            if (retrievable && !retrievable->WasRetrieved())
            {
                auto streamWrapper = SpxQueryInterface<ISpxAudioDataStreamWrapper>(m_recoAdapter);
                streamWrapper->DetachInput();
            }
        }
        auto singleShotInFlight = std::make_shared<Operation>(RecognitionKind::DetectionOnce);
        auto task = CreateTask([=]()
        {
            RecognizeOnceAsync(singleShotInFlight, model);
        });

        // Wait for the task execution.
        std::promise<bool> executed;
        std::shared_future<bool> executedFuture(executed.get_future());
        m_threadService->ExecuteAsync(std::move(task), ISpxThreadService::Affinity::Background, std::move(executed));
        if (!executedFuture.get())
        {
            // The task has not been even executed, throwing.
            // This can happen i.e. if the user disposing the recognizer while
            // async is still in flight.
            SPX_THROW_HR(SPXERR_UNEXPECTED_CREATE_OBJECT_FAILURE);
        }

        /* This function is meant for KWS results so no need to get back to idle */
        return singleShotInFlight->m_future.get();
    }));

    return CSpxAsyncOp<shared_ptr<ISpxRecognitionResult>>(waitForResult, AOS_Started);
}

CSpxAsyncOp<void> CSpxAudioStreamSession::StartContinuousRecognitionAsyncWithVAD()
{
    m_GatedOffset = 0;
    return StartRecognitionAsync(RecognitionKind::Detection);
}

CSpxAsyncOp<void> CSpxAudioStreamSession::StopContinuousRecognitionAsyncWithVAD()
{
    if (m_recoKind == RecognitionKind::DetectionOnceSingleShot)
    {
        /* For StopContinuousRecognitionAsyncWithVAD stopping is equivalent to Detaching the input */
        auto streamWrapper = m_recoAdapter->QueryInterface<ISpxAudioDataStreamWrapper>();
        if (streamWrapper != nullptr)
        {
            streamWrapper->DetachInput();
            return CSpxAsyncOp<void>::FromResult();
        }
    }

    return StopRecognitionAsync(m_recoKind == RecognitionKind::DetectionOnce ? m_recoKind : RecognitionKind::Detection);
}

CSpxAsyncOp<void> CSpxAudioStreamSession::StartKeywordRecognitionAsync(std::shared_ptr<ISpxKwsModel> model)
{
    // throw exception if vad is enabled for kws
    SPX_THROW_HR_IF(SPXERR_VAD_CANNOT_BE_USED_WITH_KEYWORD_RECOGNIZER, GetOr<bool>(g_Detection_VadModeOn, false));

    return StartRecognitionAsync(RecognitionKind::Detection, model);
}

CSpxAsyncOp<void> CSpxAudioStreamSession::StopKeywordRecognitionAsync()
{
    if (m_recoKind == RecognitionKind::DetectionOnceSingleShot)
    {
        /* For KWSOnceSingleShot stopping is equivalent to Detaching the input */
        auto streamWrapper = m_recoAdapter->QueryInterface<ISpxAudioDataStreamWrapper>();
        if (streamWrapper != nullptr)
        {
            streamWrapper->DetachInput();
            return CSpxAsyncOp<void>::FromResult();
        }
    }
    return StopRecognitionAsync(m_recoKind == RecognitionKind::DetectionOnce ? m_recoKind : RecognitionKind::Detection);
}

CSpxAsyncOp<void> CSpxAudioStreamSession::StartRecognitionAsync(RecognitionKind startKind, std::shared_ptr<ISpxKwsModel> model)
{
    SPX_DBG_TRACE_FUNCTION();
    auto keepAlive = SpxSharedPtrFromThis<ISpxSession>(this);
    shared_future<void> started(async(launch::async, [=]() {
        SPX_DBG_TRACE_SCOPE("*** CSpxAudioStreamSession::StartRecognitionAsync kicked-off THREAD started ***", "*** CSpxAudioStreamSession::StartRecognitionAsync kicked-off THREAD stopped ***");

        auto task = CreateTask([=]() { StartRecognizing(startKind, model); }, false);

        shared_future<void> taskFuture(task.get_future());
        promise<bool> executed;
        shared_future<bool> executedFuture(executed.get_future());
        m_threadService->ExecuteAsync(std::move(task), ISpxThreadService::Affinity::Background, std::move(executed));
        if (executedFuture.get())
        {
            taskFuture.get();
        }
    }));

    return CSpxAsyncOp<void>(started, AOS_Started);
}

CSpxAsyncOp<void> CSpxAudioStreamSession::StopRecognitionAsync(RecognitionKind stopKind)
{
    SPX_DBG_TRACE_FUNCTION();

    // If audio pump is starting while we are being disposed, wait for state to change to ProcessingAudio for better synchronization between start and stop.
    if (m_isDisposing && m_sessionState == SessionState::WaitForPumpSetFormatStart)
    {
        SPX_DBG_TRACE_VERBOSE("CSpxAudioStreamSession::StopRecognitionAsync, audio pump is starting, wait to start and then do stop");
        std::unique_lock<std::mutex> lck(m_stopMutex);
        m_stopCondVar.wait_for(lck, 100ms);
    }

    // Wait till the session switches to Idle state.
    // Because we are blocking here, we do this on std::async.
    auto keepAlive = SpxSharedPtrFromThis<ISpxSession>(this);
    shared_future<void> waitForIdle(async(launch::async,
    [this, keepAlive, stopKind]()
    {
        auto task = CreateTask([=]() {
            SPX_DBG_TRACE_SCOPE("*** CSpxAudioStreamSession::StopRecognitionAsync kicked-off THREAD started ***", "*** CSpxAudioStreamSession::StopRecognitionAsync kicked-off THREAD stopped ***");
            StopRecognizing(stopKind);
        });

        shared_future<void> taskFuture(task.get_future());
        promise<bool> executed;
        shared_future<bool> stoppedStarted(executed.get_future());
        m_threadService->ExecuteAsync(std::move(task), ISpxThreadService::Affinity::Background, std::move(executed));
        if (stoppedStarted.get())
        {
            taskFuture.get();
        }

        WaitForIdle(GetStopRecognitionTimeout());
    }));

    return CSpxAsyncOp<void>(waitForIdle, AOS_Started);
}

void CSpxAudioStreamSession::StartRecognizing(RecognitionKind startKind, std::shared_ptr<ISpxKwsModel> model)
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::StartRecognizing", (void*)this);

    if (m_recoKind == startKind
        && (m_recoKind == RecognitionKind::Detection || m_recoKind == RecognitionKind::DetectionOnce)
        && ((m_kwsModel && (model->GetFileName() == m_kwsModel->GetFileName())) || GetOr<bool>(g_Detection_ProcessingVAD, false)))
    {
        SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::StartRecognizing: Already recognizing keyword/vad, ignoring call...", (void*)this);
        return;
    }

    if (model && model != m_kwsModel)
    {
        // Check for multi-keyword recognition.
        auto propertyBag = SpxQueryInterface<ISpxNamedProperties>(model);
        m_isMultiKeywordRecognition = propertyBag->GetOr<bool>(g_isMultiKeywordRecognition, false);

        if (m_isMultiKeywordRecognition)
        {
            std::string propertyString;

            // Copy settings from the model for the multi-keyword reco adapter.
            propertyString = propertyBag->GetOr(g_keywordRecognitionModelPath, "");
            Set(g_keywordRecognitionModelPath, propertyString);

            propertyString = propertyBag->GetOr(PropertyId::KeywordRecognition_ModelKey, "");
            Set(PropertyId::KeywordRecognition_ModelKey, propertyString);

            propertyString = propertyBag->GetOr(g_keywordRecognitionUserDefinedWakeWords, "");
            Set(g_keywordRecognitionUserDefinedWakeWords, propertyString);
        }
    }

    if (startKind == RecognitionKind::Detection)
    {
        Ensure16kHzSampleRate();
    }

    auto originState = m_sessionState;

    if (TryChangeState(SessionState::Idle, startKind, SessionState::WaitForPumpSetFormatStart))
    {
        // We're starting from idle!! Let's get the Audio Pump running
        SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::StartRecognizing:  Now WaitForPumpSetFormatStart ...", (void*)this);

        // Make sure we drop left overs of the previous recognition
        // if the source does not need reliability guarantees.
        if (m_audioBuffer && !m_isReliableDelivery)
        {
            m_audioBuffer->Drop();
        }
        StartAudioPump(startKind, model);
    }
    else if (TryChangeState({ RecognitionKind::Detection, RecognitionKind::DetectionOnce }, { SessionState::ProcessingAudio }, startKind, SessionState::HotSwapPaused))
    {
        // We're moving from Keyword/ProcessingAudio to startKind/Paused...
        SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::StartRecognizing: Now Paused ...", (void*)this);

        HotSwapAdaptersWhilePaused(startKind, model);
    }
    else if (m_adapterResetPending && (startKind == RecognitionKind::Continuous || startKind == RecognitionKind::SingleShot
        || (GetOr<bool>(g_Detection_VadModeOn, false) && (m_recoKind == RecognitionKind::DetectionOnce || m_recoKind == RecognitionKind::Detection || m_recoKind == RecognitionKind::DetectionSingleShot)))
        && TryChangeState({ SessionState::ProcessingAudio, SessionState::HotSwapPaused, SessionState::ProcessingAudioLeftovers }, SessionState::HotSwapPaused))
    {
        SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::StartRecognizing: Resetting adapter via HotSwap. Attempting to stay in continuous mode!!! ...", (void*)this);
        HotSwapAdaptersWhilePaused(startKind, model);

        if (originState == SessionState::ProcessingAudio || originState == SessionState::HotSwapPaused)
        {
            SPX_DBG_TRACE_WARNING("[%p]CSpxAudioStreamSession::StartRecognizing: Simulating GetSite()->AdapterCompletedSetFormatStop() ...", (void*)this);
            AdapterCompletedSetFormatStop(AdapterDoneProcessingAudio::Speech);
        }
        else if (originState == SessionState::ProcessingAudioLeftovers)
        {
            // Change back and replay the leftovers.
            SPX_DBG_TRACE_WARNING("[%p]CSpxAudioStreamSession::StartRecognizing: Resending audio leftovers ...", (void*)this);
            (void)TryChangeState(SessionState::HotSwapPaused, SessionState::ProcessingAudioLeftovers);
            while (ProcessNextAudio())
            {
            }
        }
    }
    else if ((m_recoKind != RecognitionKind::Detection && m_recoKind != RecognitionKind::DetectionOnce)
        && (startKind == RecognitionKind::Detection || startKind == RecognitionKind::DetectionOnce)
        && m_sessionState == SessionState::ProcessingAudio)
    {
        // We're already doing something other than keyword spotting, but, someone wants to change/update the keyword ...
        // So ... let's update it
        SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::StartRecognizing: Changing keyword ... nothing else...", (void*)this);
        m_kwsModel = model;
    }
    else
    {
        // All other state transitions are invalid when attempting to start recognizing...
        SPX_TRACE_ERROR("[%p]CSpxAudioStreamSession::StartRecognizing: Unexpected/Invalid State Transition: recoKind %d; sessionState %d", (void*)this, static_cast<int>(m_recoKind), static_cast<int>(m_sessionState));
        SPX_THROW_HR(SPXERR_START_RECOGNIZING_INVALID_STATE_TRANSITION);
    }
}

void CSpxAudioStreamSession::StopRecognizing(RecognitionKind stopKind)
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::StopRecognizing ...", (void*)this);
    SPX_DBG_TRACE_VERBOSE_IF(GetOr<bool>(g_Detection_VadModeOn, false), "[%p]CSpxAudioStreamSession::StopRecognizing: VAD gated in ms=%" PRIu64, (void*)this, m_GatedOffset / 100000);

    if ((m_kwsModel != nullptr || (GetOr<bool>(g_Detection_ProcessingVAD, false) && !m_adapterStreamingAudio))
        && (stopKind != RecognitionKind::Detection && stopKind != RecognitionKind::DetectionOnce)
        && TryChangeState({ stopKind }, { SessionState::ProcessingAudio }, RecognitionKind::Detection, SessionState::HotSwapPaused))
    {
        // We've got a keyword/vad, we're not trying to stop keyword/VAD spotting, and we're currently processing audio...
        // So ... We should hot swap over to the keyword spotter (which will stop the current adapter doing whatever its doing)
        // stop VAD spoting ONLY when m_adapterStreamingAudio is false which means replay buffer shrink and adapter turn stop is done for current utterance. Otherwise, go to WaitForAdapterCompletedSetFormatStop state and wait for done.
        SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::StopRecognizing: Now Keyword/Paused ...", (void*)this);

        HotSwapAdaptersWhilePaused(RecognitionKind::Detection, m_kwsModel);
        if (stopKind == RecognitionKind::DetectionSingleShot || stopKind == RecognitionKind::SingleShot)
        {
            EnsureFireSessionStopped();
        }
    }
    else if ((stopKind == RecognitionKind::Detection || stopKind == RecognitionKind::DetectionOnce)
        && (m_recoKind == RecognitionKind::Detection || m_recoKind == RecognitionKind::DetectionOnce)
        && TryChangeState({ stopKind }, { SessionState::ProcessingAudio }, RecognitionKind::Detection, SessionState::StoppingPump))
    {
        // We're actually keyword/vad spotting right now, and we've been asked to stop doing that...
        // So ... Let's clear our keyword and turn off vad (since we're not spotting, we don't need it anymore)
        SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::StopRecognizing: Changing keyword to '', turn off vad. ... nothing else...", (void*)this);
        if (!m_isMultiKeywordRecognition) // avoid unnecessary multi-keyword adapter re-init on next reco
        {
            m_kwsModel.reset();
        }
        SetStringValue(g_Detection_ProcessingVAD, "false");

        // And we'll stop the pump
        SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::StopRecognizing: Now StoppingPump[%p] ...", (void*)this, (void*)m_audioShim.get());
        InvokeMemberIfNotNull(m_audioShim, &ISpxAudioSessionShim::StopAudio);
        InvokeMemberIfNotNull(m_codecAdapter, &ISpxAudioStreamReader::Close);
    }
    else if (GetOr<bool>(g_Detection_ProcessingVAD, false) && (stopKind == RecognitionKind::Detection && m_recoKind == RecognitionKind::DetectionSingleShot)
        && TryChangeState(SessionState::ProcessingAudio, SessionState::StoppingPump))
    {
        // We've been asked to stop vad spotting or vad singleshot
        // So ... Let's just turn off vad
        SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::StopRecognizing:Turn off vad, stop pump, ... nothing else...", (void*)this);
        SetStringValue(g_Detection_ProcessingVAD, "false");

        // And we'll stop the pump
        SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::StopRecognizing: Now StoppingPump[%p] ...", (void*)this, (void*)m_audioShim.get());
        InvokeMemberIfNotNull(m_audioShim, &ISpxAudioSessionShim::StopAudio);
        InvokeMemberIfNotNull(m_codecAdapter, &ISpxAudioStreamReader::Close);
    }
    else if (stopKind == RecognitionKind::Detection && m_recoKind != RecognitionKind::Detection)
    {
        // We've been asked to stop keyword/vad spotting, but we're not keyword/vad spotting right now ...
        // So ... Let's just clear the keyword and turn off vad
        SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::StopRecognizing: Changing keyword to '' ... nothing else...", (void*)this);
        if (!m_isMultiKeywordRecognition) // avoid unnecessary multi-keyword adapter re-init on next reco
        {
            m_kwsModel.reset();
        }
        SetStringValue(g_Detection_ProcessingVAD, "false");
    }
    else if (stopKind == RecognitionKind::DetectionSingleShot && m_recoKind != RecognitionKind::DetectionSingleShot)
    {
        // We've been asked to stop KwsSingleShot, but we've already stopped that, and have switched back to Keyword, or Idle
        SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::StopRecognizing: Already stopped KwsSingleShot ... recoKind %d; sessionState %d", (void*)this, static_cast<int>(m_recoKind), static_cast<int>(m_sessionState));
    }
    else if (stopKind == RecognitionKind::DetectionSingleShot
        && m_recoKind == RecognitionKind::DetectionSingleShot
        && TryChangeState(SessionState::ProcessingAudio, SessionState::WaitForAdapterCompletedSetFormatStop))
    {
        // TODO: Is this ever called?!! How does it not get eaten by the first if statement above?!!

        // We're going to wait for done ... (which will flip us back to wherever we need to go...)
        SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::StopRecognizing: Now KwsSingleShot/WaitForAdapterCompletedSetFormatStop ...", (void*)this);
    }
    else if (TryChangeState(SessionState::ProcessingAudio, SessionState::StoppingPump))
    {
        // We've been asked to stop whatever it is we're doing, while we're actively processing audio ...
        // So ... Let's stop the pump
        SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::StopRecognizing: We've been asked to stop whatever it is we're doing, while we're actively processing audio ...", (void*)this);

        auto audioShim = m_audioShim;
        SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::StopRecognizing: Now StoppingPump[%p] ...", (void*)this, (void*)audioShim.get());
        if(!InvokeMemberIfNotNull(m_audioShim, &ISpxAudioSessionShim::StopAudio))
        {
            SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::StopRecognizing: Pump has already been released", (void*)this);
        }
        InvokeMemberIfNotNull(m_codecAdapter, &ISpxAudioStreamReader::Close);
    }
    else if (m_sessionState == SessionState::WaitForAdapterCompletedSetFormatStop)
    {
        // If we're already in the WaitForAdapterCompletedSetFormatStop state... That's fine ... We'll eventually transit to Idle once AdapterCompletedSetFormatStop() is called...
        SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::StopRecognizing: Now (still) WaitForAdapterCompletedSetFormatStop ...", (void*)this);
    }
    else if (m_sessionState == SessionState::ProcessingAudioLeftovers)
    {
        // If we're already in the ProcessingAudioLeftovers state... That's fine ... We'll eventually transit to Idle once AdapterCompletedSetFormatStop() is called
        // and we processed all data.
        SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::StopRecognizing: Now (still) ProcessingAudioLeftovers ...", (void*)this);
    }
    else if (SessionIsIdle())
    {
        // If we're already in the idle state... Awesome!!!
        SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::StopRecognizing: Now Idle/Idle already...", (void*)this);
    }
    else if (m_recoKind == RecognitionKind::Detection && GetOr<bool>(g_Detection_VadModeOn, false)
        && TryChangeState(SessionState::StoppingPump, SessionState::WaitForAdapterCompletedSetFormatStop))
    {
        // For VAD, stop pump request has been satisfied early. Current session has been set format null. Let's wait for the adapter to finish...
        SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::StopRecognizing: Now WaitForAdapterCompletedSetFormatStop (from StoppingPump)...", (void*)this);
        InformAdapterSetFormatStopping(SessionState::StoppingPump);
    }
    else
    {
        SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::StopRecognizing: Unexpected State: recoKind %d; sessionState %d", (void*)this, static_cast<int>(m_recoKind), static_cast<int>(m_sessionState));
    }
}

void CSpxAudioStreamSession::WaitForRecognition_Complete(std::shared_ptr<ISpxRecognitionResult> result)
{
    SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::WaitForRecognition_Complete: ...", (void*)this);

    auto keywordOnly = GetOr<bool>(g_keyword_KeywordOnly, false);

    // In VAD mode, add back vad adapter processed offset to the recognized result.
    if (GetOr<bool>(g_Detection_VadModeOn, false))
    {
        result->SetOffset(result->GetOffset() + m_GatedOffset);
    }

    /* If this was a keyword only interaction, no result is expected */
    if (m_recoKind != RecognitionKind::DetectionOnceSingleShot || !keywordOnly)
    {
        FireResultEvent(GetSessionId(), result);
    }

    if (m_singleShotInFlight)
    {
        // Make sure we fulfill the promise if StopRecognizing throws.
        // If it is a vad detection, it needs to fullfill the promise of original singleShotInFlight.
        auto operationKind = m_singleShotInFlight->m_kind;
        auto operation = (GetOr<bool>(g_Detection_VadModeOn, false) && m_originalSingleShotInFlight) ? m_originalSingleShotInFlight : m_singleShotInFlight;
        auto finish = shared_ptr<void>(nullptr, [&](void*) { operation->m_promise.set_value(result); });

        m_singleShotInFlight = nullptr;
        // Here in continuous recognition with conversation mode, we don't stop recognition since it needs to recognize the next utterance with
        // the same sr engine. It will stop recognizing when sr engine turn ends for current session and hotswap to vad detection again.
        if (GetOr<bool>(g_Detection_VadModeOn, false) && GetOr<std::string>("SPEECH-RecoMode", "") == g_recoModeConversation) {
            return;
        }
        StopRecognizing(operationKind);
    }

    if (m_singleTextInFlight)
    {
        auto operation = m_singleTextInFlight;
        auto guard = shared_ptr<void>(nullptr, [&](void*) {
            operation->m_promise.set_value(result);
            });
        m_singleTextInFlight = nullptr;
    }
}

void CSpxAudioStreamSession::EnsureFireSessionStarted()
{
    if (m_sessionActive)
    {
        return;
    }
    SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::FireSessionStartedEvent: ...", (void*)this);
    m_sessionActive = true;

    std::wstring sessionIdOverride;
    /* For dialog service connector, we replace session id with the interaction id that's going to be send in the context message. */
    auto bIsDialogServiceConnector = GetOr<bool>(g_isDialogServiceConnector, false);
    if (bIsDialogServiceConnector)
    {
        sessionIdOverride = PAL::ToWString(CSpxInteractionIdProvider::NextInteractionId(InteractionIdPurpose::Speech));
    }

    SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::FireSessionStartedEvent: Firing SessionStarted event: SessionId: %ls", (void*)this, m_sessionId.c_str());

    FireEvent(EventType::SessionStart, nullptr, sessionIdOverride.empty() ? nullptr : sessionIdOverride.c_str());
    m_fireEndOfStreamAtSessionStop = true;
}

void CSpxAudioStreamSession::EnsureFireSessionStopped()
{
    if (!m_sessionActive)
    {
        return;
    }
    SPX_DBG_TRACE_VERBOSE(
        "[%p]CSpxAudioStreamSession::FireSessionStoppedEvent: Firing SessionStopped event: SessionId: %ls",
        (void*)this,
        m_sessionId.c_str());
    m_sessionActive = false;

    /* For dialog service connector, we replace session id with the interaction id that was sent in the context message. */
    std::wstring sessionIdOverride;
    auto bIsDialogServiceConnector = GetOr<bool>(g_isDialogServiceConnector, false);
    if (bIsDialogServiceConnector)
    {
        sessionIdOverride = PAL::ToWString(CSpxInteractionIdProvider::GetInteractionId(InteractionIdPurpose::Speech));
    }

    auto keywordOnly = GetOr<bool>(g_keyword_KeywordOnly, false);

    /* If this was a keyword only interaction, no result is expected */
    if (!keywordOnly)
    {
        EnsureFireResultEvent();
    }

    FireEvent(EventType::SessionStop, nullptr, sessionIdOverride.empty() ? nullptr : sessionIdOverride.c_str());
}

void CSpxAudioStreamSession::AdapterConnected(const std::string& url)
{
    SetStringValue("SPEECH-LastConnectedUrl", url.c_str());

    SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::AdapterConnected to %s", (void*)this, url.c_str());

    FireEvent(EventType::Connected);
}

void CSpxAudioStreamSession::AdapterDisconnected(std::shared_ptr<ISpxErrorInformation> payload)
{
    UNUSED(payload);

    SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::AdapterDisconnected", (void*)this);

    FireEvent(EventType::Disconnected);
}

void CSpxAudioStreamSession::FireConnectionMessageReceived(const std::string& headers, const std::string& path, const uint8_t* buffer, uint32_t bufferSize, bool isBufferBinary)
{
    SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::FireConnectionMessageReceived; path=%s", (void*)this, path.c_str());

    auto bufferKeepAlive = SpxAllocSharedUint8Buffer(bufferSize);
    memcpy(bufferKeepAlive.get(), buffer, bufferSize);

    auto task = CreateTask([=]() {
        ForEachRecognizer([=](auto recognizer) {
            auto ptr = SpxQueryInterface<ISpxRecognizerEvents>(recognizer);
            ptr->FireConnectionMessageReceived(headers, path, bufferKeepAlive.get(), bufferSize, isBufferBinary);
        });
    });
    m_threadService->ExecuteAsync(std::move(task), ISpxThreadService::Affinity::User);
}

void CSpxAudioStreamSession::FireSpeechStartDetectedEvent(uint64_t offset)
{
    SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::FireSpeechStartDetectedEvent", (void*)this);

    FireEvent(EventType::SpeechStart, nullptr, nullptr, (GetOr<bool>(g_Detection_VadModeOn, false) ? offset + m_GatedOffset : offset));
}

void CSpxAudioStreamSession::FireSpeechEndDetectedEvent(uint64_t offset)
{
    SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::FireSpeechEndDetectedEvent", (void*)this);

    FireEvent(EventType::SpeechEnd, nullptr, nullptr, (GetOr<bool>(g_Detection_VadModeOn, false) ? offset + m_GatedOffset : offset));
}

void CSpxAudioStreamSession::GatingAdapterFireInitialSilenceTimeout()
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::GatingFireInitialSilenceTimeout", (void*)this);

    // Gating Aadapters stops if Initial Silence is defined.
    // Currently only VAD single shot needs this function to stop gating. If SpeechServiceConnection_InitialSilenceTimeoutMs is not defined, there is no initial silence timeout for VAD.
    // Note: Continuous recognition with VAD on is undetermined.
    if (m_recoKind == RecognitionKind::DetectionOnce)
    {
        auto factory = SpxQueryService<ISpxRecoResultFactory>(SpxSharedPtrFromThis<ISpxSession>(this));
        auto result = factory->CreateFinalResult(ResultReason::NoMatch, static_cast<NoMatchReason>(0), "", 0, 0, "");

        WaitForRecognition_Complete(result);
        m_fireEndOfStreamAtSessionStop = false;
    }
}

void CSpxAudioStreamSession::EnsureFireResultEvent()
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::EnsureFireResultEvent", (void*)this);

    if (m_singleShotInFlight || (m_fireEndOfStreamAtSessionStop && m_sawEndOfStream))
    {
        SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::EnsureFireResultEvent: Getting ready to fire ResultReason::Canceled result (sawEos=%d, fireEos=%d)", (void*)this, m_sawEndOfStream, m_fireEndOfStreamAtSessionStop);

        constexpr auto timeoutErrorMessage = "Timeout: no recognition result received";

        auto factory = SpxQueryService<ISpxRecoResultFactory>(SpxSharedPtrFromThis<ISpxSession>(this));
        auto result = (m_fireEndOfStreamAtSessionStop && m_sawEndOfStream)
            ? factory->CreateEndOfStreamResult()
            : factory->CreateErrorResult(ErrorInfo::FromExplicitError(CancellationErrorCode::ServiceTimeout, timeoutErrorMessage));

        WaitForRecognition_Complete(result);
        m_fireEndOfStreamAtSessionStop = false;
    }
}

void CSpxAudioStreamSession::FireResultEvent(const std::wstring& sessionId, std::shared_ptr<ISpxRecognitionResult> result)
{
    SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::FireResultEvent", (void*)this);

    auto namedProperties = SpxQueryInterface<ISpxNamedProperties>(result);
    if (auto maybeErrorDetails = namedProperties->Get(PropertyId::SpeechServiceResponse_JsonErrorDetails))
    {
        namedProperties->Set(
            PropertyId::SpeechServiceResponse_JsonErrorDetails,
            (maybeErrorDetails.Get() + " SessionId: " + PAL::ToString(m_sessionId)).c_str());
    }

    FireEvent(EventType::RecoResultEvent, result, const_cast<wchar_t*>(sessionId.c_str()));
}

void CSpxAudioStreamSession::DispatchEvent(
    const list<weak_ptr<ISpxRecognizer>>& weakRecognizers,
    const wstring& sessionId,
    EventType sessionType,
    uint64_t offset,
    std::shared_ptr<ISpxRecognitionResult> result,
    std::string activity,
    int statusCode,
    std::shared_ptr<ISpxAudioOutput> audio)
{
    for (auto weakRecognizer : weakRecognizers)
    {
        string error;
        SPXAPI_TRY()
        {
            auto recognizer = weakRecognizer.lock();
            if (!recognizer)
            {
                continue;
            }

            auto ptr = SpxQueryInterface<ISpxRecognizerEvents>(recognizer);
            auto connectorEvents = SpxQueryInterface<ISpxDialogServiceConnectorEvents>(ptr);

            if (!ptr)
            {
                continue;
            }

            switch (sessionType)
            {
            case EventType::SessionStart:
                m_sessionStopped = false;
                ptr->FireSessionStarted(sessionId);
                m_sessionStarted = true;
                break;

            case EventType::SessionStop:
                ptr->FireSessionStopped(sessionId);
                m_sessionStopped = true;
                m_cv.notify_all();
                break;

            case EventType::SpeechStart:
                ptr->FireSpeechStartDetected(sessionId, offset);
                break;

            case EventType::SpeechEnd:
                ptr->FireSpeechEndDetected(sessionId, offset);
                break;

            case EventType::RecoResultEvent:
                ptr->FireResultEvent(sessionId, result);
                break;

            case EventType::Connected:
                ptr->FireConnected(sessionId);
                break;

            case EventType::Disconnected:
                ptr->FireDisconnected(sessionId);
                break;

            case EventType::ActivityReceivedEvent:
            {
                if (connectorEvents == nullptr)
                {
                    SPX_TRACE_ERROR("Attempted ActivityReceivedEvent with no connector event impl; ignoring");
                }
                else
                {
                    connectorEvents->FireActivityReceived(sessionId, std::move(activity), audio);
                }
                break;
            }

            case EventType::TurnStatusEvent:
            {
                if (connectorEvents == nullptr)
                {
                    SPX_TRACE_ERROR("Attempted TurnStatusEvent with no connector event impl; ignoring");
                }
                else
                {
                    connectorEvents->FireTurnStatus(PAL::ToString(sessionId), activity, statusCode);
                }
                break;
            }

            case EventType::TokenRequest:
            {
                ptr->FireTokenRequest(sessionId);
                break;
            }

            default:
                SPX_TRACE_ERROR("EventDelivery unknown event type %d", (int)sessionType);
            }
        }
        SPXAPI_CATCH_ONLY()
        // We do not communicate anything to the user here
        // in order to avoid a live lock :
        //   failed event delivery causes other failed deliveries.
    }
}

void CSpxAudioStreamSession::FireEvent(
    EventType eventType,
    shared_ptr<ISpxRecognitionResult> result,
    const wchar_t* eventSessionId,
    uint64_t offset,
    std::string activity,
    int statusCode,
    std::shared_ptr<ISpxAudioOutput> audio)
{
    if (m_isDisposing)
    {
        SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::FireEvent, recognizer is disposing, ignore events", (void*)this);

        if (eventType == EventType::SessionStop)
        {
            SPX_DBG_TRACE_VERBOSE("Marking session as stopped without firing event");
            m_sessionStopped = true;
            m_cv.notify_all();
        }
        return;
    }

    // Make a copy of the recognizers (under lock), to use to send events;
    // otherwise the underlying list could be modified while we're sending events...

    list<weak_ptr<ISpxRecognizer>> weakRecognizers;
    {
        unique_lock<mutex> lock(m_recognizersLock);
        weakRecognizers.assign(m_recognizers.begin(), m_recognizers.end());
    }

    std::wstring sessionId = (eventSessionId != nullptr) ? eventSessionId : m_sessionId;

    // Schedule event dispatch on the user facing thread.
    // DispatchEvent is exception safe in order not to cause livelock of failed messages.
    // i.e. if some events cannot be delivered to the user, we do not try to deliver events about failed events...
    auto task = CreateTask([this, weakRecognizers, sessionId, eventType, offset, result, activity{ std::move(activity) }, statusCode, audio]()
    {
        SPX_DBG_TRACE_SCOPE("DispatchEvent task started...", "DispatchEvent task complete!");
        DispatchEvent(weakRecognizers, sessionId, eventType, offset, result, std::move(activity), statusCode, audio);
    }, false);
    m_threadService->ExecuteAsync(std::move(task), ISpxThreadService::Affinity::User);
}

void CSpxAudioStreamSession::OnDetected(ISpxDetectorEngineAdapter* adapter, uint64_t offset, uint64_t duration, double confidence, const std::string& keyword, const DataChunkPtr& audioChunk)
{
    UNUSED(adapter);

    SPX_DBG_TRACE_VERBOSE("[%p] CSpxAudioStreamSession::KeywordDetected/VAD Detected: Keyword/VAD detected!! Starting KwsSingleShot recognition... offset=%" PRIu64 "; duration=%" PRIu64 "; size=%d", (void*)this, offset, duration, audioChunk->size);
    SPX_DBG_ASSERT_WITH_MESSAGE(m_threadService->IsOnServiceThread(), "called on wrong thread, must be thread service managed thread.");

    // Report that we have a keyword candidate
    auto factory = SpxQueryService<ISpxRecoResultFactory>(SpxSharedPtrFromThis<ISpxSession>(this));

    // Check if current VAD is on
    auto usingVAD = GetOr<bool>(g_Detection_ProcessingVAD, false);
    SPX_DBG_TRACE_VERBOSE_IF(usingVAD, "Now with VAD on ...");

    // Keyword verification is enabled via an explicit property or implicitly via dependent properties:
    // * "RemoveKeyword" requires KWV and implies it
    // * VAD needs keyword verification to turn off
    // store the VAD gated offset to m_GatedOffset for adding back to recognition result
    if (usingVAD)
    {
        SetStringValue(KeywordConfig_EnableKeywordVerification, "false");
        m_GatedOffset += offset;
    }
    auto keywordVerificationEnabled = GetOr<bool>(KeywordConfig_EnableKeywordVerification, false)
        || GetOr<bool>("SPEECH-RemoveKeyword", false);
    auto keywordOnly = GetOr<bool>(g_keyword_KeywordOnly, false);
    SPX_DBG_TRACE_VERBOSE_IF(keywordVerificationEnabled, "Now with keywordVerificationEnabled on ...");

    // Keyword verification is part of USP protocol, thus, we have to store the spotted keyword to enable keyword verification.
    // If keyword verification is enabled, fire the keyword result as intermediate result. Otherwise, fire it as intermediate and final result
    // so it's a consistent experience regardless of whether keyword verification is enabled or not.
    std::shared_ptr<ISpxRecognitionResult> spottedKeywordResult = nullptr;

    if (!usingVAD)
    {
        if (audioChunk->size > 0) // multi-keyword reco adapter does not return keyword audio
        {
            m_saveToWavDetectedKeyword.OpenWav("kws-detected-", m_format.get());
            m_saveToWavDetectedKeyword.SaveToWav(audioChunk->data.get(), audioChunk->size);
            m_saveToWavDetectedKeyword.CloseWav();
        }

        spottedKeywordResult = factory->CreateKeywordResult(confidence, offset, duration, keyword.c_str(), ResultReason::RecognizingKeyword, nullptr);
        FireResultEvent(GetSessionId(), spottedKeywordResult);
    }

    if (!keywordVerificationEnabled)
    {
        std::shared_ptr<ISpxAudioDataStream> stream{ nullptr };
        if (keywordOnly)
        {
            auto object = SpxCreateObjectWithSite<ISpxRecoEngineAdapter>("CSpxOutputRecoEngineAdapter", this);
            auto didSetDuration = TryQueryInterface<ISpxAudioProcessorMinInput>(object, [duration](ISpxAudioProcessorMinInput& processor)
            {
                processor.SetMinInputSize(duration);
            });
            if (didSetDuration)
            {
                InvokeOnServiceIfAvailable<ISpxNamedProperties>(ISpxServiceProvider::shared_from_this(), [duration](ISpxNamedProperties& properties)
                {
                    auto durationMs = duration / 10000;
                    auto durationStr = std::to_string(durationMs);
                    properties.SetStringValue("SPEECH-TransmitLengthBeforeThrottleMs", durationStr.c_str());
                });
            }
            m_recoAdapter = object;
            stream = SpxQueryInterface<ISpxAudioDataStream>(object);
        }

        if (!usingVAD)
        {
            spottedKeywordResult = factory->CreateKeywordResult(confidence, offset, duration, keyword.c_str(), ResultReason::RecognizedKeyword, stream);
            FireResultEvent(GetSessionId(), spottedKeywordResult);

            if (m_singleShotInFlight && (m_singleShotInFlight->m_kind == RecognitionKind::DetectionOnce))
            {
                /* This means the single shot was a kws single shot */
                m_singleShotInFlight->m_promise.set_value(spottedKeywordResult);
                m_singleShotInFlight = nullptr;
            }
        }
        else
        {
            if (m_singleShotInFlight && (m_singleShotInFlight->m_kind == RecognitionKind::DetectionOnce))
            {
                m_singleShotInFlight = nullptr;
            }
        }

        // Set spottedKeywordResult to null since KWV is off and there is no need to verify KWS spotted keyword
        spottedKeywordResult = nullptr;
    }

    if (m_recoKind == RecognitionKind::DetectionOnce)
    {
        SPX_DBG_TRACE_VERBOSE("Current Gating is in Single shot mode.");
        if (!m_isMultiKeywordRecognition) // avoid unnecessary multi-keyword adapter re-init on next reco
        {
            m_kwsModel = nullptr;
        }
        SetStringValue(g_Detection_ProcessingVAD, "false");
    }

    if (TryChangeState({ RecognitionKind::Detection }, { SessionState::ProcessingAudio }, RecognitionKind::DetectionSingleShot, SessionState::HotSwapPaused) ||
        TryChangeState({ RecognitionKind::DetectionOnce }, { SessionState::ProcessingAudio }, RecognitionKind::DetectionOnceSingleShot, SessionState::HotSwapPaused))
    {
        // We've been told a keyword was recognized... Stash the audio, and start KwsSingleShot recognition!!
        SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::KeywordDetected: Now KwsSingleShot/Paused ...", (void*)this);

        m_keyword = keyword;
        m_replayBuffer = audioChunk;
        HotSwapToDetectionSingleShotWhilePaused(spottedKeywordResult);
    }
}

void CSpxAudioStreamSession::GetScenarioCount(uint16_t* countSpeech, uint16_t* countTranslation, uint16_t* countDialog, uint16_t* countConversationTranscriber, uint16_t* countConversationTranscriberV2, uint16_t* countMeetingTranscriber, uint16_t* countLanguageId)
{
    unique_lock<mutex> lock(m_recognizersLock);
    if (m_recognizers.empty())
    {
        // we only support 1 recognizer today... but can be deleted if user is killing it right now.
        *countSpeech = *countTranslation = *countDialog =  *countConversationTranscriber = *countMeetingTranscriber = *countLanguageId = 0;
        return;
    }

    SPX_DBG_ASSERT(m_recognizers.size() == 1);
    auto recognizer = m_recognizers.front().lock();
    auto translationRecognizer = SpxQueryInterface<ISpxTranslationRecognizer>(recognizer);
    auto dialogServiceConnector = SpxQueryInterface<ISpxDialogServiceConnector>(recognizer);
    auto conversationTranscriber = SpxQueryInterface<ISpxConversationTranscriber>(recognizer);
    auto conversationTranscriberV2 = SpxQueryInterface<ISpxConversationTranscriberV2>(recognizer);
    auto meetingTranscriber = SpxQueryInterface<ISpxMeetingTranscriber>(recognizer);
    auto sourceLanguageRecognizer = SpxQueryInterface<ISpxSourceLanguageRecognizer>(recognizer);

    *countConversationTranscriberV2 = (conversationTranscriberV2 != nullptr) ? 1 : 0;
    *countConversationTranscriber = (conversationTranscriber != nullptr) ? 1 : 0;
    *countMeetingTranscriber = (meetingTranscriber != nullptr) ? 1 : 0;
    *countDialog = (dialogServiceConnector != nullptr) ? 1 : 0;
    *countTranslation = (translationRecognizer != nullptr) ? 1 : 0;
    *countLanguageId = (sourceLanguageRecognizer != nullptr) ? 1 : 0;
    *countSpeech = 1 - *countTranslation - *countDialog - *countConversationTranscriber - *countConversationTranscriberV2 - *countMeetingTranscriber - *countLanguageId;

    SPX_DBG_TRACE_VERBOSE("%s: countSpeech=%d; countTranslation=%d; countDialog=%d, countConversationTranscriber=%d, countConversationTranscriberV2=%d, countMeetingTranscriber=%d, countLanguageId=%d", __FUNCTION__, *countSpeech, *countTranslation, *countDialog, *countConversationTranscriber, *countConversationTranscriberV2, *countMeetingTranscriber, *countLanguageId);
}

std::list<std::string> CSpxAudioStreamSession::GetListenForList()
{
    unique_lock<mutex> lock(m_recognizersLock);
    SPX_DBG_ASSERT(m_recognizers.size() == 1);
    auto recognizer = m_recognizers.front().lock();
    lock.unlock();
    if (!recognizer)
    {
        SPX_TRACE_ERROR("%s: going to throw recognizer destroyed runtime_error", __FUNCTION__);
        ThrowRuntimeError("GetListenForList: Recognizer is already destroyed, cannot continue.");
    }
    // Get the listen for list from the recognizer(s)
    auto grammarlist = SpxQueryInterface<ISpxGrammar>(recognizer);
    auto listenForList = grammarlist->GetListenForList();

    return listenForList;
}

std::shared_ptr<ISpxRecognitionResult> CSpxAudioStreamSession::GetSpottedKeywordResult()
{
    return m_singleShotInFlight != nullptr ? m_singleShotInFlight->m_spottedKeywordResult : nullptr;
}

void CSpxAudioStreamSession::AdapterStartingTurn(ISpxRecoEngineAdapter* /* adapter */)
{
    SPX_DBG_TRACE_FUNCTION();
    SPX_DBG_ASSERT(!m_adapterStreamingAudio);
    m_adapterStreamingAudio = true;
}

void CSpxAudioStreamSession::AdapterStartedTurn(ISpxRecoEngineAdapter* /* adapter */, const std::string& /* id */, OffsetType /* adapterStartOffset */)
{
    SPX_DBG_TRACE_FUNCTION();
    m_adapterStreamingAudio = true;
}

void CSpxAudioStreamSession::AdapterStoppedTurn(ISpxRecoEngineAdapter* /* adapter */, bool isRestarting)
{
    SPX_DBG_ASSERT(m_adapterStreamingAudio);
    SPX_DBG_TRACE_VERBOSE("CSpxAudioStreamSession::AdapterStoppedTurn: set m_adapterStreamingAudio to false");
    m_adapterStreamingAudio = false;

    uint64_t bufferedBytes = 0;
    uint64_t previousTurnGlobalOffset = m_currentTurnGlobalOffset;
    if (m_audioBuffer)
    {
        if (IsUsingRecoEngineRnnt() && CurrentStateMatches({ RecognitionKind::Continuous }))
        {
            // When m_audioBuffer->NewTurn() is called, it only resets the index
            // of already sent but non-acknowledged audio chunks. If embedded SR
            // has not processed all the chunks it received by now, the next turn
            // in continuous reco would start with re-sending of same chunks and
            // generate duplicate results. Therefore skip calling NewTurn() here.
        }
        else
        {
            m_audioBuffer->NewTurn();
        }
        m_currentTurnGlobalOffset = m_audioBuffer->GetAbsoluteOffset();
        bufferedBytes = m_audioBuffer->StashedSizeInBytes();
    }
    SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::AdapterStoppedTurn: m_currentTurnGlobalOffset=%" PRIu64 ", previousTurnGlobalOffset=%" PRIu64 " bufferedBytes=%" PRIu64, (void*)this, m_currentTurnGlobalOffset, previousTurnGlobalOffset, bufferedBytes);

    auto bIsConversationTranscriber = GetOr<bool>(g_isConversationTranscriber, false);
    auto bIsMeetingTranscriber = GetOr<bool>(g_isMeetingTranscriber, false);

    if (m_sessionState == SessionState::ProcessingAudioLeftovers)
    {
        // In single shot we only have a single result, so simply stop so that the following RecognizeOnce can take place.
        if (m_recoKind != RecognitionKind::Continuous && !isRestarting)
        {
            TryChangeState(SessionState::ProcessingAudioLeftovers, SessionState::WaitForAdapterCompletedSetFormatStop);
            if (!bufferedBytes) // No more data, report to the user.
            {
                EncounteredEndOfStream();
            }
        }
        else // In continuous mode we have to make sure we resend all leftovers.
        {
            bool useRecoEngineRnnt = IsUsingRecoEngineRnnt();

            if (!bufferedBytes || useRecoEngineRnnt ||
                (!isRestarting &&
                    (
                        bIsConversationTranscriber ||
                        bIsMeetingTranscriber ||
                        // Currently the last portion of data (silence) can be non acknowledged by the Bing service,
                        // in order to avoid the live lock, we stop if there was no progress during the last turn.
                        m_currentTurnGlobalOffset == previousTurnGlobalOffset
                    )
                ))
            {
                SPX_TRACE_WARNING_IF(m_currentTurnGlobalOffset == previousTurnGlobalOffset, "[%p]CSpxAudioStreamSession::AdapterStoppedTurn: Dropping %d bytes due to no progress in the last turn", (void*)this, (int)bufferedBytes);
                TryChangeState(SessionState::ProcessingAudioLeftovers, SessionState::WaitForAdapterCompletedSetFormatStop);
                EncounteredEndOfStream();
            }
            else
            {
                while (ProcessNextAudio())
                {
                }
            }
        }
    }

    if (isRestarting)
    {
        // Unmute the adapter when restarting happen right after usp engine turn end (muted adapter), but response to turn end (unmute adapter) cannot be received because adapter has been disconnected.
        SPX_DBG_TRACE_VERBOSE("CSpxAudioStreamSession::AdapterStoppedTurn: set m_adapterAudioMuted to false");
        m_adapterAudioMuted = false;
    }

    if (GetOr<bool>(g_Detection_ProcessingVAD, false) && !isRestarting
        && (GetOr<std::string>("SPEECH-RecoMode", "") == g_recoModeConversation)
        && TryChangeState({ RecognitionKind::DetectionSingleShot }, { SessionState::ProcessingAudio }, RecognitionKind::Detection, SessionState::HotSwapPaused))
    {
        // When use VAD in continuous recognition with recoMode Conversation, sr engine will turn ends, then it needs to do hotswap to VAD adapter again.
        // We've got a vad, we're not trying to stop VAD spotting, and we're done with processing audio and adapter stopped turn.
        // So ... We should hot swap over to the vad spotter (which will stop the current adapter doing whatever its doing)
        // Add this transition here to make sure previous utterance session is done and shrink buffer is finished for previous utterace session before current utterace (session) start processing audio chunks.
        EnsureFireSessionStopped();
        HotSwapAdaptersWhilePaused(RecognitionKind::Detection, m_kwsModel);
    }
    else if (GetOr<bool>(g_Detection_ProcessingVAD, false) && !isRestarting
        && TryChangeState({ RecognitionKind::DetectionSingleShot }, { SessionState::WaitForAdapterCompletedSetFormatStop }, RecognitionKind::Detection, SessionState::HotSwapPaused))
    {
        // We've got a vad, we're not trying to stop VAD spotting, and we're done with processing audio and adapter stopped turn.
        // So ... We should hot swap over to the vad spotter (which will stop the current adapter doing whatever its doing)
        // Add this transition here to make sure previous utterance session is done and shrink buffer is finished for previous utterace session before current utterace (session) start processing audio chunks.
        EnsureFireSessionStopped();
        HotSwapAdaptersWhilePaused(RecognitionKind::Detection, m_kwsModel);
    }
    else if (m_sessionState == SessionState::WaitForAdapterCompletedSetFormatStop)
    {
        // We are waiting for the adapter to confirm it got the SetFormat(nullptr), but we haven't sent the request yet...
        SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::AdapterStoppedTurn: Still WaitForAdapterCompletedSetFormatStop, calling ->SetFormat(nullptr) ...", (void*)this);
        InformAdapterSetFormatStopping(SessionState::WaitForAdapterCompletedSetFormatStop);
    }
    else if (m_adapterAudioMuted
        && m_recoKind == m_turnEndStopKind
        && m_turnEndStopKind == RecognitionKind::Idle
        && m_sessionState == SessionState::ProcessingAudio)
    {
        // The adapter previously requested to stop processing audio ... We can now safely stop recognizing...
        SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::AdapterStoppedTurn: ->StopRecognizing(%d) ...", (void*)this, static_cast<int>(m_turnEndStopKind));
        auto stopKind = m_turnEndStopKind;
        m_turnEndStopKind = RecognitionKind::Idle;
        StopRecognizing(stopKind);
    }
}

bool CSpxAudioStreamSession::IsExpectingAdapterStoppedTurn(ISpxRecoEngineAdapter* /* adapter */)
{
    return m_adapterStreamingAudio;
}

void CSpxAudioStreamSession::AdapterEndOfDictation(ISpxRecoEngineAdapter*, uint64_t /*offset*/, uint64_t /*duration*/)
{
}

void CSpxAudioStreamSession::AdapterDetectedSpeechStart(ISpxRecoEngineAdapter* adapter, uint64_t offset)
{
    UNUSED(adapter);
    if (!m_speechProcessor)
    {
       FireSpeechStartDetectedEvent(offset);
    }
}

void CSpxAudioStreamSession::AdapterDetectedSpeechEnd(ISpxRecoEngineAdapter* adapter, uint64_t originalOffset)
{
    UNUSED(adapter);
    if (!m_speechProcessor)
    {
        FireSpeechEndDetectedEvent(originalOffset);
    }
}

void CSpxAudioStreamSession::AdapterDetectedSoundStart(ISpxRecoEngineAdapter* adapter, uint64_t offset)
{
    UNUSED(adapter);
    UNUSED(offset);
    // TODO: Next: Implement
    // SPX_THROW_HR(SPXERR_NOT_IMPL);
}

void CSpxAudioStreamSession::AdapterDetectedSoundEnd(ISpxRecoEngineAdapter* adapter, uint64_t offset)
{
UNUSED(adapter);
UNUSED(offset);
// TODO: Next: Implement
// SPX_THROW_HR(SPXERR_NOT_IMPL);
}

std::shared_ptr<ISpxSessionEventArgs> CSpxAudioStreamSession::CreateSessionEventArgs(const std::wstring& sessionId)
{
    auto sessionEvent = SpxCreateObjectWithSite<ISpxSessionEventArgs>("CSpxSessionEventArgs", this);

    auto argsInit = SpxQueryInterface<ISpxSessionEventArgsInit>(sessionEvent);
    argsInit->Init(sessionId);

    return sessionEvent;
}

std::shared_ptr<ISpxSessionEventArgs> CSpxAudioStreamSession::CreateTokenRequestEventArgs(const std::wstring& sessionId)
{
    auto sessionEvent = SpxCreateObjectWithSite<ISpxSessionEventArgs>("CSpxTokenReqeustEventArgs", this);

    auto argsInit = SpxQueryInterface<ISpxSessionEventArgsInit>(sessionEvent);
    argsInit->Init(sessionId);

    return sessionEvent;
}

std::shared_ptr<ISpxActivityEventArgs> CSpxAudioStreamSession::CreateActivityEventArgs(std::string activity, std::shared_ptr<ISpxAudioOutput> audio)
{
    auto activityAudioEvent = SpxCreateObjectWithSite<ISpxActivityEventArgs>("CSpxActivityEventArgs", this);

    auto argsInit = SpxQueryInterface<ISpxActivityEventArgsInit>(activityAudioEvent);
    argsInit->Init(activity, audio);

    return activityAudioEvent;
}

std::shared_ptr<ISpxTurnStatusEventArgs> CSpxAudioStreamSession::CreateTurnStatusEventArgs(
    const std::string& interactionId,
    const std::string& conversationId,
    int statusCode)
{
    auto TurnStatusEvent = SpxCreateObjectWithSite<ISpxTurnStatusEventArgs>("CSpxTurnStatusEventArgs", this);
    auto argsInit = SpxQueryInterface<ISpxTurnStatusEventArgsInit>(TurnStatusEvent);
    argsInit->Init(interactionId, conversationId, statusCode);

    return TurnStatusEvent;
}

std::shared_ptr<ISpxConnectionEventArgs> CSpxAudioStreamSession::CreateConnectionEventArgs(const std::wstring& sessionId)
{
    auto connectionEvent = SpxCreateObjectWithSite<ISpxConnectionEventArgs>("CSpxConnectionEventArgs", this);

    auto argsInit = SpxQueryInterface<ISpxConnectionEventArgsInit>(connectionEvent);
    argsInit->Init(sessionId);

    return connectionEvent;
}

std::shared_ptr<ISpxConnectionMessageEventArgs> CSpxAudioStreamSession::CreateConnectionMessageEventArgs(const std::string& headers, const std::string& path, const uint8_t* buffer, uint32_t bufferSize, bool isBufferBinary)
{
    auto message = SpxCreateObjectWithSite<ISpxConnectionMessage>("CSpxConnectionMessage", this);
    auto messageInit = SpxQueryInterface<ISpxConnectionMessageInit>(message);
    messageInit->Init(headers, path, buffer, bufferSize, isBufferBinary);

    auto connectionMessageEvent = SpxCreateObjectWithSite<ISpxConnectionMessageEventArgs>("CSpxConnectionMessageEventArgs", this);
    auto argsInit = SpxQueryInterface<ISpxConnectionMessageEventArgsInit>(connectionMessageEvent);
    argsInit->Init(message);

    return connectionMessageEvent;
}

std::shared_ptr<ISpxRecognitionEventArgs> CSpxAudioStreamSession::CreateRecognitionEventArgs(const std::wstring& sessionId, uint64_t offset)
{
    auto site = SpxSiteFromThis(this);
    auto recoEvent = SpxCreateObjectWithSite<ISpxRecognitionEventArgs>("CSpxRecognitionEventArgs", site);

    auto argsInit = SpxQueryInterface<ISpxRecognitionEventArgsInit>(recoEvent);
    argsInit->Init(sessionId, offset);

    return recoEvent;
}

std::shared_ptr<ISpxRecognitionEventArgs> CSpxAudioStreamSession::CreateRecognitionEventArgs(const std::wstring& sessionId, std::shared_ptr<ISpxRecognitionResult> result)
{
    auto site = SpxSiteFromThis(this);
    auto recoEvent = SpxCreateObjectWithSite<ISpxRecognitionEventArgs>("CSpxRecognitionEventArgs", site);

    auto argsInit = SpxQueryInterface<ISpxRecognitionEventArgsInit>(recoEvent);
    argsInit->Init(sessionId, result);

    return recoEvent;
}

std::shared_ptr<ISpxRecognitionResult> CSpxAudioStreamSession::CreateIntermediateResult(const char* text, uint64_t offset, uint64_t duration, const char* phraseId)
{
    auto result = SpxCreateObjectWithSite<ISpxRecognitionResult>("CSpxRecognitionResult", this);
    auto initResult = SpxQueryInterface<ISpxRecognitionResultInit>(result);
    initResult->InitIntermediateResult(text, offset, duration, phraseId);

    return result;
}

std::shared_ptr<ISpxRecognitionResult> CSpxAudioStreamSession::CreateKeywordResult(const double confidence, const uint64_t offset, const uint64_t duration, const char* keyword, ResultReason reason, std::shared_ptr<ISpxAudioDataStream> stream)
{
    auto result = SpxCreateObjectWithSite<ISpxRecognitionResult>("CSpxRecognitionResult", this);

    auto initResult = SpxQueryInterface<ISpxKeywordRecognitionResultInit>(result);
    initResult->InitKeywordResult(confidence, offset, duration, keyword, reason, stream);

    return result;
}

std::shared_ptr<ISpxRecognitionResult> CSpxAudioStreamSession::CreateFinalResult(ResultReason reason, NoMatchReason noMatchReason, const char* text, uint64_t offset, uint64_t duration, const char* phraseId, const char* userId)
{
    auto result = SpxCreateObjectWithSite<ISpxRecognitionResult>("CSpxRecognitionResult", this);
    auto initResult = SpxQueryInterface<ISpxRecognitionResultInit>(result);
    initResult->InitFinalResult(reason, noMatchReason, text, offset, duration, phraseId);

    // UserId is optional parameter and defaults to nullptr.
    if (userId != nullptr)
    {
        auto initConversationResult = SpxQueryInterface<ISpxConversationTranscriptionResultInit>(result);
        if (initConversationResult)
        {
            initConversationResult->InitConversationResult(userId);
        }
        auto initMeetingResult = SpxQueryInterface<ISpxMeetingTranscriptionResultInit>(result);
        if (initMeetingResult)
        {
            initMeetingResult->InitMeetingResult(userId);
        }
        auto initConversationV2Result = SpxQueryInterface<ISpxConversationTranscriptionV2ResultInit>(result);
        if (initConversationV2Result)
        {
            initConversationV2Result->InitConversationV2Result(userId);
        }
    }

    return result;
}

std::shared_ptr<ISpxRecognitionResult> CSpxAudioStreamSession::CreateErrorResult(const std::shared_ptr<ISpxErrorInformation>& error)
{
    auto result = SpxCreateObjectWithSite<ISpxRecognitionResult>("CSpxRecognitionResult", this);
    auto initResult = SpxQueryInterface<ISpxRecognitionResultInit>(result);
    initResult->InitErrorResult(error);

    return result;
}

std::shared_ptr<ISpxRecognitionResult> CSpxAudioStreamSession::CreateEndOfStreamResult()
{
    auto result = SpxCreateObjectWithSite<ISpxRecognitionResult>("CSpxRecognitionResult", this);
    auto initResult = SpxQueryInterface<ISpxRecognitionResultInit>(result);
    initResult->InitEndOfStreamResult();

    return result;
}

void CSpxAudioStreamSession::FireAdapterResult_KeywordResult(uint64_t offset, std::shared_ptr<ISpxRecognitionResult> result, bool isAccepted)
{
    UNUSED(offset);
    SPX_DBG_ASSERT_WITH_MESSAGE(m_sessionState != SessionState::WaitForPumpSetFormatStart, "ERROR! FireAdapterResult_KeywordResult was called with SessionState==WaitForPumpSetFormatStart");

    auto buffer = m_audioBuffer;

    if (isAccepted) {
        FireResultEvent(GetSessionId(), result);
    }
    else {
        // Drop all current audio since keyword is rejected
        if (buffer) {
            buffer->Drop();
        }

        WaitForRecognition_Complete(result);
    }
}

uint64_t CSpxAudioStreamSession::CalculateLatencyInMs(const ProcessedAudioTimestampPtr& audioTimestamp) const
{
    auto nowTime = system_clock::now();
    if (nowTime < audioTimestamp->chunkReceivedTime)
    {
        SPX_TRACE_ERROR("Unexpected error: result received time (%s) is earlier than audio received time (%s).",
            PAL::GetTimeInString(nowTime).c_str(), PAL::GetTimeInString(audioTimestamp->chunkReceivedTime).c_str());
        return 0;
    }

    uint64_t ticks = PAL::GetTicks(nowTime - audioTimestamp->chunkReceivedTime);
    if (GetOr(PropertyId::AudioConfig_AudioSource, "") == g_audioSourceMicrophone)
    {
        ticks += audioTimestamp->remainingAudioInTicks;
    }

    uint64_t latencyMs = (ticks + 5000) / 10000;
    return latencyMs;
}

uint64_t CSpxAudioStreamSession::CalculateResultLatencyInMs(uint64_t offset, std::shared_ptr<ISpxRecognitionResult> result, bool isFinalResult, bool isFirstHypothesis)
{
    uint64_t latencyMs = 0;
    auto buffer = m_audioBuffer;
    if (buffer)
    {
        auto audioTimestamp = buffer->GetTimestamp(offset + result->GetDuration());
        if (audioTimestamp != nullptr &&
            audioTimestamp->chunkReceivedTime != std::chrono::system_clock::from_time_t(0))
        {
            latencyMs = CalculateLatencyInMs(audioTimestamp);
        }
        else
        {
            SPX_TRACE_ERROR("CalculateResultLatencyInMs:(%s, final=%s, firstHyp=%s): no audio   available.", result->GetResultId().c_str(), isFinalResult ? "true" : "false", isFirstHypothesis ? "true" : "false");
        }
    }
    else
    {
        SPX_TRACE_ERROR("CalculateResultLatencyInMs:(%s, final=%s, firstHyp=%s): audio buffer is empty, cannot get audio timestamp.", result->GetResultId().c_str(), isFinalResult ? "true" : "false", isFirstHypothesis ? "true" : "false");
    }
    return latencyMs;
}

//
// This is targeted for applications that display intermediate and final results on screen, occupying the same screen real-estate.
// If the service falls too far behind real-time, the application may prefer to minimize the churn on the screen. This means not showing.
// new intermediate results if they are latent. And when a latent final recognition results shows up, replacing it with the latest
// intermediate result that is already displayed on screen. Or in other words, targeting applications that have a hard-limit on max latency.
// They do not want to see any events from Carbon with a latency exceeding a threshold. The application is okay with taking the hit of
// missing recognized words or incorrect recognized words, in order to not handle events that come too late.
//
// As of June 7th 2021 we have one very high profile external customer that is likely to use the "RESULT-MaxRecognitionLatencyMs" property.
//
std::shared_ptr<ISpxRecognitionResult> CSpxAudioStreamSession::ApplyLatencyThreshold(uint64_t latencyMs, std::shared_ptr<ISpxRecognitionResult> result, bool isFinalResult)
{
    // Determine the maximum latency allowed
    auto maxLatencyMs = GetOr<uint64_t>("RESULT-MaxRecognitionLatencyMs", 0);
    auto isAboveMaxLatency = maxLatencyMs > 0 && latencyMs > maxLatencyMs;
    auto preferTranslations = GetOr<bool>("RESULT-PreferTranslationResults", false);

    // Does the result exceed the threshold
    auto reason = result->GetReason();
    auto phraseId = result->GetResultId();
    auto isIntermediate = !isFinalResult;
    auto isLatentIntermediate = isAboveMaxLatency && !isFinalResult && (reason == ResultReason::RecognizingSpeech || reason == ResultReason::TranslatingSpeech);
    auto isLatentFinal = isAboveMaxLatency && isFinalResult && (reason == ResultReason::RecognizedSpeech || reason == ResultReason::TranslatedSpeech);
    auto replaceFinal = isFinalResult && preferTranslations && result->GetReason() != ResultReason::TranslatedSpeech;

    bool resultChanged = false;

    // If this latency handling logic is ever to be used again, you may want to enable this log as part of manual testing. Commented out by default.
    // SPX_DBG_TRACE_INFO("ApplyLatencyThreshold: latencyMs = %llu, maxLatencyMs = %llu, reason = %d, isIntermediate = %d, isLatentIntermediate = %d, isFinalResult = %d, isLatentFinal =%d", latencyMs, maxLatencyMs, reason, isIntermediate,  isLatentIntermediate, isFinalResult, isLatentFinal);

    // If the intermediate exceeds the threshold, filter the intermediate, otherwise cache it
    if (isLatentIntermediate)
    {
        SPX_TRACE_WARNING("MAXIMUM LATENCY detected!! %" PRIu64 " ms: Removing intermediate...", latencyMs);
        result = nullptr;
        resultChanged = true;
    }
    else if (isIntermediate) // && !isLatentIntermediate ... cache the intermediate
    {
        SPX_DBG_TRACE_VERBOSE("Storing intermediate result for possible promotion.");
        m_lastLowLatencyIntermediate = result;
    }

    // If the final exceeds the threshold, upgrade the last low latency intermediate, skip the final, or switch the final to a nomatch
    if (isLatentFinal)
    {
        if (m_lastLowLatencyIntermediate != nullptr)
        {
            // If we have a previous low latency intermediate, upgrade that to final
            SPX_TRACE_WARNING("MAXIMUM LATENCY detected!! %" PRIu64 " ms: Upgrading last low latency intermediate", latencyMs);
            auto init = SpxQueryInterface<ISpxRecognitionResultInit>(m_lastLowLatencyIntermediate);

            // If the last good intermediate was a "translating" result, make sure the final is "translated"
            reason = m_lastLowLatencyIntermediate->GetReason() == ResultReason::TranslatingSpeech ? ResultReason::TranslatedSpeech : reason;

            init->InitFinalResult(reason, NO_MATCH_REASON_NONE, m_lastLowLatencyIntermediate->GetText().c_str(), m_lastLowLatencyIntermediate->GetOffset(), m_lastLowLatencyIntermediate->GetDuration(), phraseId.c_str());

            // Replace the provided final, with the upgraded low latency intermediate
            result = m_lastLowLatencyIntermediate;
            m_lastLowLatencyIntermediate = nullptr;
        }
        else if (m_recoKind == RecognitionKind::Continuous) // && m_lastLowLatencyIntermediate == nullptr
        {
            // If we're using continuous recognition and we don't have a low latency intermediate, don't fire a result at all
            SPX_TRACE_WARNING("MAXIMUM LATENCY detected!! %" PRIu64 " ms: Continuous recognition mode, not firing final result...", latencyMs);
            result = nullptr;
        }
        else
        {
            SPX_TRACE_WARNING("MAXIMUM LATENCY detected!! %" PRIu64 " ms: Firing no match result...", latencyMs);
            result = CreateFinalResult(ResultReason::NoMatch, NoMatchReason::NotRecognized, "", result->GetOffset(), result->GetDuration(), phraseId.c_str());
        }

        resultChanged = true;
        EvaluateMaxLatencyReconnect();
    }
    else if (replaceFinal) // && !isLatentFinal ... stop caching the previous low latency intermediate
    {
        auto newFinal = m_lastLowLatencyIntermediate != nullptr ? m_lastLowLatencyIntermediate : result;

        // Overwrite this final with the intermediate.
        SPX_TRACE_WARNING("Final Result was not of the preferred type, changing it.");

        auto init = SpxQueryInterface<ISpxRecognitionResultInit>(newFinal);

        init->InitFinalResult(ResultReason::TranslatedSpeech, NO_MATCH_REASON_NONE, newFinal->GetText().c_str(), newFinal->GetOffset(), newFinal->GetDuration(), phraseId.c_str());

        // Replace the provided final, with the upgraded low latency intermediate
        result = newFinal;

        m_lastLowLatencyIntermediate = nullptr;
    }
    else if (isFinalResult)
    {
        m_lastLowLatencyIntermediate = nullptr;
    }

    if (resultChanged)
    {
        // Note: This log string is used by the manual C# test ContinuousRecognitionWithMaxResultLatency()
        SPX_TRACE_INFO("Exit CSpxAudioStreamSession::ApplyLatencyThreshold with changed result");
    }

    return result;
}

//
// Handles optional flushing of audio buffer and reconnections on latent final recognition results.
//
// As of June 7th 2021 the two string properties "RESULT-MaxRecognitionLatencyForceReconnect" and "RESULT-MaxRecognitionLatencyDropAudio"
// are not being set by any customer (external or internal). Keeping this code around for further evaluation and
// potential use in the future.
//
void CSpxAudioStreamSession::EvaluateMaxLatencyReconnect()
{
    if (GetOr<bool>("RESULT-MaxRecognitionLatencyForceReconnect", false))
    {
        if (GetOr<bool>("RESULT-MaxRecognitionLatencyDropAudio", false))
        {
            auto unAkBytes = m_audioBuffer->NonAcknowledgedSizeInBytes();

            m_audioBuffer->DiscardBytes(unAkBytes);
        }

        auto task = CreateTask([=]() {

                // Update the Offset the USP will use when it reconnects to reflect any dropped audio.
                SetStringValue(g_audioContinuationOffset, std::to_string(m_audioBuffer->GetAbsoluteOffset()).c_str());

                auto error = ErrorInfo::FromRecognitionStatus(RecognitionStatus::Success, "Latency exceeded, forcing reconnect.");
                Error(nullptr, error);
            }, false);

        m_threadService->ExecuteAsync(std::move(task));
    }
}

std::shared_ptr<ISpxRecognitionResult> CSpxAudioStreamSession::UpdateResultLatency(uint64_t offset, std::shared_ptr<ISpxRecognitionResult> result, bool isFinalResult, bool isFirstHypothesis)
{
    // Calculate the latency and write it into the result
    uint64_t latencyMs = CalculateResultLatencyInMs(offset, result, isFinalResult, isFirstHypothesis);
    result->SetLatency(latencyMs);

    // Update the telemetry with the latency information
    WriteTelemetryLatency(latencyMs, isFinalResult, isFirstHypothesis);

    // Apply the threshold and return
    return ApplyLatencyThreshold(latencyMs, result, isFinalResult);
}

void CSpxAudioStreamSession::UpdateResultWithDetectionOffset(std::shared_ptr<ISpxRecognitionResult> result)
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
        uint64_t newOffset = oldOffset + m_GatedOffset;
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
            auto indexesToUpdate = { "Words", "DisplayWords" };

            for_each(indexesToUpdate.begin(), indexesToUpdate.end(), [&item, &valueChanged, this](const char* oneIndex)
                {
                    auto findWords = item.ValueAt(oneIndex);
                    if (findWords.IsArray())
                    {
                        valueChanged = true;
                        auto wordListSize = findWords.ValueCount();
                        for (int wordIndex = 0; wordIndex < wordListSize; wordIndex++)
                        {
                            auto word = findWords[wordIndex];
                            auto findOffset = word.ValueAt("Offset");
                            if (findOffset.IsNumber())
                            {
                                uint64_t wordOffset = findOffset.AsUint<uint64_t>();
                                findWords[wordIndex]["Offset"] = wordOffset + m_GatedOffset;
                            }
                        }
                    }
                });
        }
    }

    if (valueChanged)
    {
        string updatedJsonStr = root.AsJson();
        SPX_DBG_TRACE_VERBOSE("%s: after update: json='%s'", __FUNCTION__, updatedJsonStr.c_str());
        namedProperties->Set(PropertyId::SpeechServiceResponse_JsonResult, updatedJsonStr.c_str());
    }
}

void CSpxAudioStreamSession::FireAdapterResult_Intermediate(uint64_t offset, shared_ptr<ISpxRecognitionResult> result)
{
    SPX_DBG_ASSERT(m_sessionState != SessionState::WaitForPumpSetFormatStart);

    // if it is VAD on gating, update the result offset with gated offset
    if (GetOr<bool>(g_Detection_VadModeOn, false))
    {
        UpdateResultWithDetectionOffset(result);
        offset += m_GatedOffset;
    }

    // Update the result w/ the latency, potentially filtering it out
    result = UpdateResultLatency(offset, result, false, m_expectFirstHypothesis);
    if (result == nullptr)
    {
        return;
    }

    // Fire the intermediate, but first, update state
    m_expectFirstHypothesis = false;
    FireResultEvent(GetSessionId(), result);
}

void CSpxAudioStreamSession::FireAdapterResult_FinalResult(uint64_t offset, shared_ptr<ISpxRecognitionResult> result)
{
    SPX_DBG_ASSERT_WITH_MESSAGE(m_sessionState != SessionState::WaitForPumpSetFormatStart, "ERROR! FireAdapterResult_FinalResult was called with SessionState==WaitForPumpSetFormatStart");

    // if it is VAD on gating, update the result offset with gated offset
    if (GetOr<bool>(g_Detection_VadModeOn, false))
    {
        UpdateResultWithDetectionOffset(result);
        offset += m_GatedOffset;
    }

    auto resultProperties = SpxQueryInterface<ISpxNamedProperties>(result);
    auto silenceTelemetry = resultProperties->GetOr<bool>("CARBON-INTERNAL-Silence_Telemetry", false);
    if (!silenceTelemetry)
    {
        // Update the result w/ the latency, potentially filtering it out
        result = UpdateResultLatency(offset, result, true, false);
        if (result == nullptr)
        {
            return;
        }
    }

    // Fire the final, but first, update state
    m_expectFirstHypothesis = true;
    WaitForRecognition_Complete(result);
}

void CSpxAudioStreamSession::FireAdapterResult_ActivityReceived(std::string activity, std::shared_ptr<ISpxAudioOutput> audio)
{
    FireEvent(
        EventType::ActivityReceivedEvent,
        nullptr,
        const_cast<wchar_t*>(GetSessionId().c_str()),
        0,
        std::move(activity),
        0,
        audio);
}

void CSpxAudioStreamSession::FireAdapterResult_TurnStatusReceived(std::wstring interactionId, std::string conversationId, int status)
{
    FireEvent(
        EventType::TurnStatusEvent,
        nullptr,
        interactionId.c_str(),
        0,
        conversationId.c_str(),
        status,
        nullptr);
}

void CSpxAudioStreamSession::FireAdapterResult_TranslationSynthesis(std::shared_ptr<ISpxRecognitionResult> result)
{
    SPX_DBG_ASSERT(m_sessionState != SessionState::WaitForPumpSetFormatStart);

    FireResultEvent(GetSessionId(), result);
}

void CSpxAudioStreamSession::AdapterRequestingAudioMute(ISpxRecoEngineAdapter* /* adapter */, bool muteAudio)
{
    if (muteAudio && m_adapterStreamingAudio
        && (m_recoKind == RecognitionKind::SingleShot || m_recoKind == RecognitionKind::DetectionSingleShot)
        && (m_sessionState == SessionState::ProcessingAudio || m_sessionState == SessionState::WaitForAdapterCompletedSetFormatStop))
    {
        // The adapter is letting us know that it wants us to stop sending it audio data for this single shot utterance
        SPX_DBG_TRACE_VERBOSE("%s: Muting audio (SingleShot or KwsSingleShot) ... recoKind/sessionState: %d/%d", __FUNCTION__, static_cast<int>(m_recoKind), static_cast<int>(m_sessionState));
        m_turnEndStopKind = m_recoKind;
        m_adapterAudioMuted = true;
    }
    else if (muteAudio
        && m_adapterStreamingAudio
        && CurrentStateMatches({ RecognitionKind::Continuous }, { SessionState::ProcessingAudio, SessionState::WaitForAdapterCompletedSetFormatStop }))
    {
        // The adapter is letting us know that it wants us to stop sending it audio data for this until it requests to restart the audio flow
        SPX_DBG_TRACE_VERBOSE("%s: Muting audio (Continuous) ... recoKind/sessionState: %d/%d", __FUNCTION__, static_cast<int>(m_recoKind), static_cast<int>(m_sessionState));
        m_adapterAudioMuted = true;
    }
    else if (!muteAudio && m_adapterAudioMuted)
    {
        // The adapter is letting us know that its OK to once again send audio data to it
        SPX_DBG_TRACE_VERBOSE("%s: UN-muting audio ... recoKind/sessionState: %d/%d", __FUNCTION__, static_cast<int>(m_recoKind), static_cast<int>(m_sessionState));
        if (!GetOr<bool>(g_Detection_VadModeOn, false) || !(GetOr<std::string>("SPEECH-RecoMode", "") == g_recoModeConversation))
        {
            // When it is in conversation with VAD on mode, we will unmute while HotswapAdapter. So we skip unmute here to avoid unmuting new adapter is not ready
            m_adapterAudioMuted = false;
        }
    }
    else if (m_sessionState == SessionState::ProcessingAudioLeftovers)
    {
        SPX_DBG_TRACE_VERBOSE("%s: Skipping audio mute for last portion of data. recoKind/sessionState: %d/%d", __FUNCTION__, static_cast<int>(m_recoKind), static_cast<int>(m_sessionState));
    }
    else
    {
        SPX_TRACE_ERROR("%s: Is this OK? recoKind/sessionState: %d/%d", __FUNCTION__, static_cast<int>(m_recoKind), static_cast<int>(m_sessionState));
    }
}

void CSpxAudioStreamSession::AdditionalMessage(ISpxRecoEngineAdapter* adapter, uint64_t offset, AdditionalMessagePayload_Type payload)
{
    UNUSED(adapter);
    UNUSED(offset);
    UNUSED(payload);
    // TODO: FUTURE: Implement
    // SPX_THROW_HR(SPXERR_NOT_IMPL);
}

// Called by audio pump thread after exceptions.
void CSpxAudioStreamSession::Error(const std::string& error)
{
    if (!error.empty())
    {
        auto task = CreateTask([=]() { CheckError(error); }, false);
        m_threadService->ExecuteAsync(std::move(task));
    }
}

void CSpxAudioStreamSession::CheckError(const string& errorMessage)
{
    if (!errorMessage.empty())
    {
        auto error = ErrorInfo::FromExplicitError(CancellationErrorCode::RuntimeError, errorMessage);
        Error(nullptr, error);
    }
}

void CSpxAudioStreamSession::Error(ISpxRecoEngineAdapter*, std::shared_ptr<ISpxErrorInformation> payload)
{
    SPX_DBG_TRACE_FUNCTION();

    const auto isDetectionRecoKind = m_recoKind == RecognitionKind::Detection || m_recoKind == RecognitionKind::DetectionSingleShot;

    const auto isDialogServiceConnector = GetOr<bool>(g_isDialogServiceConnector, false);

    // Check for embedded speech errors that are generally non-recoverable.
    const std::string errorMsg = payload->GetDetails();
    bool isEmbeddedSpeechError = errorMsg.find("embedded") != std::string::npos;

    if ((m_recoKind == RecognitionKind::Detection) && !isEmbeddedSpeechError)
    {
        m_adapterResetPending = true;
    }
    else
    {
        // capture this here since it gets cleared by the badly named WaitForRecognition_Complete function
        // which of course does no waiting whatsoever
        // Need to stop the Pump
        if (m_compressedAudioAdapter != nullptr)
        {
            m_compressedAudioAdapter->StopCompressedPump();
        }

        bool isSingleShotInFlight = m_singleShotInFlight != nullptr;

        auto factory = SpxQueryService<ISpxRecoResultFactory>(SpxSharedPtrFromThis<ISpxSession>(this));
        auto errorResult = factory->CreateErrorResult(payload);
        WaitForRecognition_Complete(errorResult);
        EnsureFireSessionStopped();

        // For a DialogServiceConnector we raise a cancellation error but we reset the session so we can keep using it
        m_adapterResetPending |= isDialogServiceConnector;

        // For dialog service connector, we drop the audio, this is the point that we would need to play with if we were to recover the lost turn
        // For keyword recognizer, we drop audio when cancellation with an error happens to prevent extra keyword detections in the stopping phase
        if (m_audioBuffer && (isDialogServiceConnector || isDetectionRecoKind))
        {
            m_audioBuffer->Drop();
        }

        // We're not going to see a start or stop message. We're done. Canceled.
        m_adapterStreamingAudio = false;

        m_canceledOnError = true; // currently used in GetStopRecognitionTimeout()

        // Make sure recognition is stopped in case failure occurred
        if (m_recoKind != RecognitionKind::Idle)
        {
            /*
            * When using recognize once async, and the audio file we are using is very short, it is possible for
            * us to have buffered all the audio before we realize that connection to the service failed to open.
            * In this case, we get stuck in the ProcessingAudioLeftovers state since we will (obviously) never
            * get a turn.end message which results in a call to AdapterStoppedTurn. Right now that is the only
            * function that moves us from the ProcessingAudioLeftovers to any other state
            * Also, tell the adapter that we're done with it.
            */
            if (isSingleShotInFlight)
            {
                TryChangeState(SessionState::ProcessingAudioLeftovers, SessionState::WaitForAdapterCompletedSetFormatStop);
                if (m_audioProcessor) // if the error occurred during adapter init then m_audioProcessor may be nullptr
                {
                    m_audioProcessor->SetFormat(nullptr);
                }
            }

            StopRecognizing(m_recoKind);
        }
    }
}

std::shared_ptr<ISpxSession> CSpxAudioStreamSession::GetDefaultSession()
{
    return SpxSharedPtrFromThis<ISpxSession>(this);
}

std::shared_ptr<ISpxRecoEngineAdapter> CSpxAudioStreamSession::EnsureInitRecoEngineAdapter()
{
    SPX_DBG_TRACE_FUNCTION();

    if (m_recoAdapter == nullptr || m_adapterResetPending)
    {
        SPX_DBG_TRACE_VERBOSE("CSpxAudioStreamSession::EnsureInitRecoEngineAdapter EnsureResetEngineEngineAdapterComplete");
        EnsureResetEngineEngineAdapterComplete();
        InitRecoEngineAdapter();
    }
    return m_recoAdapter;
}

std::shared_ptr<ISpxRecoEngineAdapter> CSpxAudioStreamSession::EnsureInitOutputEngineAdapter()
{
    SPX_DBG_TRACE_FUNCTION();
    /* If we are here, the recoAdapter should have been initialized and passed to the result */
    SPX_DBG_ASSERT(m_recoAdapter != nullptr);
    return m_recoAdapter;
}

std::shared_ptr < ISpxSpeechAudioProcessorAdapter> CSpxAudioStreamSession::EnsureInitSpeechProcessor()
{
    std::shared_ptr < ISpxSpeechAudioProcessorAdapter> retval = SpxCreateObjectWithSite<ISpxSpeechAudioProcessorAdapter>("CSpxSpeechAudioProcessor", this);
    m_speechProcessor = SpxQueryInterface<ISpxAudioProcessor>(retval);
    return retval;
}

void CSpxAudioStreamSession::InitRecoEngineAdapter()
{
    SPX_DBG_TRACE_FUNCTION();

    // determine which type (or types) of reco engine adapters we should try creating...
    bool tryRnnt = IsUsingRecoEngineRnnt();
    bool tryMock = GetOr<bool>("CARBON-INTERNAL-UseRecoEngine-Mock", false);
    bool tryUsp = GetOr<bool>("CARBON-INTERNAL-UseRecoEngine-Usp", false);
    bool tryHybrid = GetOr<std::string>(PropertyId::SpeechServiceConnection_RecoBackend, std::string()) == "hybrid";
    bool tryWebsocket = GetOr<bool>("CARBON-INTERNAL-UseRecoEngine-Websocket", false);

    // if nobody specified which type(s) of reco engine adapters this session should use, we'll use the USP
    if (!tryRnnt && !tryMock && !tryUsp && !tryWebsocket && !tryHybrid)
    {
        tryUsp = true;
    }

    // try to create the RNN-T adapter...
    if (m_recoAdapter == nullptr && tryRnnt)
    {
        m_recoAdapter = SpxCreateObjectWithSite<ISpxRecoEngineAdapter>("CSpxRnntRecoEngineAdapter", this);
    }

    // try to create the Usp adapter...
    if (m_recoAdapter == nullptr && tryUsp)
    {
        m_recoAdapter = SpxCreateObjectWithSite<ISpxRecoEngineAdapter>("CSpxUspRecoEngineAdapterRetry_OffsetFixupWrapper", this);
    }

    // try to create the Hybrid adapter...
    if (m_recoAdapter == nullptr && tryHybrid)
    {
        m_recoAdapter = SpxCreateObjectWithSite<ISpxRecoEngineAdapter>("CSpxHybridRecoEngineAdapter", this);
    }

    // try to create the Websocket adapter...
    if (m_recoAdapter == nullptr && tryWebsocket)
    {
        m_recoAdapter = SpxCreateObjectWithSite<ISpxRecoEngineAdapter>("CSpxCustomCommandsRecoEngineAdapter", this);
    }

    // try to create the mock reco engine adapter...
    if (m_recoAdapter == nullptr && tryMock)
    {
        m_recoAdapter = SpxCreateObjectWithSite<ISpxRecoEngineAdapter>("CSpxMockRecoEngineAdapter", this);
    }

    // if we still don't have an adapter... that's an exception
    if (m_recoAdapter == nullptr)
    {
        if (tryRnnt)
        {
            // Give a bit more descriptive error message instead of just SPXERR_NOT_FOUND.
            ThrowRuntimeError("Could not create the embedded speech adapter. Are all required libraries installed?");
        }
        else
        {
            SPX_THROW_HR(SPXERR_NOT_FOUND);
        }
    }
}

void CSpxAudioStreamSession::StartResetEngineAdapter()
{
    SPX_DBG_TRACE_FUNCTION();

    SPX_DBG_TRACE_VERBOSE("CSpxAudioStreamSession::StartResetEngineAdapter set m_adapterResetPending true");
    m_adapterResetPending = true;
    if (m_recoKind == RecognitionKind::Continuous || m_recoKind == RecognitionKind::SingleShot
        || (GetOr<bool>(g_Detection_VadModeOn, false) && (m_recoKind == RecognitionKind::DetectionOnce || m_recoKind == RecognitionKind::Detection || m_recoKind == RecognitionKind::DetectionSingleShot)))
    {
        // If we were continuous, let's try and restart recognition, same kind...
        StartRecognizing(m_recoKind, nullptr);
    }
    else if (m_recoKind != RecognitionKind::Idle)
    {
        // Otherwise... Let's stop the recognition
        StopRecognizing(m_recoKind);
    }
}

void CSpxAudioStreamSession::EnsureResetEngineEngineAdapterComplete()
{
    SPX_DBG_TRACE_FUNCTION();

    if (m_adapterResetPending)
    {
        // Let's term and clear our reco adapter...
        SPX_DBG_TRACE_VERBOSE("%s: resetting reco adapter (0x%8p)...", __FUNCTION__, (void*)m_recoAdapter.get());
        SpxTermAndClear(m_recoAdapter);

        SPX_DBG_TRACE_VERBOSE("CSpxAudioStreamSession::EnsureResetEngineEngineAdapterComplete set m_adapterStreamingAudio to false");
        m_adapterStreamingAudio = false;
        m_adapterAudioMuted = false;

        m_recoAdapter = nullptr; // Already termed (see above)
        m_adapterResetPending = false;
        m_sessionStarted = false;
        m_sessionStopped = false;
    }
}

std::shared_ptr<ISpxDetectorEngineAdapter> CSpxAudioStreamSession::EnsureInitDetectionEngineAdapter(std::shared_ptr<ISpxKwsModel> model)
{
    SPX_DBG_TRACE_FUNCTION();

    if (GetOr<bool>(g_Detection_VadModeOn, false))
    {
        // If recognition start with VAD mode (g_Detection_VadModeOn), then mark VAD processing (g_Detection_ProcessingVAD) start.
        // g_Detection_ProcessingVAD turn to false when VAD gating is done.
        SetStringValue(g_Detection_ProcessingVAD, "true");
    }

    if (m_detectionAdapter == nullptr || model != m_kwsModel)
    {
        SpxTermAndClear(m_detectionAdapter);
        InitDetectionEngineAdapter(model);
    }

    return m_detectionAdapter;
}

std::shared_ptr<ISpxRecoEngineAdapter> CSpxAudioStreamSession::EnsureInitMultiKeywordRecoAdapter(std::shared_ptr<ISpxKwsModel> model)
{
    SPX_DBG_TRACE_FUNCTION();

    // Only (re-)init if the adapter is uninitialized or the model has changed.
    if (m_multiKeywordRecoAdapter == nullptr || model != m_kwsModel)
    {
        SpxTermAndClear(m_multiKeywordRecoAdapter);

        m_kwsModel = model;

        // Initialize the embedded speech adapter for keyword recognition.
        SetStringValue("CARBON-INTERNAL-InitMultiKeywordRecoAdapter", "true");
        m_multiKeywordRecoAdapter = SpxCreateObjectWithSite<ISpxRecoEngineAdapter>("CSpxRnntRecoEngineAdapter", this);
        SetStringValue("CARBON-INTERNAL-InitMultiKeywordRecoAdapter", "");

        if (m_multiKeywordRecoAdapter == nullptr)
        {
            ThrowRuntimeError("Could not create the embedded speech adapter for keyword recognition. Are all required libraries installed?");
        }
    }

    return m_multiKeywordRecoAdapter;
}

void CSpxAudioStreamSession::InitDetectionEngineAdapter(std::shared_ptr<ISpxKwsModel> model)
{
    SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::InitKwsEngineAdapter ...", (void*)this);

    m_kwsModel = model;

    bool tryMock = GetOr<bool>("CARBON-INTERNAL-UseKwsEngine-Mock", false);
    bool tryDdk = GetOr<bool>("CARBON-INTERNAL-UseKwsEngine-Ddk", false);
    bool trySdk = GetOr<bool>("CARBON-INTERNAL-UseKwsEngine-Sdk", false);
    bool tryVad = (GetOr<bool>("CARBON-INTERNAL-UseVadEngine-Sdk", false) && (model == nullptr));
    bool tryMockVad = (GetOr<bool>("CARBON-INTERNAL-UseVadEngine-Mock", false) && (model == nullptr));
    bool vadMode = (GetOr<bool>(g_Detection_VadModeOn, false) && (model == nullptr));

    // if it is a vad mode on but not specified mock or not, we will use the vad engine.
    if (vadMode && !tryVad && !tryMockVad)
    {
        tryVad = true;
    }
    // if nobody specified which type(s) of reco engine adapters this session should use, we'll use the SDK KWS engine
    else if (!tryMock && !trySdk && !tryDdk)
    {
        trySdk = true;
        tryDdk = true;
        tryMock = true;
    }

    // try to create the DDK adapter...
    if (m_detectionAdapter == nullptr && tryDdk)
    {
        m_detectionAdapter = SpxCreateObjectWithSite<ISpxDetectorEngineAdapter>("CSpxSpeechDdkKwsEngineAdapter", this);
    }

    // try to create the vad adapter
    if (m_detectionAdapter == nullptr && tryVad)
    {
        m_detectionAdapter = SpxCreateObjectWithSite<ISpxDetectorEngineAdapter>("CSpxSdkVadEngineAdapter", this);
    }

    // try to create the mock vad adapter
    if (m_detectionAdapter == nullptr && tryMockVad)
    {
        m_detectionAdapter = SpxCreateObjectWithSite<ISpxDetectorEngineAdapter>("CSpxMockVadEngineAdapter", this);
    }

    // try to create the SDK adapter...
    if (m_detectionAdapter == nullptr && trySdk)
    {
        m_detectionAdapter = SpxCreateObjectWithSite<ISpxDetectorEngineAdapter>("CSpxSdkKwsEngineAdapter", this);
    }

    // try to create the mock adapter...
    if (m_detectionAdapter == nullptr && tryMock)
    {
        m_detectionAdapter = SpxCreateObjectWithSite<ISpxDetectorEngineAdapter>("CSpxMockKwsEngineAdapter", this);
    }

    // if we still don't have an adapter... that's an exception
    SPX_THROW_HR_IF(SPXERR_EXTENSION_LIBRARY_NOT_FOUND, m_detectionAdapter == nullptr);
}

void CSpxAudioStreamSession::HotSwapToDetectionSingleShotWhilePaused(std::shared_ptr<ISpxRecognitionResult> spottedKeywordResult)
{
    SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::HotSwapToKwsSingleShotWhilePaused ...", (void*)this);

    // We need to do all this work on a background thread, because we can't guarantee it's safe
    // to spend any significant amount of time blocking this the KWS or Audio threads...
    auto task = CreateTask([=]() mutable {

        SPX_DBG_TRACE_SCOPE("*** CSpxAudioStreamSession::HotSwapToKwsSingleShotWhilePaused kicked-off THREAD started ***", "*** CSpxAudioStreamSession::HotSwapToKwsSingleShotWhilePaused kicked-off THREAD stopped ***");
        SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::HotSwapToKwsSingleShotWhilePaused:  Task", (void*)this);

        // Keep track of the fact that we have a thread pending waiting to hear
        // what the final recognition result is, and then start/stop recognizing...
        if (m_singleShotInFlight != nullptr)
        {
            // There is another single shot in flight. Report an error.
            // TODO: Should queue in the future.
            SPX_THROW_HR(SPXERR_START_RECOGNIZING_INVALID_STATE_TRANSITION);
        }

        auto singleShotInFlight = make_shared<Operation>(m_recoKind);
        singleShotInFlight->m_spottedKeywordResult = spottedKeywordResult;
        m_singleShotInFlight = singleShotInFlight;

        SPX_DBG_ASSERT(CurrentStateMatches({ RecognitionKind::DetectionSingleShot, RecognitionKind::DetectionOnceSingleShot }, { SessionState::HotSwapPaused }));
        this->HotSwapAdaptersWhilePaused(m_recoKind);

        SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::HotSwapToKwsSingleShotWhilePaused - Task: m_audioPumpStoppedBeforeHotSwap %s", (void*)this, m_audioPumpStoppedBeforeHotSwap ? "true": "false");
        if (m_audioPumpStoppedBeforeHotSwap == true)
        {
            m_audioPumpStoppedBeforeHotSwap = false;

            // Process the buffered audio leftovers
            SPX_DBG_ASSERT(m_sessionState == SessionState::ProcessingAudio);
            SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::HotSwapToKwsSingleShotWhilePaused:  Task - Processing audio leftovers after hot swap", (void*)this);

            TryChangeState(SessionState::ProcessingAudio, SessionState::ProcessingAudioLeftovers);
            while (ProcessNextAudio())
            {
            }
            TryChangeState(SessionState::ProcessingAudioLeftovers, SessionState::WaitForAdapterCompletedSetFormatStop);
            InformAdapterSetFormatStopping(SessionState::ProcessingAudio);
            EncounteredEndOfStream();
        }

        auto cancelTimer = CreateTask([=]()
        {
            auto status = singleShotInFlight->m_future.wait_for(0ms);
            if (status != future_status::ready &&
                m_singleShotInFlight &&
                m_singleShotInFlight->m_operationId == singleShotInFlight->m_operationId)
            {
                EnsureFireResultEvent();
            }
        });

        m_threadService->ExecuteAsync(std::move(cancelTimer), Operation::Timeout);
    });

    m_threadService->ExecuteAsync(std::move(task));
}

void CSpxAudioStreamSession::Ensure16kHzSampleRate()
{
    SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::Ensure16kHzSampleRate:  Pump: %p", (void*)this, (void*)m_audioShim.get());

    if (m_audioShim)
    {
        auto waveFormat = m_audioShim->GetFormat();

        // we only support 16kHz sampling rate for Keyword spot for now.
        if (waveFormat->nSamplesPerSec != SAMPLES_PER_SECOND)
        {
            SPX_TRACE_ERROR("going to throw wrong sampling rate runtime_error");
            ThrowRuntimeError("Sampling rate '" + std::to_string(waveFormat->nSamplesPerSec) + "' is not supported. 16kHz is the only sampling rate that is supported.");
        }
    }
}

void CSpxAudioStreamSession::StartAudioPump(RecognitionKind startKind, std::shared_ptr<ISpxKwsModel> model)
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::StartAudioPump:  RecognitionKind %d | m_audioPump [%p]", (void*)this, (int)startKind, (void*)m_audioShim.get());
    SPX_DBG_ASSERT(m_sessionState == SessionState::WaitForPumpSetFormatStart);

    // Tell everyone we're starting if we're not waiting for a keyword
    if (startKind != RecognitionKind::Detection && startKind != RecognitionKind::DetectionOnce)
    {
        EnsureFireSessionStarted();
    }
    else
    {
        // Disable the fast lane for KWS.
        auto properties = SpxQueryService<ISpxNamedProperties>(GetSite());

        // It was spelled wrong originally. Check both until Carbon 2.0.
        if (!properties->HasStringValue("SPEECH-TransmitLengthBeforThrottleMs") && !properties->HasStringValue("SPEECH-TransmitLengthBeforeThrottleMs"))
        {
            properties->SetStringValue("SPEECH-TransmitLengthBeforeThrottleMs", "0");
        }
    }

    if (m_compressedAudioAdapter == nullptr)
    {
        if (!m_audioShim)
        {
            // The session was terminated, exiting.
            SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::StartAudioPump: Audio pump was shutdown, exiting...", (void*)this);
            return;
        }

        if (!m_audioBuffer)
        {
            auto format = m_audioShim->GetFormat();
            SetThrottleVariables(format.get());

            m_audioBuffer = std::make_shared<PcmAudioBuffer>(*format);
        }
        m_audioBuffer->NewTurn();
        m_currentTurnGlobalOffset = m_audioBuffer->GetAbsoluteOffset();
    }

    // Depending on the startKind, we'll either switch to the Kws/VAD Engine Adapter or the Reco Engine Adapter
    if (startKind == RecognitionKind::Detection || startKind == RecognitionKind::DetectionOnce)
    {
        if (!GetOr<bool>(g_Detection_VadModeOn, false))
        {
            SetStringValue(g_keyword_KeywordAndSpeech, "true");
        }
        if (m_isMultiKeywordRecognition)
        {
            m_audioProcessor = SpxQueryInterface<ISpxAudioProcessor>(EnsureInitMultiKeywordRecoAdapter(model));
        }
        else
        {
            m_audioProcessor = SpxQueryInterface<ISpxAudioProcessor>(EnsureInitDetectionEngineAdapter(model));
        }
    }
    else
    {
        SetStringValue(g_keyword_KeywordAndSpeech, "false");
        m_audioProcessor = SpxQueryInterface<ISpxAudioProcessor>(EnsureInitRecoEngineAdapter());
    }
    m_detectionProcessorMode = DetectionProcessorModeFromKind(startKind);

    // Should be conditional based on a property
    if (GetOr<bool>("UseLocalSpeechDetection", false))
    {
        uint32_t energy = GetOr<uint32_t>("LocalSpeechDetectionThreshold", 180);
        uint32_t silence = GetOr<uint32_t>("LocalSpeechDetectionSilenceMs", 600);
        uint32_t skip = GetOr<uint32_t>("LocalSpeechDetectionSkipMs", 200);
        uint32_t base = GetOr<uint32_t>("LocalSpeechDetectionBaseMs", 300);

        std::shared_ptr<ISpxSpeechAudioProcessorAdapter> speechAdapter = EnsureInitSpeechProcessor();
        if (speechAdapter)
        {
            speechAdapter->SetSpeechDetectionThreshold(energy);
            speechAdapter->SetSpeechDetectionSilenceMs(silence);
            speechAdapter->SetSpeechDetectionSkipMs(skip);
            speechAdapter->SetSpeechDetectionBaselineMs(base);
        }
    }

    // Start pumping audio data from the pump, to the Audio Stream Session
    auto ptr = (ISpxAudioProcessor*)this;
    auto pISpxAudioProcessor = ptr->shared_from_this();

    // auto audioPump = m_audioPump;
    SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::StartAudioPump: Starting pump[%p]...", (void*)this, (void*)m_audioShim.get());

    if (m_compressedAudioAdapter == nullptr)
    {
        InvokeMemberIfNotNull(m_audioShim, &ISpxAudioSessionShim::StartAudio);
        // The call to StartPump (immediately above) will initiate calls from the pump to this::SetFormat() and then this::ProcessAudio()...
    }
    else
    {
        m_compressedAudioAdapter->StartCompressedPump(pISpxAudioProcessor);
    }
}

void CSpxAudioStreamSession::HotSwapAdaptersWhilePaused(RecognitionKind startKind, std::shared_ptr<ISpxKwsModel> model)
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::HotSwapAdaptersWhilePaused: ...", (void*)this);
    SPX_DBG_ASSERT(m_recoKind == startKind && m_sessionState == SessionState::HotSwapPaused);

    auto oldAudioProcessor = m_audioProcessor;
    if (!m_audioShim)
    {
        // The session was terminated, exiting.
        SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::HotSwapAdaptersWhilePaused: Audio pump was shutdown, exiting...", (void*)this);
        return;
    }

    // Get the format in which the pump is currently sending audio data to the Session
    auto waveformat = m_audioShim->GetFormat();

    auto keywordOnly = GetOr<bool>(g_keyword_KeywordOnly, false);

    // Depending on the startKind, we'll either switch to the Kws Engine Adapter or the Reco Engine Adapter
    if (startKind == RecognitionKind::Detection || startKind == RecognitionKind::DetectionOnce)
    {
        if (m_isMultiKeywordRecognition)
        {
            m_audioProcessor = SpxQueryInterface<ISpxAudioProcessor>(EnsureInitMultiKeywordRecoAdapter(model));
        }
        else
        {
            m_audioProcessor = SpxQueryInterface<ISpxAudioProcessor>(EnsureInitDetectionEngineAdapter(model));
        }
        m_detectionProcessorMode = DetectionProcessorModeFromKind(startKind);
    }
    else
    {
        if (m_recoKind == RecognitionKind::DetectionOnceSingleShot && keywordOnly)
        {
            m_audioProcessor = SpxQueryInterface<ISpxAudioProcessor>(EnsureInitOutputEngineAdapter());
        }
        else
        {
            m_audioProcessor = SpxQueryInterface<ISpxAudioProcessor>(EnsureInitRecoEngineAdapter());
        }
        m_detectionProcessorMode = DetectionProcessorMode::None;
        if (m_replayBuffer)
        {
            // Switching from KWS/VAD to single shot, creating buffer from scratch with added replay audio.
            SPX_DBG_TRACE_VERBOSE_IF(1, "%s: Size of spotted keyword/vad replay audio %d", __FUNCTION__, m_replayBuffer->size);
            auto currentBuffer = m_audioBuffer;
            m_audioBuffer = std::make_shared<PcmAudioBuffer>(*waveformat);

            if (GetOr<bool>(g_Detection_VadModeOn, false))
            {
                // For VAD continuous recognition, creating a new buffer here will set buffer start offset to 0. So add back the previous utterance's m_bufferStartOffsetInBytesAbsolute
                // which will be used by sr adapter to calculate shrink buffer size (accumulation value) when one utterance is done.
                m_audioBuffer->SetCurrentOffset(currentBuffer->GetCurrentOffset());
            }

            if (m_replayBuffer->size > 0)
            {
                if (m_replayBuffer->size <= MAX_REPLAY_BUFFER_SIZE_FOR_DETECTION_BYTE)
                {
                    m_audioBuffer->Add(m_replayBuffer);
                }
                else
                {
                    // if replay buffer is larger than 65536, break it into smaller chunks before add to session buffer.
                    // this is to avoid KWS/VAD adapter pass in a buffer larger than 65536 which will cause USP websocket message fail.
                    auto replayBufferLeft = m_replayBuffer->size;
                    uint32_t replayBufferAdded = 0;
                    uint32_t replaySubDataSize = MAX_REPLAY_BUFFER_SIZE_FOR_DETECTION_BYTE;
                    while (replayBufferLeft > MAX_REPLAY_BUFFER_SIZE_FOR_DETECTION_BYTE)
                    {
                        auto replaySubData = SpxAllocSharedAudioBuffer(replaySubDataSize);
                        memcpy(&replaySubData.get()[0], &m_replayBuffer->data.get()[replayBufferAdded], replaySubDataSize);
                        auto replaySubChunk = std::make_shared<DataChunk>(replaySubData, replaySubDataSize, m_replayBuffer->receivedTime);
                        m_audioBuffer->Add(replaySubChunk);
                        replayBufferAdded += replaySubDataSize;
                        replayBufferLeft -= replaySubDataSize;
                        replaySubData = nullptr;
                    }
                    if (replayBufferLeft > 0)
                    {
                        replaySubDataSize = replayBufferLeft;
                        auto replaySubData = SpxAllocSharedAudioBuffer(replaySubDataSize);
                        memcpy(&replaySubData.get()[0], &m_replayBuffer->data.get()[replayBufferAdded], replaySubDataSize);
                        auto replaySubChunk = std::make_shared<DataChunk>(replaySubData, replaySubDataSize, m_replayBuffer->receivedTime);
                        m_audioBuffer->Add(replaySubChunk);
                        replayBufferAdded += replaySubDataSize;
                        replayBufferLeft -= replaySubDataSize;
                        replaySubData = nullptr;
                    }
                }
            }
            // In multi-keyword recognition a result from the adapter does
            // not include keyword audio. Instead, the processed audio is
            // discarded normally up until the start of a recognized wake
            // word. For the next audio processor after keyword recognition,
            // unacknowledged audio in m_audioBuffer then begins with this
            // keyword audio.

            currentBuffer->CopyNonAcknowledgedDataTo(m_audioBuffer);
            m_replayBuffer = nullptr;
        }
    }

    // Tell the old Audio Processor that we've sent the last bit of audio data we're going to send
    SPX_DBG_TRACE_VERBOSE_IF(1, "%s: ProcessingAudio - size=%d", __FUNCTION__, 0);
    if (oldAudioProcessor)
    {
        oldAudioProcessor->ProcessAudio(std::make_shared<DataChunk>(nullptr, 0));
        // Then tell it we're finally done, by sending a nullptr SPXWAVEFORMAT
        oldAudioProcessor->SetFormat(nullptr);
    }

    if (m_audioBuffer)
    {
        m_audioBuffer->NewTurn();
        m_currentTurnGlobalOffset = m_audioBuffer->GetAbsoluteOffset();
    }

    // Inform the Audio Processor that we're starting...
    InformAdapterSetFormatStarting(waveformat.get());

    // Unmute audio after the current adapter set format is done. Otherwise, it may starts when adapter is not ready
    SPX_DBG_TRACE_VERBOSE_IF(1, "%s: Unmute audio after set format.", __FUNCTION__);
    m_adapterAudioMuted = false;

    // The call to ProcessAudio(nullptr, 0) and SetFormat(nullptr) will instigate a call from that Adapter to this::AdapterCompletedSetFormatStop(adapter) shortly...
}

void CSpxAudioStreamSession::InformAdapterSetFormatStarting(const SPXWAVEFORMATEX* format)
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);

    auto sizeOfFormat = sizeof(SPXWAVEFORMATEX) + format->cbSize;
    {
        unique_lock<mutex> formatLock(m_formatMutex);
        m_format = SpxAllocWAVEFORMATEX(sizeOfFormat);
        memcpy(m_format.get(), format, sizeOfFormat);
    }

    auto recoAdapter = m_recoAdapter;
    if (recoAdapter != nullptr)
    {
        // In continuous reco kind and VAD continuous recognition with Conversation reco mode, recoAdapter mode is 0 (continuous). Otherwise, 1 (singleshot).
        recoAdapter->SetAdapterMode(m_recoKind != RecognitionKind::Continuous
            && (!GetOr<bool>(g_Detection_VadModeOn, false) || GetOr<std::string>("SPEECH-RecoMode", "") != g_recoModeConversation));

        // For proper segmentation with keyword in offline SR
        recoAdapter->SetKeyword(m_keyword);
        m_keyword.clear();
    }

    SPX_DBG_ASSERT(format != nullptr);

    if (m_audioProcessor)
    {
        m_audioProcessor->SetFormat(format);
    }

    if (m_speechProcessor)
    {
        m_speechProcessor->SetFormat(format);
    }
}

void CSpxAudioStreamSession::SetThrottleVariables(const SPXWAVEFORMATEX* format)
{
    m_throttleLogic = std::make_unique<AudioStreamSessionThrottleLogic>(this, format->nAvgBytesPerSec, IsUsingRecoEngineRnnt(), GetOr<bool>(g_Detection_VadModeOn, false));

    // While this could be set once, if logging is enabled in a retail build the constructor isn't logged.
    if (std::ratio_greater<std::chrono::steady_clock::period, std::milli>::value)
    {
        SPX_DBG_TRACE_VERBOSE("%s - steady_clock resolution (%e) insufficient, falling back to duration based throttling.", __FUNCTION__,
            static_cast<double>(std::chrono::steady_clock::period::num) / static_cast<double>(std::chrono::steady_clock::period::den));
        m_useDurationBasedThrottle = true;
    }
}

void CSpxAudioStreamSession::InformAdapterSetFormatStopping(SessionState comingFromState)
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);

    // If we transitioned --> WaitForAdapterCompletedSetFormatStop state because this Session was trying to stop the pump...
    // We need to tell the audio processor that we've sent the last bit of audio we're planning on sending
    SPX_DBG_ASSERT(comingFromState == SessionState::ProcessingAudio ||
                   comingFromState == SessionState::StoppingPump ||
                   comingFromState == SessionState::WaitForAdapterCompletedSetFormatStop);
    if (comingFromState == SessionState::StoppingPump)
    {
        if (m_audioProcessor)
        {
            SPX_TRACE_INFO("[%p]CSpxAudioStreamSession::InformAdapterSetFormatStoppingProcessingAudio - Send zero size audio, processor=%p", (void*)this, (void*)m_audioProcessor.get());
            m_audioProcessor->ProcessAudio(std::make_shared<DataChunk>(nullptr, 0));
        }
    }

    SPX_DBG_TRACE_VERBOSE("CSpxAudioStreamSession::InformAdapterSetFormatStopping m_adapterStreamingAudio: %s", m_adapterStreamingAudio ? "true" : "false");

    if (!m_adapterStreamingAudio)
    {
        // Then we can finally tell it we're done, by sending a nullptr SPXWAVEFORMAT
        SPX_TRACE_INFO("[%p]CSpxAudioStreamSession::InformAdapterSetFormatStoppingSetFormat(nullptr)", (void*)this);
        m_saveToWavEverything.CloseWav();
        if (m_audioProcessor)
        {
            m_audioProcessor->SetFormat(nullptr);
        }

        if (m_speechProcessor)
        {
            m_speechProcessor->SetFormat(nullptr);
        }

        m_adapterAudioMuted = false;
    }
}

void CSpxAudioStreamSession::EncounteredEndOfStream()
{
    SPX_DBG_TRACE_FUNCTION();

    m_sawEndOfStream = true;
    if (CurrentStateMatches({ RecognitionKind::Continuous, RecognitionKind::Detection, RecognitionKind::DetectionOnce})
        || (GetOr<bool>(g_Detection_VadModeOn, false) && CurrentStateMatches({ RecognitionKind::DetectionSingleShot })))
    {
        // VAD only fire end of stream at session stop in singleshot mode, continuous recognition fires when stop recognition.
        m_fireEndOfStreamAtSessionStop = true;
    }
}

void CSpxAudioStreamSession::AdapterCompletedSetFormatStop(AdapterDoneProcessingAudio doneAdapter)
{
    SPX_DBG_TRACE_SCOPE("*** CSpxAudioStreamSession::AdapterCompletedSetFormatStop kicked-off THREAD started ***", "*** CSpxAudioStreamSession::AdapterCompletedSetFormatStop kicked-off THREAD stopped ***");
    SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::AdapterCompletedSetFormatStop: kicked-off THREAD started ***", (void*)this);

    if (!GetOr<bool>(g_Detection_VadModeOn, false) && TryChangeState({ RecognitionKind::DetectionSingleShot }, { SessionState::WaitForAdapterCompletedSetFormatStop }, RecognitionKind::Detection, SessionState::ProcessingAudio))
    {
        // For KWS single shot in continuous recogntion, when stop recognition happen, switch back to keyword/processingAudio and wait for done.
        // Exclude VAD here, handle in next else if.
        SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::AdapterCompletedSetFormatStop: DetectionSingleShot Waiting for done ... Done!! Switching back to Detection/Processing", (void*)this);
        SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::AdapterCompletedSetFormatStop: Now Detection/ProcessingAudio ...", (void*)this);

        EnsureFireSessionStopped();
    }
    else if (TryChangeState({ RecognitionKind::DetectionOnceSingleShot }, { SessionState::WaitForAdapterCompletedSetFormatStop }, RecognitionKind::Idle, SessionState::Idle))
    {
        // VAD single shot with DetectionOnceSingleShot mode switch to idle directly.
        SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::AdapterCompletedSetFormatStop: DetectionOnceSingleShot Waiting for done ... Done!! Going back to idle", (void*)this);
        SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::AdapterCompletedSetFormatStop: Now Idle/Idle ...", (void*)this);

        EnsureFireSessionStopped();
    }
    else if (TryChangeState(SessionState::HotSwapPaused, SessionState::ProcessingAudio))
    {
        SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::AdapterCompletedSetFormatStop: Previous Adapter is done processing audio ... resuming Processing with the new adapter...", (void*)this);
        SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::AdapterCompletedSetFormatStop: Now ProcessingAudio ...", (void*)this);

        // KWS recognize once does not need to fire session start, it only needs to fire the recognized keyword.
        // VAD needs to return the actual recognized text result, so we need to fire session start here.
        if (doneAdapter == AdapterDoneProcessingAudio::Detection
            && (m_recoKind == RecognitionKind::DetectionSingleShot
                || m_recoKind == RecognitionKind::SingleShot
                || (GetOr<bool>(g_Detection_VadModeOn, false) && m_recoKind == RecognitionKind::DetectionOnceSingleShot)))
        {
            EnsureFireSessionStarted();
        }
    }
    // Because next RecognizeAsync will be scheduled on the same thread, we can firstly set the state to idle and then fire session stop event.
    else if (TryChangeState(SessionState::WaitForAdapterCompletedSetFormatStop, RecognitionKind::Idle, SessionState::Idle))
    {
        if (doneAdapter == AdapterDoneProcessingAudio::Speech)
        {
            // The Reco Engine adapter request to finish processing audio has completed, that signifies that the "session" has stopped
            EnsureFireSessionStopped();

            // Restart the keyword spotter if necessary...
            // VAD does not need to start again here. It only rearm when session start
            if (m_kwsModel != nullptr && TryChangeState(SessionState::Idle, RecognitionKind::Detection, SessionState::WaitForPumpSetFormatStart))
            {
                // Go ahead re-start the pump
                SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::AdapterCompletedSetFormatStop: Restart the keyword spotter - Now WaitForPumpSetFormatStart ...", (void*)this);

                StartAudioPump(RecognitionKind::Detection, m_kwsModel);
            }
        }
        else if (doneAdapter == AdapterDoneProcessingAudio::Detection)
        {
            auto factory = SpxQueryService<ISpxRecoResultFactory>(SpxSharedPtrFromThis<ISpxSession>(this));
            auto result = factory->CreateEndOfStreamResult();
            WaitForRecognition_Complete(result);
        }
    }
    else
    {
        SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::AdapterCompletedSetFormatStop:  Is this OK? doneAdapter=%d; sessionState=%d **************************", (void*)this, doneAdapter, static_cast<int>(m_sessionState));
    }
}

void CSpxAudioStreamSession::SpeechStartDetected(uint64_t offset)
{
    FireSpeechStartDetectedEvent(offset);
}

void CSpxAudioStreamSession::SpeechEndDetected(uint64_t offset)
{
    FireSpeechEndDetectedEvent(offset);
}

bool CSpxAudioStreamSession::TryChangeState(
    std::initializer_list<RecognitionKind> validOriginKinds,
    std::initializer_list<SessionState> validOriginStates,
    RecognitionKind targetKind,
    SessionState targetState)
{
    // Check to see if the Audio StreamState and RecognitionKind match...
    if (std::any_of(validOriginKinds.begin(), validOriginKinds.end(), [this](const RecognitionKind validOriginKind) { return m_recoKind == validOriginKind; })
        && std::any_of(validOriginStates.begin(), validOriginStates.end(), [this](const SessionState validOriginState) { return m_sessionState == validOriginState; }))
    {
        SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::TryChangeState: recoKind/sessionState: %d/%d => %d/%d", (void*)this, static_cast<int>(m_recoKind), static_cast<int>(m_sessionState), static_cast<int>(targetKind), static_cast<int>(targetState));

        // Protecting only the write access because m_cv is waiting,
        // all changes to the state can still happen only on the background thread.
        unique_lock<mutex> lock(m_stateMutex);
        m_sessionState = targetState;
        m_recoKind = targetKind;
        m_cv.notify_all();
        return true;
    }
    SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::TryChangeState:  recoKind/sessionState: %d/%d doesn't match", (void*)this, static_cast<int>(m_recoKind), static_cast<int>(m_sessionState));

    return false;
}

std::shared_ptr<ISpxNamedProperties> CSpxAudioStreamSession::GetParentProperties() const
{
    return SpxQueryService<ISpxNamedProperties>(GetSite());
}

void CSpxAudioStreamSession::WriteTelemetryLatency(uint64_t latencyInTicks, bool isPhraseLatency, bool isFirstHypothesisLatency)
{
    if (m_recoAdapter != nullptr)
    {
        m_recoAdapter->WriteTelemetryLatency(latencyInTicks, isPhraseLatency, isFirstHypothesisLatency);
    }
    else
    {
        SPX_TRACE_ERROR("%s: m_recoAdapter is null.", __FUNCTION__);
    }
}

void CSpxAudioStreamSession::SendSpeechEventMessage(std::string&& payload)
{
    EnsureInitRecoEngineAdapter();
    m_recoAdapter->SendSpeechEventMessage(std::move(payload));
}

CSpxAsyncOp<bool> CSpxAudioStreamSession::SendNetworkMessage(const char *path, std::string&& payload, bool alwaysSend)
{
    return SendMessageToService(path, payload, alwaysSend);
}

CSpxAsyncOp<bool> CSpxAudioStreamSession::SendNetworkMessage(const char *path, std::vector<uint8_t>&& payload,  bool alwaysSend)
{
    return SendMessageToService(path, payload, alwaysSend);
}

bool CSpxAudioStreamSession::IsStreaming()
{
    return  m_adapterStreamingAudio;
}

std::string CSpxAudioStreamSession::GetSpeechEventPayload(bool startMeeting)
{
    std::string payload;

    auto bIsConversationTranscriber = GetOr<bool>(g_isConversationTranscriber, false);
    if (bIsConversationTranscriber)
    {
        std::shared_ptr<ISpxConversation> conversation;
        {
            std::unique_lock<std::mutex> lock{ m_conversationLock };
            conversation = m_conversation.lock();
        }

        if (conversation != nullptr)
        {
            payload = conversation->GetSpeechEventPayload(startMeeting ? ISpxConversation::MeetingState::START : ISpxConversation::MeetingState::END);
        }
    }
    if (bIsConversationTranscriber)
    {
        std::shared_ptr<ISpxConversation> conversation;
        {
            std::unique_lock<std::mutex> lock{ m_conversationLock };
            conversation = m_conversation.lock();
        }

        if (conversation != nullptr)
        {
            payload = conversation->GetSpeechEventPayload(startMeeting ? ISpxConversation::MeetingState::START : ISpxConversation::MeetingState::END);
        }
    }
    auto bIsMeetingTranscriber = GetOr<bool>(g_isMeetingTranscriber, false);
    if (bIsMeetingTranscriber)
    {
        std::shared_ptr<ISpxMeeting> meeting;
        {
            std::unique_lock<std::mutex> lock{ m_meetingLock };
            meeting = m_meeting.lock();
        }

        if (meeting != nullptr)
        {
            payload = meeting->GetSpeechEventPayload(startMeeting ? ISpxMeeting::MeetingState::START : ISpxMeeting::MeetingState::END);
        }
    }
    if (bIsMeetingTranscriber)
    {
        std::shared_ptr<ISpxMeeting> meeting;
        {
            std::unique_lock<std::mutex> lock{ m_meetingLock };
            meeting = m_meeting.lock();
        }

        if (meeting != nullptr)
        {
            payload = meeting->GetSpeechEventPayload(startMeeting ? ISpxMeeting::MeetingState::START : ISpxMeeting::MeetingState::END);
        }
    }
    return payload;
}

CSpxStringMap CSpxAudioStreamSession::GetParametersFromUser(std::string&& path)
{
    CSpxStringMap parametersFromUser;
    std::shared_ptr<ISpxRecognizer> recognizer;
    {
        std::unique_lock<std::mutex> lock{ m_recognizersLock };
        SPX_DBG_ASSERT(m_recognizers.size() == 1); // we only support 1 recognizer today...
        recognizer = m_recognizers.front().lock();
    }
    auto ct = SpxQueryInterface<ISpxGetUspMessageParamsFromUser>(recognizer);
    if (ct != nullptr)
    {
        parametersFromUser = ct->GetParametersFromUser(std::move(path));
    }
    return parametersFromUser;
}

CSpxStringMap CSpxAudioStreamSession::GetParametersFromRecognizer(std::string&& path)
{
    CSpxStringMap parametersFromRecognizer;
    std::shared_ptr<ISpxRecognizer> recognizer;
    {
        std::unique_lock<std::mutex> lock{ m_recognizersLock };
        SPX_DBG_ASSERT(m_recognizers.size() == 1); // we only support 1 recognizer today...
        recognizer = m_recognizers.front().lock();
    }
    auto ct = SpxQueryInterface<ISpxGetUspMessageParamsFromRecognizer>(recognizer);
    if (ct != nullptr)
    {
        parametersFromRecognizer = ct->GetParametersFromRecognizer(std::move(path));
    }
    return parametersFromRecognizer;
}

void CSpxAudioStreamSession::ForEachRecognizer(std::function<void(std::shared_ptr<ISpxRecognizer>)> fn)
{
    // Make a copy of the recognizers (under lock), to use to send events;
    // otherwise the underlying list could be modified while we're exeuting
    list<weak_ptr<ISpxRecognizer>> weakRecognizers;
    {
        unique_lock<mutex> lock(m_recognizersLock);
        weakRecognizers.assign(m_recognizers.begin(), m_recognizers.end());
    }

    string error;
    SPXAPI_TRY()
    {
        for (auto iter = weakRecognizers.begin(); iter != weakRecognizers.end(); iter++)
        {
            auto recognizer = iter->lock();
            if (recognizer != nullptr)
            {
                fn(iter->lock());
            }
        }
    }
    SPXAPI_CATCH_ONLY()
}

void CSpxAudioStreamSession::ShrinkReplayBuffer(uint64_t newBaseOffset)
{
    // This is not supported for the compressed passthrough. For compressed passthrough we do not create m_audioBuffer
    if (m_audioBuffer != nullptr)
    {
        m_audioBuffer->DiscardTill(newBaseOffset);
    }
}

void CSpxAudioStreamSession::GetCurrentAudioBufferOffset(uint64_t* offsetInTicks, uint64_t* offsetInBytes)
{
    if (m_audioBuffer != nullptr)
    {
        *offsetInBytes = m_audioBuffer->GetCurrentOffset();
        *offsetInTicks = m_audioBuffer->GetAbsoluteOffset();
    }
}

void CSpxAudioStreamSession::GetCurrentAudioContinuationOffset(uint64_t* offsetInTicks)
{
    *offsetInTicks = GetOr<uint64_t>(g_audioContinuationOffset, 0);
}

void CSpxAudioStreamSession::GetMultiChannelProcessingMode(bool* useMultiChannelProcessing)
{
    *useMultiChannelProcessing = GetOr<bool>(PropertyId::Speech_EnableMultiChannelProcessing, false);
}

void CSpxAudioStreamSession::EnsureValidToken()
{
    auto expiry = GetOr("service.auth.token.expirems", "");
    if (expiry == "infinite") {
        SPX_TRACE_INFO("Requested token synchronously.");
        auto task = GetTokenRefreshTask();
        RunSyncOnThreadService(
            m_threadService,
            std::move(task),
            ISpxThreadService::Affinity::User);
    }
    else if (!expiry.empty()) {
        SPX_TRACE_INFO("Expiry is set, schedule token refresh task at beginning.");
        ScheduleTokenRefresh();
    }
}

void CSpxAudioStreamSession::ScheduleTokenRefresh()
{
    // Schedule a refresh at 50% of the token's life, so long as it's more than the min time.
    auto currentTime = PAL::GetMillisecondsSinceEpoch();
    auto expireTime = GetOr<uint64_t>("service.auth.token.expirems", 0);
    auto minValidity = GetOr<uint64_t>("service.auth.token.minvalidityms", 5000);

    if (currentTime >= expireTime ||
        expireTime - currentTime <= minValidity)
    {
        SPX_TRACE_WARNING("Current time %" PRIu64 " is too late to schedule token refresh for expiration %" PRIu64 ".", currentTime, expireTime);
        return;
    }

    auto delayPercentage = GetOr<double>("service.auth.token.refreshpercentage", 50) / 100;

    // Calculate the delay
    uint64_t delay = static_cast<uint64_t>((expireTime - currentTime) * delayPercentage);

    auto task = GetTokenRefreshTask();
    auto threadService = SpxQueryService<ISpxThreadService>(ISpxInterfaceBase::shared_from_this());

    SPX_TRACE_VERBOSE("Scheduling token refresh in %" PRId64  "ms", delay);
    threadService->ExecuteAsync(std::move(task), std::chrono::milliseconds(delay), ISpxThreadService::Affinity::User);
}

std::packaged_task<void()> CSpxAudioStreamSession::GetTokenRefreshTask()
{
    // Get the token to refresh
    auto token = GetOr(PropertyId::SpeechServiceAuthorization_Token, "");

    auto weakPtr = ISpxSession::WkPtr(ISpxSession::shared_from_this());
    std::packaged_task<void()> task([weakPtr = std::move(weakPtr), token = std::move(token)]()-> void
        {
            SPX_TRACE_SCOPE("Background refresh of auth token", "Refresh complete");

            // Get the session.
            auto sessionBase = weakPtr.lock();
            if (nullptr == sessionBase)
            {
                SPX_TRACE_VERBOSE("Session is gone");
                return;
            }
            // See if the token has changed.
            auto sessionProperties = SpxQueryInterface<ISpxNamedProperties>(sessionBase);
            auto currentToken = sessionProperties->GetOr(PropertyId::SpeechServiceAuthorization_Token, "");

            if (currentToken != token)
            {
                SPX_TRACE_VERBOSE("Existing token has been cleared");
                // The current token is not the one we're monitoring. So quit.
                return;
            }

            // trigger the refresh
            auto tokenProvider = SpxQueryService<ISpxRecoEngineAdapterTokenProvider>(sessionBase);
            if (nullptr == tokenProvider)
            {
                SPX_TRACE_WARNING("Token provider was NULL");
                return;
            }

            tokenProvider->RefreshToken();
        });

    return task;
}

void CSpxAudioStreamSession::RefreshToken()
{
    SPX_TRACE_FUNCTION();
    std::wstring sessionIdOverride;
    auto bIsDialogServiceConnector = GetOr<bool>(g_isDialogServiceConnector, false);
    if (bIsDialogServiceConnector)
    {
        sessionIdOverride = PAL::ToWString(CSpxInteractionIdProvider::GetInteractionId(InteractionIdPurpose::Speech));
    }

    SPX_TRACE_VERBOSE("Fired token request event");
    if (m_isDisposing)
    {
        SPX_DBG_TRACE_VERBOSE("[%p]CSpxAudioStreamSession::RefreshToken, recognizer is disposing, ignore events", (void*)this);
        return;
    }

    list<weak_ptr<ISpxRecognizer>> weakRecognizers;
    {
        unique_lock<mutex> lock(m_recognizersLock);
        weakRecognizers.assign(m_recognizers.begin(), m_recognizers.end());
    }

    auto eventSessionId = sessionIdOverride.empty() ? nullptr : sessionIdOverride.c_str();
    std::wstring sessionId = (eventSessionId != nullptr) ? eventSessionId : m_sessionId;

    DispatchEvent(weakRecognizers, sessionId, EventType::TokenRequest, 0, nullptr, std::string{}, 0, nullptr);
}

template<typename T>
CSpxAsyncOp<bool> CSpxAudioStreamSession::SendMessageToService(const char *path, T&& payload, bool alwaysSend)
{
    SPX_DBG_TRACE_SCOPE("SendMessageToService", "SendMessageToService");

    auto keepAlive = SpxSharedPtrFromThis<ISpxSession>(this);

    std::string path2 = path; // Use std::string to copy and preserve accross the thread / lambda boundary.

    std::shared_ptr<std::promise<bool>> messageSentPromise = std::make_shared<std::promise<bool>>();

    auto messageSentFuture = std::shared_future<bool>(messageSentPromise->get_future());
    bool sent = false;
    auto task = CreateTask([this, keepAlive, &sent, alwaysSend, path2 = std::move(path2), payload, &messageSentPromise]() mutable {

       // alwaysSend is set to true by default. It is only set to false in CSpxTranslationRecognizer::UpdateTargetLanguages.
       // This is because CSpxTranslationRecognizer::UpdateTargetLanguages can be called before streaming audio and
       // core throws a "reco mode" not set exception due to reco mode is only set after start streaming data. Adding "alwaysSend||IsStreaming" if statement to avoid the exception.
        if (alwaysSend || IsStreaming())
        {
            EnsureInitRecoEngineAdapter();
            m_recoAdapter->SendNetworkMessage(path2.c_str(), std::move(payload), messageSentPromise);
            sent = true;
        }
     });

    m_threadService->ExecuteSync(std::move(task));

    return CSpxAsyncOp<bool>(messageSentFuture, AsyncOpState::AOS_Started);
}

std::chrono::seconds CSpxAudioStreamSession::GetStopRecognitionTimeout()
{
    std::chrono::seconds timeout;
    auto value = GetStringValue(g_stopRecognitionTimeoutPropertyName, "");

    if (value.empty())
    {
        bool useRnnt = IsUsingRecoEngineRnnt();

        if (m_canceledOnError)
        {
            timeout = c_stopRecognitionTimeoutOnError;
        }
        else if (useRnnt)
        {
            std::chrono::seconds rnntStopRecognitionTimeout = c_offlineStopRecognitionTimeout;

            if (m_audioBuffer && !m_isDisposing)
            {
                // Estimate the time needed to process audio leftovers so
                // that if recognition speed is at least 10% of real-time
                // then it can finish without a timeout and adapter reset.

                auto nonAcknowledgedSizeInBytes = m_audioBuffer->NonAcknowledgedSizeInBytes();
                auto nonAcknowledgedSizeInMsec =
                    BytesToDuration<std::chrono::milliseconds>(nonAcknowledgedSizeInBytes, m_throttleLogic->GetAverageBytesPerSecond());
                auto timeoutInMsec = nonAcknowledgedSizeInMsec * 10;
                auto timeoutInSec = std::chrono::duration_cast<std::chrono::seconds>(timeoutInMsec);

                if (timeoutInSec < timeoutInMsec)
                {
                    timeoutInSec += std::chrono::seconds(1); // std::chrono::ceil is in C++17
                }

                if (timeoutInSec > std::chrono::seconds(0))
                {
                    rnntStopRecognitionTimeout = timeoutInSec;
                }
            }

            timeout = rnntStopRecognitionTimeout;
        }
        else
        {
            timeout = c_defaultStopRecognitionTimeout;
        }
    }
    else // not value.empty()
    {
        timeout = std::chrono::seconds{ std::stoi(value) };
    }

    m_canceledOnError = false;

    return timeout;
}

} } } } // Microsoft::CognitiveServices::Speech::Impl
