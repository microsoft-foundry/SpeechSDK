//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include <chrono>
#include <named_properties.h>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

/*
    Why did we pick the configuration below?

    Design principles:
    1. The speech service segments (returns reco result or no-match) after processing ~30 seconds of audio or less, in continuous or one-shot reco. The max
       segmentation time I saw in tests was 32 when you have long speech without any pauses, so picking 33 just in case.
    2. The speech service can normally do recognition faster than real-time, for extended durations. We pick twice real-time
       as the maximum rate the client will send audio to the service, if the audio source supports it (e.g. reading from WAV file or input stream).
    3. We want to build a throttling mechanism in the client to slow down the rate when needed, when we see the service falls behind. We assume
       the service can always operate at x1 real-time.
    4. In addition to the above, we want to send the first 5 seconds of audio as fast as possible to speed up getting the first reco result. This is what we call "fast lane".
    5. We need an upper limit to the unacknowledged audio the client stores, otherwise it may grow unbounded if the service does not respond.
       When this upper limit exceeds, Carbon will fire a cancellation event.

    Configuration settings:
    We assume the service can drop at any time to x1 real-time, and the client needs to adapt to make sure recognition continues without
    unacknowledged audio exceeding the max limit. We will assume worse-case scenario where there are no pauses in the speech so segmentation
    will happen at a max of 33 seconds. If we are sending audio at x2 real-time, and the service is able to process it at x2 real-time, and we
    get reco results after processing 33 seconds of audio, it means the unacknowledged audio in the client at any time should not exceed 33 seconds.
    If however, the service drops to processing audio less than x2 real-time, the unacknowledged audio in the client may exceed 33 seconds, and
    go up to 66 seconds. Therefore we pick the 33 second unacknowledged audio (= 50% of 66) as the threshold to drop down to sending at x1 real-time instead of
    x2 real-time, and 66 seconds unacknowledged audio as the max. If the service cannot process at x1 real-time, the unacknowledged audio in the client
    eventually will exceed 66 seconds and we will fire the cancellation event.
    The switch back to x2 rate should be when there is very little unacknowledged audio, to again make sure that if we send at x2 real-time and
    the service can only operate at x1 real-time, we will not exceed the 66 second unacknowledged audio. We set it at ~6 seconds (= 9% of 66).

    How do we test throttling?
    - Functional end-to-end test: See "Audio stream session throttle test" in speech_recognizer_tests.cpp
    - Unit test of this class: See "AudioStreamSessionThrottleLogic" in audio_stream_session_throttle_logic_test.cpp

    Notes:
    - The #define values below are only used by the AudioStreamSessionThrottleLogic class, therefore could have been
    places in the CPP files (audio_stream_session_throttle_logic.cpp). However they are placed in the header file
    so the unit-test can use them.
    - Special cases for Conversation Transcriber: For some reason there was a larger max buffer value of 240 seconds set for
      Conversation Transcriber scenarios. Perhaps it was observed that the service is slower to response. I don't know if this is
      still true. I'm keeping this value with this new throttling logic in place. Conversation Transcriber has two modes,
      "RealTimeAndAsync" and "async". The first one behaves similar to regular recognizer (we get speech.phrase responses from the
      service with recognition results in real-time). When you use the "async" mode, you upload audio and never get speech.phrase
      responses from the service, since speech recognition is not done in real-time. Therefore we need to let the amount of unacknowledged
      audio increase unbounded (poor design!).
*/

// We will always try to process audio at twice "real-time" speed, if the input audio source can support it (e.g. reading
// from a WAV file or input stream). This will be the initial transmission rate. 
// Can be overwritten by property "SPEECH-AudioThrottleAsPercentageOfRealTime".
static constexpr unsigned long c_audioThrottleHighPercentageRealtime = 200;
static constexpr unsigned long c_conversationTranscriberV2ThrottleHighPercentageRealtime = 200;
static_assert(c_audioThrottleHighPercentageRealtime >= 100, "Should be faster than real-time");

// Embedded SR can process input as fast as the device allows, therefore use a
// higher default limit. This also helps in catching up with buffered real-time
// input audio in case SR falls behind due to (temporary, short) high CPU usage
// by some other process.
// Can be overwritten by property "SPEECH-AudioThrottleAsPercentageOfRealTime".
static constexpr unsigned long c_audioThrottleHighPercentageRealtimeEmbedded = 800;
static_assert(c_audioThrottleHighPercentageRealtimeEmbedded >= 100, "Should be faster than real-time");

// VAD Gating with current model will always try to process audio at twice "real-time" speed.
// And it doesn't support speed exceed twice real-time for now. VAD adapter is required to process faster than decoding engine.
// Can be overwritten by property "SPEECH-AudioThrottleAsPercentageOfRealTime".
static constexpr unsigned long c_audioThrottleHighPercentageRealtimeVAD = 200;
static_assert(c_audioThrottleHighPercentageRealtimeVAD >= 100, "Should be faster than real-time");
static_assert(c_audioThrottleHighPercentageRealtimeVAD <= 800, "Should not exceed twice real-time");

// When the service is not fast enough and the unacknowledged audio buffer duration exceeds a threshold, we slow down to this transmission rate.
// Can be overwritten by property "SPEECH-AudioThrottleLowAsPercentageOfRealTime".
static constexpr unsigned long c_audioThrottleLowPercentageRealtime = 100;
static_assert(c_audioThrottleLowPercentageRealtime < c_audioThrottleHighPercentageRealtime, "Low rate should be lower than high rate");
static_assert(c_audioThrottleLowPercentageRealtime <= 100, "Should be at or slower than real-time");

// At the start of the session, we will try to process this duration of audio as fast as possible. Only after this duration of
// audio was processed, we will throttle back to the high rate set above.
// Set to a value of 0 to disable this, and start throttling right away.
// Can be overwritten by property "SPEECH-TransmitLengthBeforeThrottleMs" (note this property is in msec, not seconds).
static constexpr unsigned long c_maxTransmittedInFastLaneMs = 5000;
static_assert(c_maxTransmittedInFastLaneMs >= 0, "Should be non-negative");

// This is the maximum amount of unacknowledged audio we want to allow in the client, for most scenarios. The speech
// service currently has the upper bound of ~30 seconds to generate a speech segment. To account for sending at x2 real-time
// to a service that is overloaded and can only recognize at x1 real-time, assuming no-pause speech (max segmentation time), we
// want this max value to be twice the max segmentation time. Hence the 66 second value.
// Can be overwritten by setting property "SPEECH-MaxBufferSizeMs".
static constexpr unsigned long c_maxBufferSizeMs = 66000;
static_assert(c_maxBufferSizeMs > 0, "Should be positive");

// This is the maximum amount of unacknowledged audio we want to allow for ConversationTranscriber, as the service may be slower at times.
// Can be overwritten by setting property "SPEECH-MaxBufferSizeMs".
// To do: Is there still a need to have a unique value for ConversationTranscriber different than c_maxBufferSizeMs?
static constexpr unsigned long c_maxBufferSizeMsConversationTranscriber = 240000;
static_assert(c_maxBufferSizeMsConversationTranscriber > 0, "Should be positive");

// When the unacknowledged audio duration exceeds this percent of its max capacity (defined by maxBufferSizeSeconds above), switch to transmitting at a lower rate.
// Setting a value of 100 means we will never switch to the lower rate.
// Can be overwritten by setting property "SPEECH-BufferSizePercentSwitchToLowRate".
static constexpr unsigned long c_bufferSizePercentSwitchToLowRate = 50;
static_assert(c_bufferSizePercentSwitchToLowRate <= 100 && 0 < c_bufferSizePercentSwitchToLowRate, "Should not be larger than 100");

// If there was a switch to lower rate per the above, and the unacknowledged audio duration is now back below this percent of its max capacity,
// switch back to transmitting at the higher rate.
// Setting a value of 0 means we will never switch back to the higher rate.
// Can be overwritten by setting property "SPEECH-BufferSizePercentSwitchToHighRate".
static constexpr unsigned long c_bufferSizePercentSwitchToHighRate = 9;
static_assert(c_bufferSizePercentSwitchToHighRate < c_bufferSizePercentSwitchToLowRate, "Make sure thresholds are ordered correctly");
static_assert(c_bufferSizePercentSwitchToHighRate >= 0, "Should not be negative");


class AudioStreamSessionThrottleLogic
{
public:
    AudioStreamSessionThrottleLogic(const ISpxNamedProperties* properties, uint32_t avgBytesPerSecond, bool isUsingRecoEngineRnnt, bool isUsingVADGating);
    std::chrono::milliseconds GetMaxDuration();
    uint32_t GetAverageBytesPerSecond();
    std::chrono::milliseconds GetPacketAudioDelay(uint64_t m_bytesTransited, uint32_t packetSizeInBytes, std::chrono::milliseconds unacknowledgedAudioDuration);

private:
    uint32_t m_avgBytesPerSecond;

    std::chrono::milliseconds m_fastLaneDuration;
    uint64_t m_fastLaneBytes;

    std::chrono::milliseconds m_maxDuration;
    std::chrono::milliseconds m_highDuration;
    std::chrono::milliseconds m_lowDuration;

    unsigned long m_highRate;
    unsigned long m_lowRate;
    unsigned long m_currentRate;
};

using AudioStreamSessionThrottleLogicPtr = std::unique_ptr<AudioStreamSessionThrottleLogic>;

}}}} // Microsoft::CognitiveServices::Speech::Impl
