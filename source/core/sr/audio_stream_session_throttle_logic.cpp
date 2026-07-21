//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#include "stdafx.h"
#include <algorithm>
#include "audio_stream_session_throttle_logic.h"
#include "property_id_2_name_map.h"
#include "buffer_helpers.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

using namespace std::chrono;

//
// Initialize variables related to the logic that determines how fast we process the audio. In general
// this includes a start-up time ("fast-lane" duration) where we process audio as fast as possible (if available). Then
// switch to a high rate (faster than real-time) if possible. From this point on we switch between a high and low rate
// depending on how much unacknowledged audio we have in the client, to make sure the service does not fall too much behind.
//
// Note this class has a unit test. See \tests\unit-tests\audio_stream_session_throttle_logit_test.cpp
//
// properties - Properties interface on the speech config to get optional overrides to the defaults defined above.
// avgBytesPerSecond - of the audio stream. Usually 32000 for 16khz mono 16bit/sample. But can be 8-ch for Conversation Transcriber.
// isUsingRecoEngineRnnt - True for embedded SR, since for this case we need to disable fast-lane.
//
AudioStreamSessionThrottleLogic::AudioStreamSessionThrottleLogic(const ISpxNamedProperties* properties, uint32_t avgBytesPerSecond, bool isUsingRecoEngineRnnt, bool isUsingVADGating) :
    m_avgBytesPerSecond(0),
    m_fastLaneDuration(milliseconds(0)),
    m_fastLaneBytes(0),
    m_maxDuration(milliseconds(0)),
    m_highDuration(milliseconds(0)),
    m_lowDuration(milliseconds(0)),
    m_highRate(0),
    m_lowRate(0),
    m_currentRate(0)
{
    SPX_THROW_HR_IF(SPXERR_INVALID_ARG, avgBytesPerSecond == 0 || properties == nullptr);

    // If the misspelled (old) value is set and the new (correct) one is not, use the old. Else use the new.
    const char* throttleFastLaneName = (properties->HasStringValue("SPEECH-TransmitLengthBeforThrottleMs") &&
        !properties->HasStringValue("SPEECH-TransmitLengthBeforeThrottleMs")) ? "SPEECH-TransmitLengthBeforThrottleMs" : "SPEECH-TransmitLengthBeforeThrottleMs";
    m_fastLaneDuration = milliseconds(properties->GetOr<int>(throttleFastLaneName, c_maxTransmittedInFastLaneMs));

    // Some first party async Conversation Transcription clients set a -1 value using SPEECH-TransmitLengthBeforeThrottleMs property, with the intention of sending
    // audio up as fast as possible all the time. In other words, the fast-lane duration should be larger than the audio stream duration. Set some high value here to make
    // sure that's true.
    if (m_fastLaneDuration.count() == -1)
    {
        m_fastLaneDuration = milliseconds(0xFFFFFFFF); // == ~1193 hours
    }

    m_avgBytesPerSecond = avgBytesPerSecond;
    m_fastLaneBytes = avgBytesPerSecond * m_fastLaneDuration.count() / 1000;

    // Set to true whenever the API JoinConversationAsync is called, which is required for Conversation Transcriber scenarios
    bool isConversationTranscriber = properties->GetOr<bool>(g_isConversationTranscriber, false);
    bool isMeetingTranscriber = properties->GetOr<bool>(g_isMeetingTranscriber, false);
    bool isConversationTranscriberV2 = properties->GetOr<bool>(g_isConversationTranscriber_V2, false);
    bool isAsyncConversationTranscriber = false;

    if (isConversationTranscriber || isMeetingTranscriber)
    {
        // It is publicly documented that an application should call:
        //       speechConfig.SetServiceProperty("transcriptionMode", "async", ServicePropertyChannel.UriQueryParameter);
        // to use the async mode of Conversation Transcriber. In this mode the service does not send back any speech.phrase responses, hence audio
        // will not be acknowledged and the client will keep storing more and more audio. Until there is a better solution for this, we need to disable
        // the overflow check.
        // Example URL: wss://transcribe.centralus.cts.speech.microsoft.com/speech/recognition/multiaudio?language=en-us&transcriptionMode=async
        std::string queryParameters = PAL::StringUtils::ToLower(properties->GetOr<std::string>(PropertyId::SpeechServiceConnection_UserDefinedQueryParameters, ""));
        isAsyncConversationTranscriber = (std::string::npos != queryParameters.find("transcriptionmode=async"));
    }

    // Maximum unacknowledged audio duration we want to allow in the client. For async Conversation Transcriber mode we have no limit, therefore it is set to
    // some very high value 0xFFFFFFFF (~ 1193 hours. This also results in no throttling down). For other Conversation Transcriber modes we have a limit that is a bit
    // larger than the limit for all other scenarios. For time segmentation strategy, reserve 2.2 times the maximum duration of the segmentation time to prevent the buffer from exceeding its capacity.
    // To do: is this still needed?
    const unsigned long selectedMaxBufferSizeMs = isAsyncConversationTranscriber ? 0xFFFFFFFF : ((isConversationTranscriber || isMeetingTranscriber) ? c_maxBufferSizeMsConversationTranscriber : c_maxBufferSizeMs);
    const unsigned long segmentationMaximumTimeMs = stoul(properties->GetStringValue("SPEECH-SegmentationMaximumTimeMs", "0"));
    m_maxDuration = milliseconds(std::max(static_cast<unsigned long>(segmentationMaximumTimeMs * 2.2), stoul(properties->GetStringValue("SPEECH-MaxBufferSizeMs", std::to_string(selectedMaxBufferSizeMs).c_str()))));

    // When we buffer this duration or more of unacknowledged audio in the client, we switch to lower rate. A value of 100 means we will never switch to lower rate.
    // Therefore if for some reason you need to disable throttling, have the app set this property to 100.
    int percent = stoi(properties->GetStringValue("SPEECH-BufferSizePercentSwitchToLowRate", std::to_string(c_bufferSizePercentSwitchToLowRate).c_str()));
    m_highDuration = milliseconds(m_maxDuration.count() * percent / 100);

    // If we were running at lower rate, and we got down to this duration of unacknowledged audio in the client, we will switch back to high rate
    percent = stoi(properties->GetStringValue("SPEECH-BufferSizePercentSwitchToHighRate", std::to_string(c_bufferSizePercentSwitchToHighRate).c_str()));
    m_lowDuration = milliseconds(m_maxDuration.count() * percent / 100);

    // These define the high and low rates, as percent of real-time (A value of 100 being process at real-time. Higher values mean faster than real time)
    // If it is VAD Gating, always use c_audioThrottleHighPercentageRealtimeVAD to make sure gating correctly.
    m_lowRate  = properties->GetOr<unsigned long>("SPEECH-AudioThrottleLowAsPercentageOfRealTime", c_audioThrottleLowPercentageRealtime);
    const unsigned long selectedHighRatePercentage =
        isConversationTranscriberV2 ? c_conversationTranscriberV2ThrottleHighPercentageRealtime :
        isUsingVADGating ? c_audioThrottleHighPercentageRealtimeVAD :
        (isUsingRecoEngineRnnt ? c_audioThrottleHighPercentageRealtimeEmbedded : c_audioThrottleHighPercentageRealtime);
    m_highRate = properties->GetOr<unsigned long>("SPEECH-AudioThrottleAsPercentageOfRealTime", selectedHighRatePercentage);

    // After we exist fast lane, we want to process audio at the high rate.
    m_currentRate = m_highRate;

    SPX_DBG_TRACE_VERBOSE("[%p] Is VAD Gating = %d, Is RNNT reco engine = %d, Is conversation transcriber = %d, Is meeting transcriber = %d, Is async transcriber  = %d, fastLane = %" PRIu64 " msec (%" PRIu64 " bytes), maxDuration = %" PRIu64 " msec, highDuration = %" PRIu64 " msec, lowDuration = %" PRIu64 " msec, lowRate = %lu%%, highRate = %lu%%",
        (void*)this, isUsingVADGating, isUsingRecoEngineRnnt, isConversationTranscriber, isMeetingTranscriber, isAsyncConversationTranscriber, static_cast<uint64_t>(m_fastLaneDuration.count()), m_fastLaneBytes, static_cast<uint64_t>(m_maxDuration.count()), static_cast<uint64_t>(m_highDuration.count()), static_cast<uint64_t>(m_lowDuration.count()), m_lowRate, m_highRate);

    // Some sanity checks...
    SPX_THROW_HR_IF(SPXERR_INVALID_ARG, m_maxDuration < m_highDuration);
    SPX_THROW_HR_IF(SPXERR_INVALID_ARG, m_highDuration < m_lowDuration);
    SPX_THROW_HR_IF(SPXERR_INVALID_ARG, m_lowDuration < milliseconds(0));
    SPX_THROW_HR_IF(SPXERR_INVALID_ARG, m_highRate < 100);
    SPX_THROW_HR_IF(SPXERR_INVALID_ARG, m_highRate < m_lowRate);
    SPX_THROW_HR_IF(SPXERR_INVALID_ARG, m_lowRate <= 0);
}

milliseconds AudioStreamSessionThrottleLogic::GetMaxDuration()
{
    return m_maxDuration;
}

uint32_t AudioStreamSessionThrottleLogic::GetAverageBytesPerSecond()
{
    return m_avgBytesPerSecond;
}

//
// Return the pause duration in msec that the audio session thread should use before the next processing loop.
//
// m_bytesTransited - The total number of audio bytes sent to the cloud since the start of the stream (always increasing).
// packetSizeInBytes - The size size in bytes of the current package of audio.
// unacknowledgedAudioDuration - The current duration of unacknowledged audio on the client.
//
milliseconds AudioStreamSessionThrottleLogic::GetPacketAudioDelay(uint64_t bytesTransited, uint32_t packetSizeInBytes, milliseconds unacknowledgedAudioDuration)
{
    // By default, we process audio as fast as possible. This will happen in the first maxSecondsTransmittedInFastLane.
    milliseconds packetAudioDelay = milliseconds(0);

    // If we are out of the fast lane, set the rate based on how much unacknowledged audio duration we have in the buffer. Slow down
    // if needed to make sure the service does not fall too much behind
    if (bytesTransited >= m_fastLaneBytes)
    {
        if (m_highRate != m_lowRate)
        {
            if (m_currentRate == m_highRate && unacknowledgedAudioDuration >= m_highDuration && m_maxDuration > m_highDuration)
            {
                SPX_DBG_TRACE_VERBOSE("Slowing down to low rate (%lu%%). unacknowledgedAudioDuration = %" PRIu64 " msec", m_lowRate, static_cast<uint64_t>(unacknowledgedAudioDuration.count()));
                m_currentRate = m_lowRate;
            }
            else if (m_currentRate == m_lowRate && unacknowledgedAudioDuration <= m_lowDuration)
            {
                SPX_DBG_TRACE_VERBOSE("Speeding up to high rate (%lu%%). unacknowledgedAudioDuration = %" PRIu64 " msec", m_highRate, static_cast<uint64_t>(unacknowledgedAudioDuration.count()));
                m_currentRate = m_highRate;
            }
        }

        // The amount of time we should ensure current audio packet takes to process.
        // This calculates the duration in msec associated with dataSize bytes of input audio, after adjusting for non real-time rate. So
        // for example if the rate is 100% (= real-time), then this duration corresponds to the actual audio duration of dataSize bytes of audio.
        // If the rate is 200% (= twice as fast as real-time), this duration is half of the previous value, which means we try to
        // process audio at twice the cadence.
        packetAudioDelay = BytesToDuration<milliseconds>(packetSizeInBytes, static_cast<uint32_t>(m_avgBytesPerSecond * m_currentRate / 100));

    }

    SPX_DBG_TRACE_VERBOSE("[%p] m_bytesTransited = %" PRIu64 ", unacknowledgedAudioDuration = %" PRIu64 " msec, packetAudioDelay = %" PRIu64 " msec",
        (void*)this, bytesTransited, static_cast<uint64_t>(unacknowledgedAudioDuration.count()), static_cast<uint64_t>(packetAudioDelay.count()));

    return packetAudioDelay;
}

}}}} // Microsoft::CognitiveServices::Speech::Impl
