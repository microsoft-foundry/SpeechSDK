//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// synthesizer.cpp: Implementation definitions for CSpxSynthesizer C++ class
//

#include "stdafx.h"
#include "synthesizer.h"
#include "synthesizer_connection.h"
#include <future>
#include <thread>
#include <ajv.h>
#include "site_helpers.h"
#include "service_helpers.h"
#include "create_object_helpers.h"
#include "synthesis_helper.h"
#include "guid_utils.h"
#include "log_helpers.h"
#include "error_info.h"
#include "audio_format_id_2_name_map.h"
#include "codec_helpers.h"
#include "time_utils.h"
#include "http_utils.h"
#include "try_catch_helpers.h"
#include "endpoint_utils.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

using namespace std;

CSpxSynthesizer::CSpxSynthesizer()
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
}

CSpxSynthesizer::~CSpxSynthesizer()
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    CSpxSynthesizer::Term();
}

void CSpxSynthesizer::Init()
{
    // NOTE: Due to current ownership model, and our late-into-the-cycle changes for SpeechConfig objects
    // the CSpxSynthesizer is sited to the CSpxApiFactory. This ApiFactory is not held by the
    // dev user at or above the CAPI. Thus ... we must hold it alive in order for the properties to be
    // obtainable via the standard ISpxNamedProperties mechanisms... It will be released in ::Term()
    m_siteKeepAlive = GetSite();

    CheckLogFilename();

    m_threadService = SpxCreateObjectWithSite<ISpxThreadService>("CSpxThreadService", GetSite());
    m_syncCallbacksThreadService = SpxCreateObjectWithSite<ISpxThreadService>("CSpxThreadService", GetSite());
    m_timeoutManagement = make_shared<SynthesisTimeoutManagement>(m_threadService);
    m_timeoutManagement->SetTimeoutCallback([this](const std::shared_ptr<ISpxErrorInformation> &errorInfo) {
        if (m_ttsAdapter) {
            m_ttsAdapter->StopSpeaking(errorInfo);
        } });
    m_latencies = make_shared<SynthesisLatency>();

    EnsureTtsEngineAdapter();

    auto cachePath = Get("SPEECH-SynthesisCachingPath");
    const auto cacheSize = GetOr<int>("SPEECH-SynthesisCachingMaxNumber", -1);
    if (cachePath && !cachePath.Get().empty() && cacheSize > 0)
    {
        m_cache = SpxCreateObjectWithSite<ISpxFileCache>("CSpxAudioFileCacheAdapter", SpxQueryInterface<ISpxGenericSite>(GetSite()));
        if (m_cache)
        {
            auto cacheCapacityInBytes = GetOr<int64_t>("SPEECH-SynthesisCachingCapacityInBytes", -1);
            auto maxSizeOfOneItem = GetOr<int64_t>("SPEECH-SynthesisCachingMaxSizeOfOneItemInBytes", -1);
            m_cache->Init(cacheSize, cachePath.Get(), cacheCapacityInBytes, maxSizeOfOneItem);
        }
        m_cacheExpiredDays = GetOr<int>("SPEECH-SynthesisCachingExpiredDays", 14);
    }
}

void CSpxSynthesizer::Term()
{
    // Stop both thread services before destroying members the workers may
    // call into. Both m_threadService (general worker tasks) and
    // m_syncCallbacksThreadService (event-dispatch tasks) hold tasks that
    // reach into m_ttsAdapter and synthesizer-internal state; stopping them
    // first ensures no worker is executing when those members are freed.
    //
    // See docs/architecture/core-architecture/lifetime-safety-invariants.md
    // invariant 1 (threadservice owners must Term the threadservice before
    // destroying members the worker may call into).
    SpxTermAndClear(m_threadService);
    SpxTermAndClear(m_syncCallbacksThreadService);
    SpxTermAndClear(m_ttsAdapter);

    if (m_audioOutput)
    {
        m_audioOutput->Close();
    }

    SpxTermAndClear(m_siteKeepAlive);
    std::lock_guard<std::mutex> lock(m_wordBoundaryQueueMutex);
    m_backendNameByRequestId.clear();
}

void CSpxSynthesizer::SetOutput(std::shared_ptr<ISpxAudioOutput> output)
{
    m_audioOutput = output;
    m_format = std::make_shared<SynthesisAudioFormat>();

    // Get audio output format string
    const auto outputFormatStr = GetOr(
        PropertyId::SpeechServiceConnection_SynthOutputFormat,
        GetAudioFormatName(SpeechSynthesisOutputFormat::Riff16Khz16BitMonoPcm));

    const auto format = CSpxSynthesisHelper::GetSpeechSynthesisOutputFormatFromString(outputFormatStr);
    auto formatInit = SpxQueryInterface<ISpxAudioStreamInitFormat>(output);
    formatInit->SetFormat(format.get());

    auto outputFormatInit = SpxQueryInterface<ISpxAudioOutputInitFormat>(output);
    const auto hasHeader = outputFormatStr.rfind("riff", 0) == 0;
    outputFormatInit->SetHeader(hasHeader);
    outputFormatInit->SetFormatString(outputFormatStr);

    m_ttsAdapter->SetOutput(output);

    m_format->outputFormat = format;
    m_format->outputFormatString = outputFormatStr;
    m_format->hasHeader = SpxQueryInterface<ISpxAudioOutputFormat>(m_audioOutput)->HasHeader();
    m_audioRender = SpxQueryInterface<ISpxAudioRender>(m_audioOutput);
}

std::shared_ptr<ISpxSynthesisResult> CSpxSynthesizer::Speak(
    const std::string& text, bool isSsml, const std::shared_ptr<ISpxSynthesisRequest> &request)
{
    // Request ID is per speak, different events from same speak will share one request ID
    const auto requestId = PAL::ToString(PAL::CreateGuidWithoutDashes());

    if (request) request->SetRequestId(requestId);

    // Push request into queue
    PushRequestIntoQueue(requestId);

    // Wait until current request to be in front of the queue
    if (!WaitUntilRequestInFrontOfQueue(requestId))
    {
        return CreateUserCancelledResult(requestId);
    }

    m_audioDataStream = SpxCreateObjectWithSite<ISpxAudioDataStream>("CSpxAudioDataStream", GetSite());
    m_audioDataStreamWriter = SpxQueryInterface<ISpxAudioOutput>(m_audioDataStream);
    const auto audioDataStreamInit = SpxQueryInterface<ISpxAudioDataStreamInit>(m_audioDataStream);
    audioDataStreamInit->InitFromFormat(m_format->outputFormat.get(), m_format->hasHeader);

    // Fire SynthesisStarted event
    const auto synthesisStartedResult = CreateResult(requestId, ResultReason::SynthesizingAudioStarted, nullptr, 0);
    FireEvent(EventType::SynthesisResultEvent, synthesisStartedResult);

    // Speak
    return this->ExecuteSynthesis(requestId, text, isSsml, request);
}

CSpxAsyncOp<std::shared_ptr<ISpxSynthesisResult>> CSpxSynthesizer::SpeakAsync(
    const std::string &text, bool isSsml, const std::shared_ptr<ISpxSynthesisRequest> &request)
{
    const auto keepAlive = SpxSharedPtrFromThis<ISpxSynthesizer>(this);
    std::shared_future<std::shared_ptr<ISpxSynthesisResult>> waitForCompletion(std::async(
        std::launch::async, [this, keepAlive, text, isSsml, request]() {
            // Speak
            return Speak(text, isSsml, request);
        }));

    return CSpxAsyncOp<std::shared_ptr<ISpxSynthesisResult>>(waitForCompletion, AOS_Started);
}

std::shared_ptr<ISpxSynthesisResult> CSpxSynthesizer::StartSpeaking(
    const std::string& text, bool isSsml, const std::shared_ptr<ISpxSynthesisRequest> &request)
{
    // Request ID is per speak, different events from same speak will share one request ID
    auto requestId = PAL::ToString(PAL::CreateGuidWithoutDashes());

    if (request) request->SetRequestId(requestId);

    // Push request into queue
    PushRequestIntoQueue(requestId);

    // Wait until current request to be in front of the queue
    if (!WaitUntilRequestInFrontOfQueue(requestId))
    {
        return CreateUserCancelledResult(requestId);
    }

    m_audioDataStream = SpxCreateObjectWithSite<ISpxAudioDataStream>("CSpxAudioDataStream", GetSite());
    m_audioDataStreamWriter = SpxQueryInterface<ISpxAudioOutput>(m_audioDataStream);
    auto audioDataStreamInit = SpxQueryInterface<ISpxAudioDataStreamInit>(m_audioDataStream);
    audioDataStreamInit->InitFromFormat(m_format->outputFormat.get(), m_format->hasHeader);

    // Fire SynthesisStarted event
    auto synthesisStartedResult = CreateResult(requestId, ResultReason::SynthesizingAudioStarted, nullptr, 0);
    FireEvent(EventType::SynthesisResultEvent, synthesisStartedResult);

    auto keepAlive = SpxSharedPtrFromThis<ISpxSynthesizer>(this);
    // just run it in another thread.
    std::thread([this, requestId, text, isSsml, keepAlive, request]() {
        // Speak
        this->ExecuteSynthesis(requestId, text, isSsml, request);
    }).detach();

    return synthesisStartedResult;
}

CSpxAsyncOp<std::shared_ptr<ISpxSynthesisResult>> CSpxSynthesizer::StartSpeakingAsync(
    const std::string& text, bool isSsml, const std::shared_ptr<ISpxSynthesisRequest> &request)
{
    auto keepAlive = SpxSharedPtrFromThis<ISpxSynthesizer>(this);
    std::shared_future<std::shared_ptr<ISpxSynthesisResult>> waitForSpeakStart(std::async(
        std::launch::async, [this, keepAlive, text, isSsml, request]() {
            // Start speaking
            return StartSpeaking(text, isSsml, request);
        }));

    return CSpxAsyncOp<std::shared_ptr<ISpxSynthesisResult>>(waitForSpeakStart, AOS_Started);
}

std::shared_ptr<ISpxSynthesisResult> CSpxSynthesizer::ExecuteSynthesis(const std::string &requestId, const std::string &text,
                                                                       bool isSsml, const std::shared_ptr<ISpxSynthesisRequest> &request)
{
    SPX_TRACE_VERBOSE("%s: synthesis started, request id: %s; text: %s", __FUNCTION__, requestId.c_str(), text.c_str());

    m_synthesisStartedTime = PAL::GetMillisecondsSinceEpoch();
    m_latencies->OnSynthesisStarted();
    // we have 3 metadata events: wordboundary, viseme, bookmark
    m_eventsSyncLatch = std::make_shared<CountDownLatch>(3);

    // check if stop is called during firing synthesis started event
    if (m_shouldStop)
    {
        auto canceledResult = CreateUserCancelledResult(requestId);
        // Fire SynthesisCanceled (depending on the result reason) event
        FireEvent(EventType::SynthesisResultEvent, canceledResult);
        PopRequestFromQueue(requestId);
        return canceledResult;
    }

    EnsureValidToken();

    m_audioReceived = false;
    m_writeToOutputFailed = false;
    m_resultProperties.reset();
    m_audioOutputStartTime = -1;
    m_underrunTime = 0;
    m_synthesisResultAudioDuration = 0;

    m_timeoutManagement->SetTimeoutValues(GetOr<double>(PropertyId::SpeechSynthesis_RtfTimeoutThreshold, 2),
        chrono::milliseconds{ GetOr<int>(PropertyId::SpeechSynthesis_FrameTimeoutInterval, 3000) });
    m_timeoutManagement->OnSynthesisStart();

    m_eventsSyncToAudio = GetOr<bool>(PropertyId::SpeechServiceResponse_SynthesisEventsSyncToAudio, true);

    std::shared_ptr<ISpxSynthesisResult> synthesisDoneResult;
    std::string cacheKey;
    // no cache for text stream mode.
    if (!request) std::tie(synthesisDoneResult, cacheKey) = CreateResultFromCache(requestId, text, isSsml);

    if (!synthesisDoneResult)
    {
        if (request)
        {
            synthesisDoneResult = m_ttsAdapter->Speak(SpxQueryInterface<ISpxSynthesisRequestReader>(request), true);
        }
        else
        {
            synthesisDoneResult = m_ttsAdapter->Speak(text, isSsml, requestId, true);
        }
    }
    else
    {
        SPX_TRACE_VERBOSE("Using cached result for %s.", text.c_str());
    }

    if (m_codecAdapter)
    {
        m_codecBuffer->Close();
    }

    std::shared_ptr<ISpxErrorInformation> decodingError;

    /* Normally, for partial compressed audio, gstreamer will raise a decoding error.
    But gstreamer 1.8.x (default on Ubuntu 16.04) would not raise this error, so we need to wait for the timeout here.
    We can remove the timeout logic when we drop the support for Ubuntu 16.04/gstreamer 1.8.x
    */
    auto remainingTime = 2 * 60 * 1000; // 2 minutes for canceled synthesis
    if (synthesisDoneResult->GetReason() == ResultReason::SynthesizingAudioCompleted)
    {
        remainingTime = 5 * 60 * 1000; // 5 minutes for completed synthesis
    }

    while (!m_decodingDone && remainingTime > 0)
    {
        if (synthesisDoneResult->GetReason() == ResultReason::SynthesizingAudioCompleted && PAL::GetMillisecondsSinceEpoch() - m_decodingStartedTime > 2000 && AudioLengthOfCurrentTurn() < 320)
        {
            SPX_TRACE_ERROR("%s: Codec decoding is not started within 2s.", __FUNCTION__);
            m_decodingDone = true;
            decodingError = ErrorInfo::FromExplicitError(CancellationErrorCode::RuntimeError, "Codec decoding is not started within 2s.");
        }

        SPX_TRACE_VERBOSE("%s: waiting for decoding finished.", __FUNCTION__);
        try
        {
            m_codecAdapter->Read(nullptr, 0);
        }
        catch(const std::exception& e)
        {
            SPX_TRACE_ERROR("%s: Codec decoding error: %s", __FUNCTION__, e.what());
            m_decodingDone = true;
            decodingError = ErrorInfo::FromExplicitError(CancellationErrorCode::RuntimeError, e.what());
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        remainingTime -= 2;
    }

    if (!m_decodingDone)
    {
        const auto errorMessage =
            "Timeout while decoding synthesized audio, probably caused by limited CPU resource. Consider to use raw pcm format for transmission and disable decoding.";
        SPX_TRACE_VERBOSE("%s: Codec decoding error: %s", __FUNCTION__, errorMessage);
        decodingError = ErrorInfo::FromExplicitError(CancellationErrorCode::RuntimeError, errorMessage);
        m_decodingDone = true;
    }

    if (m_audioOfCurrentTurn)
    {
        m_audioOfCurrentTurn->Close();
    }

    m_timeoutManagement->OnSynthesisEnd();

    auto resultInit = SpxQueryInterface<ISpxSynthesisResultInit>(synthesisDoneResult);
    // Update result format
    resultInit->SetAudioFormat(m_format->outputFormat, m_format->hasHeader);
    // Update error if decoding failed
    if (decodingError)
    {
        resultInit->UpdateError(decodingError);
    }
    // Update error if synthesis is stopped during decoding
    if (m_shouldStop && synthesisDoneResult->GetError() == nullptr)
    {
        resultInit->UpdateError(ErrorInfo::FromHttpStatus(HttpStatusCode::CLIENT_CLOSED_REQUEST));
    }
    // Update (decoding) result
    if (AudioLengthOfCurrentTurn())
    {
        auto reader = SpxQueryInterface<ISpxAudioOutputReader>(m_audioOfCurrentTurn);
        auto size = reader->AvailableSize();
        auto audioData = std::make_shared<std::vector<uint8_t>>(size);
        const auto readSize = reader->Read(audioData->data(), size);
        SPX_DBG_ASSERT_WITH_MESSAGE(size == readSize, "Could not read all data from pull audio stream.");
        if (m_synthesisResultAudioDuration == 0)
        {
            m_synthesisResultAudioDuration = static_cast<uint64_t>(readSize) * 1000 / m_format->outputFormat->nAvgBytesPerSec;
        }
        SPX_TRACE_VERBOSE("%s: audio duration: %lu ms.", __FUNCTION__, (unsigned long)m_synthesisResultAudioDuration);
        resultInit->SetAudioData(audioData, m_synthesisResultAudioDuration);
    }
    if (m_writeToOutputFailed && synthesisDoneResult->GetError() == nullptr)
    {
        resultInit->UpdateError(
            ErrorInfo::FromRuntimeMessage(
                "Synthesis succeeded but error occurred when writing audio to speaker or stream. " + m_writeToOutputFailedDetails));
    }

    resultInit->SetAudioDataStream(m_audioDataStream);

    // Update backend name
    auto resultProperties = SpxQueryInterface<ISpxNamedProperties>(synthesisDoneResult);
    resultProperties->SetAsDefault(PropertyId::SpeechServiceResponse_SynthesisBackend, m_backendName.c_str());

    m_latencies->OnAudioFinished();
    m_latencies->SetLatencyProperties(resultProperties);
    resultProperties->Set(
        PropertyId::SpeechServiceResponse_SynthesisUnderrunTimeMs,
        std::to_string(m_underrunTime).c_str());

    if (m_audioDataStream)
    {
        if (synthesisDoneResult->GetReason() == ResultReason::Canceled)
        {
            m_audioDataStream->SetError(synthesisDoneResult->GetError());
        }
        m_audioDataStreamWriter->Close();
    }

    // Wait for audio output to be done
    m_audioOutput->WaitUntilDone();

    // Fire SynthesisCompleted or SynthesisCanceled (depending on the result reason) event
    FireEvent(EventType::SynthesisResultEvent, synthesisDoneResult);

    m_audioOfCurrentTurn.reset();
    m_audioDataStream.reset();
    m_codecAdapter.reset();

    if (isSsml && synthesisDoneResult->GetReason() == ResultReason::SynthesizingAudioCompleted && cacheKey.empty())
    {
        cacheKey = CacheResult(text, synthesisDoneResult);
    }

    SPX_TRACE_INFO("%s: synthesis done, request id: %s; text: %s; request hash: %s", __FUNCTION__, requestId.c_str(), text.c_str(), cacheKey.c_str());

    LogSynthesisEvent(synthesisDoneResult, cacheKey);

    // Pop processed request from queue
    PopRequestFromQueue(requestId);

    return synthesisDoneResult;
}

void CSpxSynthesizer::StopSpeaking()
{
    std::unique_lock<std::mutex> lock(m_stopMutex);
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);

    // clear request queue and push an empty request as a placeholder
    ClearRequestQueueAndKeepFront();

    m_shouldStop = true;

    auto startingInterval = PAL::GetMillisecondsSinceEpoch() - m_synthesisStartedTime;
    if (startingInterval < 20)
    {
        SPX_TRACE_INFO("%s: Synthesis is just started, wait for a while before stopping it.", __FUNCTION__);
        std::this_thread::sleep_for(std::chrono::milliseconds(20 - startingInterval));
    }

    // call stop method of adapter
    m_ttsAdapter->StopSpeaking();

    if (m_codecBuffer)
    {
        m_codecBuffer->ClearUnread();
    }

    if (m_audioOutput)
    {
        while(!m_decodingDone)
        {
            SPX_TRACE_VERBOSE("%s: waiting for decoding finished before clearing audio output.", __FUNCTION__);
            this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        m_audioOutput->ClearUnread();
    }

    if (!WaitUntilRequestInFrontOfQueue(std::string(), 30s))
    {
        SPX_TRACE_ERROR("%s: Timeout stopping speaking.", __FUNCTION__);
        ClearRequestQueueAndKeepFront();
    }

    m_shouldStop = false;

    // Pop the empty request from queue
    PopRequestFromQueue();
}

CSpxAsyncOp<void> CSpxSynthesizer::StopSpeakingAsync()
{
    auto keepAlive = SpxSharedPtrFromThis<ISpxSynthesizer>(this);
    std::shared_future<void> waitForSpeakStop(std::async(std::launch::async, [this, keepAlive]() {
        // stop speaking
        return StopSpeaking();
    }));

    return CSpxAsyncOp<void>(waitForSpeakStop, AOS_Started);
}

std::shared_ptr<ISpxSynthesisVoicesResult> CSpxSynthesizer::GetVoices(const std::string& locale)
{
    EnsureValidToken();
    return m_ttsAdapter->GetVoices(locale);
}

CSpxAsyncOp<std::shared_ptr<ISpxSynthesisVoicesResult>> CSpxSynthesizer::GetVoicesAsync(const std::string& locale)
{
    auto keepAlive = SpxSharedPtrFromThis<ISpxSynthesizer>(this);
    std::shared_future<std::shared_ptr<ISpxSynthesisVoicesResult>> waitForGettingVoicesList(std::async(
        std::launch::async, [this, locale, keepAlive]()
        {
        return GetVoices(locale);
    }));

    return CSpxAsyncOp<std::shared_ptr<ISpxSynthesisVoicesResult>>(waitForGettingVoicesList, AOS_Started);
}

std::shared_ptr<ISpxMessageParamFromUser> CSpxSynthesizer::GetMessageParamFromUser() {
    return SpxQueryInterface<ISpxMessageParamFromUser>(this->m_ttsAdapter);
}

void CSpxSynthesizer::Close()
{
    m_audioOutput->Close();
}

void CSpxSynthesizer::OpenConnection()
{
    EnsureTtsEngineAdapter();

    auto task = CreateTask([this]() {
        EnsureValidToken();
        SPX_TRACE_INFO("opening connection");
        m_ttsAdapter->Connect();
    });

    std::shared_future<void> taskFuture(task.get_future());
    std::promise<bool> executed;
    std::shared_future<bool> executedFuture(executed.get_future());
    m_threadService->ExecuteAsync(std::move(task), ISpxThreadService::Affinity::Background, std::move(executed));
    if (executedFuture.get())
    {
        taskFuture.get();
    }
}

void CSpxSynthesizer::CloseConnection()
{
    if (m_ttsAdapter == nullptr)
    {
        return;
    }

    auto task = CreateTask([this]() {
        m_ttsAdapter->Disconnect(false);
    });

    std::shared_future<void> taskFuture(task.get_future());
    std::promise<bool> executed;
    std::shared_future<bool> executedFuture(executed.get_future());
    m_threadService->ExecuteAsync(std::move(task), ISpxThreadService::Affinity::Background, std::move(executed));
    if (executedFuture.get())
    {
        taskFuture.get();
    }
}

void CSpxSynthesizer::SetDisposing()
{
    SPX_DBG_TRACE_FUNCTION();
    m_isDisposing = true;
    m_threadService->CancelAllTasks();
    if (m_audioOutput)
    {
        m_audioOutput->Close();
    }
}

std::shared_ptr<std::string> CSpxSynthesizer::FormatWordBoundaryForCache()
{
    auto wordBoundary = ajv::json::Build();

    std::unique_lock<std::mutex> lock(m_wordBoundaryQueueMutex);
    unsigned long cacheIndex = 0;
    for (unsigned long index = 0; index < m_wordBoundaryQueue.size(); index++)
    {
        if (m_wordBoundaryQueue[index].sourceBackend != "online (websocket)")
        {
            continue;
        }
        wordBoundary[word_boundary_property::wordBoundaryItems][cacheIndex][word_boundary_property::audioOffset] = m_wordBoundaryQueue[index].audioOffset;
        wordBoundary[word_boundary_property::wordBoundaryItems][cacheIndex][word_boundary_property::duration] = m_wordBoundaryQueue[index].duration;
        wordBoundary[word_boundary_property::wordBoundaryItems][cacheIndex][word_boundary_property::textOffset] = m_wordBoundaryQueue[index].textOffset;
        wordBoundary[word_boundary_property::wordBoundaryItems][cacheIndex][word_boundary_property::wordLength] = m_wordBoundaryQueue[index].wordLength;
        wordBoundary[word_boundary_property::wordBoundaryItems][cacheIndex][word_boundary_property::text] = m_wordBoundaryQueue[index].text;
        wordBoundary[word_boundary_property::wordBoundaryItems][cacheIndex][word_boundary_property::boundaryType] = static_cast<int>(m_wordBoundaryQueue[index].boundaryType);
        cacheIndex++;
    }

    if (wordBoundary[word_boundary_property::wordBoundaryItems].ValueCount() == 0)
    {
        return nullptr;
    }

    return std::make_shared<std::string>(wordBoundary.AsJson());
}

std::shared_ptr<ISpxSynthesisResult> CSpxSynthesizer::CreateUserCancelledResult(const std::string& requestId)
{
    auto cancelledResult = CreateResult(requestId, ResultReason::Canceled, nullptr, 0);
    if (GetFrontRequestId() == requestId)
    {
        m_audioDataStream->SetError(cancelledResult->GetError());
        m_audioDataStreamWriter->Close();
    }
    return cancelledResult;
}

void CSpxSynthesizer::LogSynthesisEvent(std::shared_ptr<ISpxSynthesisResult> result, const std::string& requestHash)
{
    if (m_enableTelemetry && m_telemetryManager != nullptr)
    {
        std::map<std::string, std::string> telemetryProperties;
        telemetryProperties["OfflineDataLocation"] = GetOr<std::string>(PropertyId::SpeechServiceConnection_SynthOfflineDataPath, "");
        telemetryProperties["SwitchingPolicy"] = GetStringValue("SPEECH-SynthBackendSwitchingPolicy");
        telemetryProperties["ResultId"] = result->GetResultId();
        telemetryProperties["CustomerId"] = GetStringValue("EmbeddedSynthesis-CustomerId");
        telemetryProperties["RequestHash"] = requestHash;

        auto resultProperties = SpxQueryInterface<ISpxNamedProperties>(result);

        if (result->GetReason() == ResultReason::Canceled)
        {
            telemetryProperties["ErrorDetails"] = resultProperties->GetOr<std::string>(PropertyId::CancellationDetails_ReasonDetailedText, "");
            m_telemetryManager->LogEvent("SynthesisCanceled", telemetryProperties, ISpxTelemetryManager::Category::Critical);
        }
        else
        {
            telemetryProperties["FirstByteLatencyInMs"] = resultProperties->GetOr<std::string>(PropertyId::SpeechServiceResponse_SynthesisFirstByteLatencyMs, "");
            telemetryProperties["ProcessingTimeInMs"] = resultProperties->GetOr<std::string>(PropertyId::SpeechServiceResponse_SynthesisFinishLatencyMs, "");
            telemetryProperties["UnderrunTimeInMs"] = resultProperties->GetOr<std::string>(PropertyId::SpeechServiceResponse_SynthesisUnderrunTimeMs, "");
            telemetryProperties["FinishedBy"] = resultProperties->GetOr<std::string>(PropertyId::SpeechServiceResponse_SynthesisBackend, "");
            telemetryProperties["CharNumber"] = resultProperties->GetStringValue("CharNumber");
            telemetryProperties["OfflineVoiceName"] = resultProperties->GetStringValue("OfflineVoiceName");
            m_telemetryManager->LogEvent("SynthesisCompleted", telemetryProperties, ISpxTelemetryManager::Category::Normal);
        }
    }
}

void CSpxSynthesizer::FireResultEvent(std::shared_ptr<ISpxSynthesisResult> result, std::shared_ptr<CountDownLatch> eventsSyncLatch)
{
    SPX_DBG_TRACE_VERBOSE("[%p]CSpxSynthesizer::%s", (void*)this, __FUNCTION__);

    SynthEvent_Type* pevent = nullptr;
    switch (result->GetReason())
    {
    case ResultReason::SynthesizingAudioStarted:
        pevent = &SynthesisStarted;
        break;

    case ResultReason::SynthesizingAudio:
        pevent = &Synthesizing;
        break;

    case ResultReason::SynthesizingAudioCompleted:
        SPX_TRACE_VERBOSE("%s: Waiting for all metadata events triggered.", __FUNCTION__);
        if (!eventsSyncLatch->Await(30s))
        {
            SPX_DBG_TRACE_ERROR("[%p]CSpxSynthesizer::%s: timeout waiting for events sync latch.", (void*)this, __FUNCTION__);
        }

        pevent = &SynthesisCompleted;
        break;

    case ResultReason::Canceled:
        pevent = &SynthesisCanceled;
        break;

    default:
        break;
    }

    if (pevent != nullptr )
    {
        if (pevent->IsConnected())
        {
            auto synthEvent = SpxCreateObjectWithSite<ISpxSynthesisEventArgs>("CSpxSynthesisEventArgs", SpxSiteFromThis(this));
            auto argsInit = SpxQueryInterface<ISpxSynthesisEventArgsInit>(synthEvent);
            argsInit->Init(result);
            pevent->Signal(synthEvent);
        }
        else
        {
            SPX_DBG_TRACE_VERBOSE("No listener connected to event");
        }
    }
}

void CSpxSynthesizer::FireWordBoundary(std::string resultId, uint64_t audioOffset, uint64_t duration, uint32_t textOffset,
                                       uint32_t wordLength, std::string text,
                                       SpeechSynthesisBoundaryType boundaryType)
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    const auto wordBoundaryEvent = SpxCreateObjectWithSite<ISpxWordBoundaryEventArgs>("CSpxWordBoundaryEventArgs", SpxSiteFromThis(this));
    auto argsInit = SpxQueryInterface<ISpxWordBoundaryEventArgsInit>(wordBoundaryEvent);
    argsInit->Init(audioOffset, duration, textOffset, wordLength, text, boundaryType);
    auto eventInit = SpxQueryInterface<ISpxSpeechSynthesisMetadataEventArgsInit>(wordBoundaryEvent);
    eventInit->SetResultId(std::move(resultId));
    WaitForCurrentEventTriggered(audioOffset);
    WordBoundary.Signal(wordBoundaryEvent);
}

void CSpxSynthesizer::FireVisemeReceived(std::string resultId, uint64_t audioOffset, uint32_t visemeId, std::string animation)
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);

    const auto visemeEvent = SpxCreateObjectWithSite<ISpxVisemeEventArgs>("CSpxVisemeEventArgs", SpxSiteFromThis(this));
    auto argsInit = SpxQueryInterface<ISpxVisemeEventArgsInit>(visemeEvent);
    argsInit->Init(audioOffset, visemeId, animation);
    auto eventInit = SpxQueryInterface<ISpxSpeechSynthesisMetadataEventArgsInit>(visemeEvent);
    eventInit->SetResultId(std::move(resultId));
    WaitForCurrentEventTriggered(audioOffset);
    VisemeReceived.Signal(visemeEvent);
}

void CSpxSynthesizer::FireBookmarkReached(std::string resultId, uint64_t audioOffset, std::string text)
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);

    const auto bookmarkEvent = SpxCreateObjectWithSite<ISpxBookmarkEventArgs>("CSpxBookmarkEventArgs", SpxSiteFromThis(this));
    auto argsInit = SpxQueryInterface<ISpxBookmarkEventArgsInit>(bookmarkEvent);
    argsInit->Init(audioOffset, text);
    auto eventInit = SpxQueryInterface<ISpxSpeechSynthesisMetadataEventArgsInit>(bookmarkEvent);
    eventInit->SetResultId(std::move(resultId));
    WaitForCurrentEventTriggered(audioOffset);
    BookmarkReached.Signal(bookmarkEvent);
}

void CSpxSynthesizer::FireConnectionChanged(bool connected)
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    SPX_DBG_TRACE_VERBOSE("%s: %s", __FUNCTION__, connected ? "Connected" : "Disconnected");

    const auto connectionEvent = SpxCreateObjectWithSite<ISpxConnectionEventArgs>("CSpxConnectionEventArgs", SpxSiteFromThis(this));
    auto argsInit = SpxQueryInterface<ISpxConnectionEventArgsInit>(connectionEvent);
    // ConnectionEventArgs needs session id to initialize, but synthesis don't have session id, just using empty string.
    argsInit->Init(std::wstring());
    if (connected)
    {
        Connected.Signal(connectionEvent);
    }
    else
    {
        Disconnected.Signal(connectionEvent);
    }
}

void CSpxSynthesizer::FireTokenRequest()
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    const auto tokenRequestEvent = SpxCreateObjectWithSite<ISpxSessionEventArgs>("CSpxTokenReqeustEventArgs", SpxSiteFromThis(this));
    auto argsInit = SpxQueryInterface<ISpxSessionEventArgsInit>(tokenRequestEvent);
    argsInit->Init(L"mockSession");
    TokenRequested.Signal(tokenRequestEvent);
}

uint32_t CSpxSynthesizer::Write(ISpxTtsEngineAdapter*, const std::string& requestId, uint8_t* buffer,
                                uint32_t size, std::shared_ptr<std::map<std::string, std::string>> properties)
{
    const auto currentRequestId = GetFrontRequestId();
    if (requestId != currentRequestId)
    {
        SPX_DBG_TRACE_WARNING("%s: requestId (%s) differs from queue front (%s), ignored", __FUNCTION__,
                            requestId.c_str(), currentRequestId.c_str());
        return size;
    }

    if (m_shouldStop)
    {
        return size;
    }

    m_audioReceived = true;
    if (m_resultProperties == nullptr && properties != nullptr)
    {
        m_resultProperties = properties;
    }

    if (m_audioOfCurrentTurn == nullptr)
    {
        m_needDecoding = (m_adapterFormat->wFormatTag != m_format->outputFormat->wFormatTag ||
            m_adapterFormat->nSamplesPerSec != m_format->outputFormat->nSamplesPerSec);
        m_audioOfCurrentTurn = SpxCreateObjectWithSite<ISpxAudioOutput>("CSpxPullAudioOutputStream", GetSite());
        string codecAdapterClassName;
    #ifdef __ANDROID__
        codecAdapterClassName = "CSpxAndroidCodecAdapter";
    #elif defined(__APPLE__)
        codecAdapterClassName = "CSpxAppleCodecAdapter";
    #else
        codecAdapterClassName = "CSpxCodecAdapter";
    #endif
        if (m_needDecoding)
        {
            m_codecAdapter = SpxCreateObjectWithSite<ISpxAudioStreamReader>(codecAdapterClassName.c_str(), GetSite());
            SPX_THROW_HR_IF(SPXERR_GSTREAMER_NOT_FOUND_ERROR, m_codecAdapter == nullptr);

            m_decodingDone = false;

            m_codecBuffer = SpxCreateObjectWithSite<ISpxAudioOutput>("CSpxPullAudioOutputStream", GetSite());
            const auto reader = SpxQueryInterface<ISpxAudioOutputReader>(m_codecBuffer);

            auto initCallbacks = SpxQueryInterface<ISpxAudioStreamReaderInitCallbacks>(m_codecAdapter);
            initCallbacks->SetCallbacks(
                [reader](uint8_t* _buffer, uint32_t _size) { return reader->Read(_buffer, _size); },
                []() {  });

            initCallbacks->SetPropertyCallback2(
                [this](PropertyId propertyId)
                { return this->GetOr(propertyId, ""); });

            auto writerInitCallbacks = SpxQueryInterface<ISpxAudioStreamWriterInitCallbacks>(m_codecAdapter);
            writerInitCallbacks->SetWriterCallbacks(
                [this, requestId](const uint8_t* _buffer, uint32_t _size) {
                    return this->WriteToOutput(_buffer, _size, requestId); },
                [this]() { this->m_decodingDone = true; });

            auto adapterAsCodec = SpxQueryInterface<ISpxAudioCodecAdapter>(m_codecAdapter);
            adapterAsCodec->EnableThrottling(GetOr<bool>("SPEECH-SynthThrottleDecoding", false));
            adapterAsCodec->SetSourceFormat(m_adapterFormat.get());

            auto adapterAsSetFormat = SpxQueryInterface<ISpxAudioStreamInitFormat>(m_codecAdapter);
            const auto decoderFormat = SpxCopyWAVEFORMATEX(m_adapterFormat);
            decoderFormat->nChannels = m_format->outputFormat->nChannels;
            decoderFormat->wBitsPerSample = m_format->outputFormat->wBitsPerSample;
            decoderFormat->nSamplesPerSec = m_format->outputFormat->nSamplesPerSec;
            adapterAsSetFormat->SetFormat(decoderFormat.get());
            // trigger decoding start
            m_codecAdapter->Read(nullptr, 0);
            m_decodingStartedTime = PAL::GetMillisecondsSinceEpoch();
        }
    }

    if (buffer == nullptr || size <= 0)
    {
        return 0;
    }

    if (m_needDecoding)
    {
        return m_codecBuffer->Write(buffer, size);
    }

    return WriteToOutput(buffer, size, requestId);
}

uint32_t CSpxSynthesizer::WriteToOutput(const uint8_t* buffer, uint32_t size, const std::string& requestId)
{
    if (m_shouldStop)
    {
        return size;
    }

    m_latencies->OnAudioReceived();

    // Fire Synthesizing event
    const auto result = CreateResult(requestId, ResultReason::SynthesizingAudio, buffer, size, m_resultProperties);
    FireEvent(EventType::SynthesisResultEvent, result);

    m_timeoutManagement->OnAudioReceived(size * 1000 / m_format->outputFormat->nAvgBytesPerSec);

    if (m_audioOutputStartTime > 0)
    {
        const auto currentTime = PAL::GetMillisecondsSinceEpoch();
        auto reader = SpxQueryInterface<ISpxAudioOutputReader>(m_audioOfCurrentTurn);
        const int64_t underrunTime = currentTime - m_audioOutputStartTime -
            (reader->AvailableSize() * 1000 / m_format->outputFormat->nAvgBytesPerSec);

        if (underrunTime > m_underrunTime)
        {
            SPX_DBG_TRACE_WARNING("Buffer underrun occurs, previous underrun time %ld, current underrun time %ld", (long)m_underrunTime, (long)underrunTime);
            m_underrunTime = underrunTime;
        }
    }

    m_audioOfCurrentTurn->Write(buffer, size);
    m_audioDataStreamWriter->Write(buffer, size);

    uint32_t writeSize = 0;
    if (!m_writeToOutputFailed)
    {
        try
        {
            writeSize = m_audioOutput->Write(buffer, size);
        }
        catch (const std::exception& e)
        {
            m_writeToOutputFailed = true;
            m_writeToOutputFailedDetails = e.what();
            SPX_DBG_TRACE_ERROR("%s: Write to output failed with exception: %s", __FUNCTION__, e.what());
        }
    }

    if (m_audioOutputStartTime < 0)
    {
        auto reader = SpxQueryInterface<ISpxAudioOutputReader>(m_audioOfCurrentTurn);
        if (reader->AvailableSize() * 1000 / m_format->outputFormat->nAvgBytesPerSec >
            GetOr<int64_t>(PropertyId::AudioConfig_PlaybackBufferLengthInMs, 50))
        {
            m_audioOutputStartTime = PAL::GetMillisecondsSinceEpoch();
        }
    }

    return writeSize;
}

std::shared_ptr<ISpxSynthesizerEvents> CSpxSynthesizer::GetEventsSite()
{
    std::shared_ptr<ISpxGenericSite> site = static_cast<ISpxGenericSite*>(this)->shared_from_this();
    return SpxQueryInterface<ISpxSynthesizerEvents>(site);
}

std::shared_ptr<ISpxSynthesisResult> CSpxSynthesizer::CreateEmptySynthesisResult()
{
    return SpxCreateObjectWithSite<ISpxSynthesisResult>("CSpxSynthesisResult", this);
}

std::shared_ptr<ISpxSynthesisVoicesResult> CSpxSynthesizer::CreateEmptySynthesisVoicesResult()
{
    return SpxCreateObjectWithSite<ISpxSynthesisVoicesResult>("CSpxSynthesisVoicesResult", this);
}

std::shared_ptr<ISpxVoiceInfo> CSpxSynthesizer::CreateEmptyVoiceInfo()
{
    return SpxCreateObjectWithSite<ISpxVoiceInfo>("CSpxVoiceInfo", this);
}

void CSpxSynthesizer::SetAdapterFormat(const ISpxTtsEngineAdapter*, const std::shared_ptr<SPXWAVEFORMATEX> &format)
{
    m_adapterFormat = format;
}

void CSpxSynthesizer::FireAdapterResult_WordBoundary(ISpxTtsEngineAdapter *, uint64_t audioOffset,
                                                     uint64_t duration, uint32_t textOffset, uint32_t wordLength,
                                                     const std::string &text, SpeechSynthesisBoundaryType boundaryType)
{
    SPX_DBG_TRACE_VERBOSE("%s: audioOffset %" PRIu64 ", duration %" PRIu64 ", textOffset %u, wordLength %u, text [%s], boundary type %d",
                         __FUNCTION__, audioOffset, duration, textOffset, wordLength, text.c_str(), (int)boundaryType);
    const auto backend = GetBackendName();
    FireWordBoundaryToInternalQueue(audioOffset, duration, textOffset, wordLength, text, boundaryType, backend);
    FireEvent(EventType::WordBoundaryEvent, nullptr, true, audioOffset, duration, textOffset, wordLength, text, boundaryType);
}

void CSpxSynthesizer::FireAdapterResult_VisemeReceived(ISpxTtsEngineAdapter*, uint64_t audioOffset,
                                                       uint32_t visemeId, std::string animation)
{
    FireEvent(EventType::VisemeEvent, nullptr, true, audioOffset, 0, visemeId, 0, std::move(animation));
}

void CSpxSynthesizer::FireAdapterResult_BookmarkReached(ISpxTtsEngineAdapter*, uint64_t audioOffset,
                                                        const std::string& text)
{
    FireEvent(EventType::BookmarkEvent, nullptr, true, audioOffset, 0, 0, 0, text);
}

void CSpxSynthesizer::FireAdapterResult_ConnectionChanged(ISpxTtsEngineAdapter*, bool connected)
{
    FireEvent(EventType::ConnectionChanged, nullptr, connected);
    if (connected)
    {
        m_latencies->OnConnected();
    }
}

void CSpxSynthesizer::FireAdapterResult_TurnStarted(ISpxTtsEngineAdapter*)
{
    m_latencies->OnServiceEchoReceived();
}

void CSpxSynthesizer::EndOfTurn(ISpxTtsEngineAdapter*)
{
    this->FireEvent(EventType::SynthesisMetadataEndEvent, nullptr, true);
}

void CSpxSynthesizer::SetSynthesisResultAudioDuration(ISpxTtsEngineAdapter*, uint64_t duration)
{
    m_synthesisResultAudioDuration = duration;
}

size_t CSpxSynthesizer::AudioLengthOfCurrentTurn()
{
    if (m_audioOfCurrentTurn == nullptr)
    {
        return 0;
    }

    auto reader = SpxQueryInterface<ISpxAudioOutputReader>(m_audioOfCurrentTurn);
    auto size = reader->AvailableSize();

    if (size == 0 && m_audioReceived)
    {
        // audio chunk is feed to codec decoder, but decode is not done; return 1 as a placeholder.
        return 1;
    }
    return size;
}

shared_ptr<ISpxConnection> CSpxSynthesizer::GetConnection()
{
    // we cannot rely on the site to create objects since we may want to get a Connection
    // instance from this *before* the site is set. Since the Connection instance doesn't
    // need to have a site, we can create directly here
    auto ptr = new CSpxSynthesizerConnection();
    auto interface = static_cast<ISpxConnection*>(ptr);
    std::shared_ptr<ISpxConnection> connection(interface);

    auto connectionInit = connection->QueryInterface<ISpxSynthesizerConnection>();
    SPX_THROW_HR_IF(SPXERR_UNEXPECTED_TTS_ENGINE_SITE_FAILURE, connection == nullptr);

    connectionInit->Init(ISpxSynthesizer::shared_from_this());

    return connection;
}

std::shared_ptr<ISpxTtsEngineAdapter> CSpxSynthesizer::GetTtsEngineAdapter()
{
    SPX_TRACE_WARNING_IF(m_ttsAdapter == nullptr, "%s: adapter is not initialized.", __FUNCTION__);
    return m_ttsAdapter;
}

std::shared_ptr<ISpxNamedProperties> CSpxSynthesizer::GetParentProperties() const
{
    return SpxQueryService<ISpxNamedProperties>(GetSite());
}

void CSpxSynthesizer::CheckLogFilename()
{
    SpxDiagLogSetProperties(GetParentProperties());
}

void CSpxSynthesizer::PushRequestIntoQueue(const std::string requestId)
{
    std::unique_lock<std::mutex> lock(m_queueOperationMutex);
    m_requestQueue.push_back(requestId);
    m_cv.notify_all();
}

bool CSpxSynthesizer::WaitUntilRequestInFrontOfQueue(const std::string& requestId)
{
    std::unique_lock<std::mutex> lock(m_requestWaitingMutex);

    auto pred = [&] {
        std::unique_lock<std::mutex> queueLock(m_queueOperationMutex);
        return std::find(m_requestQueue.begin(), m_requestQueue.end(), requestId) == m_requestQueue.end() ||
            m_requestQueue.front() == requestId;
    };

#ifdef _DEBUG
    while (!m_cv.wait_for(lock, std::chrono::milliseconds(100), pred))
    {
        SPX_DBG_TRACE_VERBOSE("%s: waiting for processing speak request %s, current queue front: %s", __FUNCTION__,
                                requestId.c_str(), m_requestQueue.front().c_str());
    }
#else
    m_cv.wait(lock, pred);
#endif

    return !m_requestQueue.empty() && m_requestQueue.front() == requestId;
}

template <class Rep, class Period>
bool CSpxSynthesizer::WaitUntilRequestInFrontOfQueue(const std::string& requestId, const std::chrono::duration<Rep, Period>& timeout)
{
    std::unique_lock<std::mutex> lock(m_requestWaitingMutex);

    auto pred = [&] {
        std::unique_lock<std::mutex> queueLock(m_queueOperationMutex);
        return std::find(m_requestQueue.begin(), m_requestQueue.end(), requestId) == m_requestQueue.end() ||
            m_requestQueue.front() == requestId;
    };

    m_cv.wait_for(lock, timeout, pred);
    return !m_requestQueue.empty() && m_requestQueue.front() == requestId;
}

void CSpxSynthesizer::PopRequestFromQueue(const std::string& requestId)
{
    std::unique_lock<std::mutex> lock(m_queueOperationMutex);
    if (!m_requestQueue.empty() && m_requestQueue.front() == requestId)
    {
        m_requestQueue.pop_front();
    }
    else
    {
        SPX_TRACE_ERROR("%s: request %s not found in queue. queue front: %s", __FUNCTION__, requestId.c_str(),
                        m_requestQueue.empty() ? "empty" : m_requestQueue.front().c_str());
    }
    m_cv.notify_all();
}

void CSpxSynthesizer::ClearRequestQueueAndKeepFront()
{
    std::unique_lock<std::mutex> lock(m_queueOperationMutex);
    if (!m_requestQueue.empty())
    {
        const auto front = m_requestQueue.front();
        m_requestQueue.clear();
        m_requestQueue.push_back(front);
    }

    m_requestQueue.emplace_back();
    m_cv.notify_all();
}

std::string CSpxSynthesizer::GetFrontRequestId() const
{
    std::unique_lock<std::mutex> lock(m_queueOperationMutex);
    return m_requestQueue.empty() ? "" : m_requestQueue.front();
}

std::shared_ptr<ISpxSynthesisResult> CSpxSynthesizer::CreateResult(const std::string& requestId, ResultReason reason,
                                                                   const uint8_t* audio_buffer, size_t audio_length,
                                                                   const std::shared_ptr<std::map<
                                                                       std::string, std::string>>& properties)
{
    // Generate cancellation error information when relevant; keep it consistent with the USP version
    std::shared_ptr<ISpxErrorInformation> errorInfo;
    if (reason == ResultReason::Canceled)
    {
        errorInfo = ErrorInfo::FromHttpStatus(HttpStatusCode::CLIENT_CLOSED_REQUEST);
    }

    // Build result
    auto result = CreateEmptySynthesisResult();
    auto resultInit = SpxQueryInterface<ISpxSynthesisResultInit>(result);
    resultInit->InitSynthesisResult(requestId, reason, errorInfo);
    resultInit->SetAudioFormat(m_format->outputFormat, m_format->hasHeader);

    if (reason == ResultReason::Canceled)
    {
        auto cancelledAudioDataStream = SpxCreateObjectWithSite<ISpxAudioDataStream>("CSpxAudioDataStream", GetSite());
        resultInit->SetAudioDataStream(cancelledAudioDataStream);
        cancelledAudioDataStream->SetError(errorInfo);
    }
    else
    {
        resultInit->SetAudioDataStream(m_audioDataStream);
    }

    auto resultProperties = SpxQueryInterface<ISpxNamedProperties>(result);
    if (properties != nullptr)
    {
        for (const auto &p : *properties)
        {
            resultProperties->SetStringValue(p.first.c_str(), p.second.c_str());
        }
    }

    if (reason == ResultReason::SynthesizingAudio)
    {
        auto audio = std::make_shared<std::vector<uint8_t>>(audio_length);
        memcpy(audio->data(), audio_buffer, audio_length);
        resultInit->SetAudioData(audio, 0);
        resultProperties->SetAsDefault(PropertyId::SpeechServiceResponse_SynthesisBackend, m_backendName.c_str());
    }

    return result;
}

void CSpxSynthesizer::DispatchEvent(
    const weak_ptr<ISpxSynthesizer>& weakSynthesizer,
    EventType eventType,
    std::string resultId,
    std::shared_ptr<ISpxSynthesisResult> result,
    bool connected,
    uint64_t offset,
    uint64_t duration,
    uint32_t textOffset,
    uint32_t textLength,
    std::string text,
    SpeechSynthesisBoundaryType boundaryType,
    std::shared_ptr<CountDownLatch> eventsSyncLatch)
{
    string error;
    SPXAPI_TRY()
    {
        auto synthesizer = weakSynthesizer.lock();
        if (!synthesizer)
        {
            return;
        }

        auto ptr = SpxQueryInterface<ISpxSynthesizerEvents>(synthesizer);

        if (!ptr)
        {
            return;
        }

        switch (eventType)
        {
        case EventType::SynthesisResultEvent:
            ptr->FireResultEvent(result, eventsSyncLatch);
            break;

        case EventType::ConnectionChanged:
            ptr->FireConnectionChanged(connected);
            break;

        case EventType::WordBoundaryEvent:
            ptr->FireWordBoundary(resultId, offset, duration, textOffset, textLength, std::move(text), boundaryType);
            break;

        case EventType::VisemeEvent:
            ptr->FireVisemeReceived(resultId, offset, textOffset, std::move(text));
            break;

        case EventType::BookmarkEvent:
            ptr->FireBookmarkReached(resultId, offset, std::move(text));
            break;

        case EventType::SynthesisMetadataEndEvent:
            eventsSyncLatch->CountDown();
            break;

        case EventType::TokenRequestEvent:
            ptr->FireTokenRequest();
            break;

        default:
            SPX_TRACE_ERROR("EventDelivery unknown event type %d", (int)eventType);
        }
    }
    SPXAPI_CATCH_ONLY()
}

void CSpxSynthesizer::FireEvent(
    EventType eventType,
    shared_ptr<ISpxSynthesisResult> result,
    bool connected,
    uint64_t offset,
    uint64_t duration,
    uint32_t textOffset,
    uint32_t textLength,
    std::string text,
    SpeechSynthesisBoundaryType boundaryType)
{
    if (m_isDisposing)
    {
        SPX_DBG_TRACE_VERBOSE("[%p]CSpxSynthesizer::%s, synthesizer is disposing, ignore events",
            static_cast<void*>(this),
            __FUNCTION__);
        return;
    }

    // Make a copy of the recognizers (under lock), to use to send events;
    // otherwise the underlying list could be modified while we're sending events...
    weak_ptr<ISpxSynthesizer> weakSynthesizer = SpxSharedPtrFromThis<ISpxSynthesizer>(this);

    const auto currentRequestId = GetFrontRequestId();

    // Schedule event dispatch on the user facing thread.
    // DispatchEvent is exception safe in order not to cause livelock of failed messages.
    // i.e. if some events cannot be delivered to the user, we do not try to deliver events about failed events...
    auto eventsSyncLatch = m_eventsSyncLatch;

    if (eventType == EventType::SynthesisMetadataEndEvent)
    {
        auto wordboundaryTask = CreateTask([this, weakSynthesizer, currentRequestId, eventType, result, connected, offset, duration, textOffset, textLength, text{ std::move(text) }, boundaryType, eventsSyncLatch ]()
        {
            SPX_DBG_TRACE_VERBOSE("Word boundary events are all dispatched.");
            DispatchEvent(weakSynthesizer, eventType, std::move(currentRequestId), result, connected, offset, duration, textOffset, textLength, std::move(text), boundaryType, eventsSyncLatch);
        });
        m_syncCallbacksThreadService->ExecuteAsync(std::move(wordboundaryTask), ISpxThreadService::Affinity::User);
        auto visemeTask = CreateTask([this, weakSynthesizer, currentRequestId, eventType, result, connected, offset, duration, textOffset, textLength, text{ std::move(text) }, boundaryType, eventsSyncLatch ]()
        {
            SPX_DBG_TRACE_VERBOSE("Viseme events are all dispatched.");
            DispatchEvent(weakSynthesizer, eventType, std::move(currentRequestId), result, connected, offset, duration, textOffset, textLength, std::move(text), boundaryType, eventsSyncLatch);
        });
        m_syncCallbacksThreadService->ExecuteAsync(std::move(visemeTask), ISpxThreadService::Affinity::Background);
        auto bookmarkTask = CreateTask([this, weakSynthesizer, currentRequestId, eventType, result, connected, offset, duration, textOffset, textLength, text, boundaryType, eventsSyncLatch]()
        {
            SPX_DBG_TRACE_VERBOSE("Bookmark events are all dispatched.");
            DispatchEvent(weakSynthesizer, eventType, std::move(currentRequestId), result, connected, offset, duration, textOffset, textLength, std::move(text), boundaryType, eventsSyncLatch);
        });
        m_syncCallbacksThreadService->ExecuteAsync(std::move(bookmarkTask), ISpxThreadService::Affinity::Media);
        return;
    }
    auto task = CreateTask([this, weakSynthesizer, currentRequestId, eventType, result, connected, offset, duration, textOffset, textLength, text{ std::move(text) }, boundaryType, eventsSyncLatch]()
    {
        SPX_DBG_TRACE_SCOPE("DispatchEvent task started...", "DispatchEvent task complete!");
        DispatchEvent(weakSynthesizer, eventType, std::move(currentRequestId), result, connected, offset, duration, textOffset, textLength, std::move(text), boundaryType, eventsSyncLatch);
    });

    switch (eventType)
    {
    case EventType::WordBoundaryEvent:
        m_syncCallbacksThreadService->ExecuteAsync(std::move(task), ISpxThreadService::Affinity::User);
        break;
    case EventType::VisemeEvent:
        m_syncCallbacksThreadService->ExecuteAsync(std::move(task), ISpxThreadService::Affinity::Background);
        break;
    case EventType::BookmarkEvent:
        m_syncCallbacksThreadService->ExecuteAsync(std::move(task), ISpxThreadService::Affinity::Media);
        break;
    case EventType::TokenRequestEvent:
        m_threadService->ExecuteAsync(std::move(task), ISpxThreadService::Affinity::Background);
        break;
    default:
        m_threadService->ExecuteAsync(std::move(task), ISpxThreadService::Affinity::User);
    }
}

packaged_task<void()> CSpxSynthesizer::CreateTask(function<void()> func)
{
    // Creates a packaged task that propagates all exceptions
    // to the user thread and then user callback.
    return packaged_task<void()>([this, func]() {
        // Make Android compiler happy.
        UNUSED(this);
        func();
    });
}

void CSpxSynthesizer::EnsureTtsEngineAdapter()
{
    if (m_ttsAdapter == nullptr)
    {
        InitializeTtsEngineAdapter();
    }
}

void CSpxSynthesizer::InitializeTtsEngineAdapter()
{
    const auto backend = PAL::StringUtils::ToLower(
        GetOr<std::string>(PropertyId::SpeechServiceConnection_SynthBackend, std::string()));

    if (backend == "offline" || backend == "local")
    {
        m_ttsAdapter = SpxCreateObjectWithSite<ISpxTtsEngineAdapter>("CSpxLocalTtsEngineAdapter", this);
        m_backendName = "offline";
        SPX_THROW_HR_IF(SPXERR_EXTENSION_LIBRARY_NOT_FOUND, m_ttsAdapter == nullptr);
        InitializeTelemetryManager();
    }
    else if (backend == "hybrid")
    {
        m_ttsAdapter = SpxCreateObjectWithSite<ISpxTtsEngineAdapter>("CSpxHybridTtsEngineAdapter", this);
        // default backend for hybrid
        m_backendName = "online (websocket)";
        SPX_THROW_HR_IF(SPXERR_EXTENSION_LIBRARY_NOT_FOUND, m_ttsAdapter == nullptr);
        // Use more strict timeout values for hybrid scenario
        SetAsDefault(PropertyId::SpeechSynthesis_RtfTimeoutThreshold, "0.8");
        SetAsDefault(PropertyId::SpeechSynthesis_FrameTimeoutInterval, "1000");
        InitializeTelemetryManager();
    }
    else if (backend == "mock")
    {
        m_ttsAdapter = SpxCreateObjectWithSite<ISpxTtsEngineAdapter>("CSpxMockTtsEngineAdapter", this);
        m_backendName = "mock";
        SPX_THROW_HR_IF(SPXERR_EXTENSION_LIBRARY_NOT_FOUND, m_ttsAdapter == nullptr);
    }
    else
    {
        auto useRest = (backend == "rest");
        auto endpointUrl = Get(PropertyId::SpeechServiceConnection_Endpoint);
        auto hostUrl = Get(PropertyId::SpeechServiceConnection_Host);
        std::string endpoint;
        if (endpointUrl && !endpointUrl.Get().empty())
        {
            // Check for invalid use of auth token service endpoint
            bool isTokenServiceEndpoint = false;
            std::string endpointRegion;

            std::tie(isTokenServiceEndpoint, endpointRegion) =
                EndpointUtils::IsTokenServiceEndpoint(endpointUrl.Get());

            // If the endpoint is a token service endpoint, extract the region and set it as the region property.
            // Otherwise, set the endpoint to the provided value.
            if (isTokenServiceEndpoint)
            {
                SPX_TRACE_WARNING("SpeechServiceConnection_Endpoint is set to a token service endpoint. "
                    "We will extract the region from the endpoint and use it to construct the speech service endpoint.");
                SetAsDefault(PropertyId::SpeechServiceConnection_Region, endpointRegion.c_str());
                Set(PropertyId::SpeechServiceConnection_Endpoint, std::string());
            }
            else
            {
                endpoint = endpointUrl.Get();
            }
        }
        else if (hostUrl && !hostUrl.Get().empty())
        {
            endpoint = hostUrl.Get();
        }

        if (useRest)
        {
            m_ttsAdapter = SpxCreateObjectWithSite<ISpxTtsEngineAdapter>("CSpxRestTtsEngineAdapter", this);
            m_backendName = "online (REST)";
        }
        else
        {
            m_isUsp = true;
            m_ttsAdapter = SpxCreateObjectWithSite<ISpxTtsEngineAdapter>("CSpxUspTtsEngineAdapter", this);
            m_backendName = "online (websocket)";
        }
    }

    // if we still don't have an adapter... that's an exception
    SPX_THROW_HR_IF(SPXERR_NOT_FOUND, m_ttsAdapter == nullptr);
}

std::pair<std::shared_ptr<ISpxSynthesisResult>, std::string> CSpxSynthesizer::CreateResultFromCache(const std::string& requestId, const std::string& text, bool isSsml)
{
    if (!m_cache || !isSsml || m_backendName == "offline" || GetOr<bool>("SPEECH-SynthDisableCaching", false))
    {
        std::unique_lock<std::mutex> lock(m_wordBoundaryQueueMutex);
        if (!m_wordBoundaryQueue.empty())
        {
            m_wordBoundaryQueue.clear();
        }

        return std::make_pair(nullptr, "");
    }

    // Add timeout guard for cache access using async call
    const auto cacheTimeoutMs = GetOr<int>("SPEECH-ReadSynthCacheTimeoutMs", 2000); // 2 second default timeout

    // Execute cache call asynchronously with timeout
    auto cacheTask = std::async(std::launch::async, [this, &text]() {
        return m_cache->GetCache(text, m_format->outputFormatString, WordBoundary.IsConnected());
    });

    // Wait for cache call with timeout
    if (cacheTask.wait_for(std::chrono::milliseconds(cacheTimeoutMs)) == std::future_status::timeout)
    {
        SPX_TRACE_ERROR("%s: Cache access timed out after %d ms", __FUNCTION__, cacheTimeoutMs);
        return std::make_pair(nullptr, "");
    }

    const auto cacheData = cacheTask.get();
    if (!cacheData.data)
    {
        return std::make_pair(nullptr, "");
    }

    if (WordBoundary.IsConnected() && (cacheData.wordBoundaryData != nullptr))
    {
        auto fireWordBoundary = FireWordBoundaryFromCache(cacheData.wordBoundaryData);
        if (!fireWordBoundary)
        {
            return std::make_pair(nullptr, "");
        }
    }

    SetAdapterFormat(nullptr, m_format->outputFormat);
    auto cacheProperty = std::make_shared<std::map<std::string, std::string>>();
    cacheProperty->insert(std::make_pair<std::string, std::string>(GetPropertyName(PropertyId::SpeechServiceResponse_SynthesisBackend), "cache"));
    Write(nullptr, requestId, cacheData.data->data(), static_cast<uint32_t>(cacheData.data->size()), cacheProperty);
    EndOfTurn(nullptr);

    auto result = CreateEmptySynthesisResult();
    auto resultInit = SpxQueryInterface<ISpxSynthesisResultInit>(result);
    resultInit->InitSynthesisResult(requestId, ResultReason::SynthesizingAudioCompleted, nullptr);
    auto resultProperties = SpxQueryInterface<ISpxNamedProperties>(result);

    resultProperties->Set(PropertyId::SpeechServiceResponse_SynthesisBackend, "cache");
    return std::make_pair(result, cacheData.cacheKey);
}

std::string CSpxSynthesizer::CacheResult(const std::string& text, std::shared_ptr<ISpxSynthesisResult> result)
{
    if (!m_cache)
    {
        return "";
    }

    std::string cacheKey;
    const auto resultProperties = SpxQueryInterface<ISpxNamedProperties>(result);

    auto backend = resultProperties->Get(PropertyId::SpeechServiceResponse_SynthesisBackend);
    if (backend && backend.Get() == "online (websocket)")
    {
        auto wordBoundaryResult = FormatWordBoundaryForCache();
        cacheKey = m_cache->PutCache(text, m_format->outputFormatString, result->GetRawAudioData(), chrono::hours(m_cacheExpiredDays * 24), wordBoundaryResult);
        std::unique_lock<std::mutex> lock(m_wordBoundaryQueueMutex);
        m_wordBoundaryQueue.clear();
    }

    return cacheKey;
}

void CSpxSynthesizer::InitializeTelemetryManager()
{
    m_enableTelemetry = !GetOr<bool>("EmbeddedSpeech-DisableTelemetry", false);
    if (!m_enableTelemetry)
    {
        return;
    }

    m_telemetryManager = SpxCreateObjectWithSite<ISpxTelemetryManager>("CSpx1dsTelemetryManager", this);
    if (m_telemetryManager == nullptr)
    {
        return;
    }

    m_telemetryManager->Init(GetOr<std::string>("EmbeddedSpeech-TelemetryMode", "") == "UTC",
                             GetOr<std::string>("EmbeddedSpeech-TelemetryRegion", ""),
                             GetOr<double>("EmbeddedSpeech-TelemetrySamplingRatio", 1.0));
}

void CSpxSynthesizer::WaitForCurrentEventTriggered(uint64_t audioOffset) const
{
    if (m_eventsSyncToAudio && m_audioRender != nullptr && m_audioRender->IsRenderDeviceAvailable())
    {
        const auto audioOffsetMs = static_cast<int64_t>(audioOffset) / 10000;
        auto playbackTime = m_audioRender->GetPlaybackTime();
        // wait for the audio to be played
        while (playbackTime < 0)
        {
            if (!m_audioRender->IsRenderDeviceAvailable()) return;
            playbackTime = m_audioRender->GetPlaybackTime();
            SPX_TRACE_INFO("Waiting for audio to be played, playback time: %" PRIi64, playbackTime);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        constexpr auto margin = 100;
        if (audioOffsetMs > playbackTime + margin)
        {
            SPX_TRACE_INFO("Waiting for event triggered, offset: %" PRIi64 ", playback time: %" PRIi64, audioOffsetMs, playbackTime);
            std::this_thread::sleep_for(std::chrono::milliseconds(audioOffsetMs - playbackTime - margin));
        }
    }
}

void CSpxSynthesizer::FireWordBoundaryToInternalQueue(uint64_t audioOffset, uint64_t duration, uint32_t textOffset,
    uint32_t wordLength, std::string text,
    SpeechSynthesisBoundaryType boundaryType, const std::string& sourceBackend)
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    m_wordBoundaryQueue.emplace_back(WordBoundaryData{ sourceBackend, audioOffset, duration, textOffset, wordLength, text, boundaryType });
}

bool CSpxSynthesizer::ParseWordBoundaryFromCache(const std::shared_ptr<std::string>& wordBoundaryCache)
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    if (!wordBoundaryCache)
    {
        SPX_DBG_TRACE_ERROR("Word boundary cache is null.");
        return false;
    }

    auto wordBoundaryCacheJson = ajv::json::Parse(*wordBoundaryCache);
    if (!wordBoundaryCacheJson[word_boundary_property::wordBoundaryItems].IsContainer()
        || !wordBoundaryCacheJson[word_boundary_property::wordBoundaryItems].IsArray())
    {
        SPX_DBG_TRACE_ERROR("Word boundary cache is not a valid json array.");
        return false;
    }

    std::unique_lock<std::mutex> lock(m_wordBoundaryQueueMutex);
    if (!m_wordBoundaryQueue.empty())
    {
        m_wordBoundaryQueue.clear();
    }

    WordBoundaryData dataItem;
    for (int index = 0; index < wordBoundaryCacheJson[word_boundary_property::wordBoundaryItems].ValueCount(); index++)
    {
        if (!wordBoundaryCacheJson[word_boundary_property::wordBoundaryItems][index][word_boundary_property::audioOffset].IsNumber()
            || !wordBoundaryCacheJson[word_boundary_property::wordBoundaryItems][index][word_boundary_property::duration].IsNumber()
            || !wordBoundaryCacheJson[word_boundary_property::wordBoundaryItems][index][word_boundary_property::textOffset].IsNumber()
            || !wordBoundaryCacheJson[word_boundary_property::wordBoundaryItems][index][word_boundary_property::wordLength].IsNumber()
            || !wordBoundaryCacheJson[word_boundary_property::wordBoundaryItems][index][word_boundary_property::text].IsString()
            || !wordBoundaryCacheJson[word_boundary_property::wordBoundaryItems][index][word_boundary_property::boundaryType].IsNumber())
        {
            if (!m_wordBoundaryQueue.empty())
            {
                m_wordBoundaryQueue.clear();
            }

            SPX_DBG_TRACE_ERROR("Word boundary cache can not be parsed, word boundary: %s", wordBoundaryCache->c_str());
            return false;
        }

        dataItem.audioOffset = wordBoundaryCacheJson[word_boundary_property::wordBoundaryItems][index][word_boundary_property::audioOffset].AsUint64();
        dataItem.duration = wordBoundaryCacheJson[word_boundary_property::wordBoundaryItems][index][word_boundary_property::duration].AsUint64();
        dataItem.textOffset = wordBoundaryCacheJson[word_boundary_property::wordBoundaryItems][index][word_boundary_property::textOffset].AsUint();
        dataItem.wordLength = wordBoundaryCacheJson[word_boundary_property::wordBoundaryItems][index][word_boundary_property::wordLength].AsUint();
        dataItem.text = wordBoundaryCacheJson[word_boundary_property::wordBoundaryItems][index][word_boundary_property::text].AsString();
        dataItem.boundaryType = static_cast<SpeechSynthesisBoundaryType>(wordBoundaryCacheJson[word_boundary_property::wordBoundaryItems][index][word_boundary_property::boundaryType].AsInt());

        m_wordBoundaryQueue.emplace_back(dataItem);
    }

    return true;
}

bool CSpxSynthesizer::FireWordBoundaryFromCache(const std::shared_ptr<std::string>& wordBoundaryCache)
{
    if (!wordBoundaryCache)
    {
        return false;
    }

    if (!ParseWordBoundaryFromCache(wordBoundaryCache))
    {
        return false;
    }

    std::unique_lock<std::mutex> lock(m_wordBoundaryQueueMutex);
    while (!m_wordBoundaryQueue.empty())
    {
        auto& front = m_wordBoundaryQueue.front();
        FireEvent(EventType::WordBoundaryEvent, nullptr, true, front.audioOffset, front.duration, front.textOffset, front.wordLength, front.text, front.boundaryType);
        m_wordBoundaryQueue.pop_front();
    }

    return true;
}

void CSpxSynthesizer::EnsureValidToken()
{
    auto expiry = GetOr("service.auth.token.expirems", "");
    if (expiry == "infinite") {
        SPX_TRACE_INFO("Requested token synchronously.");
        auto task = GetTokenRefreshTask();
        RunSyncOnThreadService(
            m_syncCallbacksThreadService,
            std::move(task),
            ISpxThreadService::Affinity::User);
    }
}

void CSpxSynthesizer::RefreshToken()
{
    // Note: token refresh is not USP-specific. Both the WebSocket (USP) and REST
    // backends consume SpeechServiceConnection_Authorization_Token and rely on this
    // bridge to TokenCredential to populate/refresh the token before a request is sent.

    SPX_TRACE_FUNCTION();
    std::wstring sessionIdOverride;
    if (m_isDisposing)
    {
        SPX_DBG_TRACE_VERBOSE("[%p]CSpxSynthesizer::RefreshToken, recognizer is disposing, ignore events", (void*)this);
        return;
    }

    weak_ptr<ISpxSynthesizer> weakSynthesizer = SpxSharedPtrFromThis<ISpxSynthesizer>(this);
    const auto currentRequestId = GetFrontRequestId();
    auto eventsSyncLatch = m_eventsSyncLatch;
    SPX_TRACE_VERBOSE("Fired token request event");
    DispatchEvent(weakSynthesizer, EventType::TokenRequestEvent, std::move(currentRequestId), nullptr, true, 0, 0, 0, 0, std::string{}, SpeechSynthesisBoundaryType::Word, eventsSyncLatch);
}

void CSpxSynthesizer::SetStringValue(const char* name, const char* value)
{
    ISpxPropertyBagImpl::SetStringValue(name, value);
    if (name != nullptr && value != nullptr && PAL::stricmp(name, "service.auth.token.expirems") == 0 && PAL::stricmp(value, "infinite") != 0)
    {
        ScheduleTokenRefresh();
    }
}

void CSpxSynthesizer::ScheduleTokenRefresh()
{
    // Note: token refresh is not USP-specific. Both the WebSocket (USP) and REST
    // backends consume SpeechServiceConnection_Authorization_Token and rely on this
    // bridge to TokenCredential to keep the token valid across requests.

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
    SPX_TRACE_VERBOSE("Scheduling token refresh in %" PRId64  "ms", delay);
    m_threadService->ExecuteAsync(std::move(task), std::chrono::milliseconds(delay), ISpxThreadService::Affinity::User);
}

std::packaged_task<void()> CSpxSynthesizer::GetTokenRefreshTask()
{
    // Get the token to refresh
    auto token = GetOr(PropertyId::SpeechServiceAuthorization_Token, "");

    weak_ptr<ISpxSynthesizer> weakSynthesizer = SpxSharedPtrFromThis<ISpxSynthesizer>(this);
    std::packaged_task<void()> task([weakPtr = std::move(weakSynthesizer), token = std::move(token)]()-> void
        {
            SPX_TRACE_SCOPE("Background refresh of auth token", "Refresh Complete");

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

void CSpxSynthesizer::SetBackendName(const std::string& backend)
{
    const auto currentRequestId = GetFrontRequestId();
    std::lock_guard<std::mutex> lock(m_wordBoundaryQueueMutex);
    if (!currentRequestId.empty())
    {
        m_backendNameByRequestId[currentRequestId] = backend;
    }
    else
    {
        m_backendName = backend;
    }
}

std::string CSpxSynthesizer::GetBackendName()
{
    const auto currentRequestId = GetFrontRequestId();
    std::lock_guard<std::mutex> lock(m_wordBoundaryQueueMutex);
    if (!currentRequestId.empty())
    {
        const auto it = m_backendNameByRequestId.find(currentRequestId);
        if (it != m_backendNameByRequestId.end())
        {
            return it->second;
        }
    }
    return m_backendName;
}


} } } } // Microsoft::CognitiveServices::Speech::Impl
