//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#pragma once

#include "speechapi_cxx_audio_processing_options.h"

#include "interfaces/base.h"
#include "interfaces/aggregates.h"
#include "interfaces/containers.h"
#include "interfaces/errors.h"
#include "interfaces/utils.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

/* TODO: Streams probably belong in a streams.h */
SPX_INTERFACE(ISpxAudioStream)
{
    public:
    virtual uint16_t GetFormat(SPXWAVEFORMATEX* pformat, uint16_t cbFormat) = 0;
};

SPX_INTERFACE(ISpxAudioStreamInitFormat)
{
    public:
    virtual void SetFormat(const SPXWAVEFORMATEX* format) = 0;
};

SPX_INTERFACE(ISpxAudioProcessor)
{
    public:

    virtual void SetFormat(const SPXWAVEFORMATEX* pformat) = 0;
    virtual void ProcessAudio(const DataChunkPtr& audioChunk) = 0;

    // Inline commit: forward a commit-marker delivery. Called by
    // the audio pump immediately after the ProcessAudio call whose bytes
    // reached the commit point. token identifies the commit; offsetBytes
    // is the marker's stored byte position, converted to ticks by the
    // session via its wave format. hasChannel indicates whether the
    // commit is scoped to a specific channel.
    // Default: no-op for processors that are not commit-aware. Note that
    // a processor sitting between the pump and the session (e.g.
    // CSpxAudioProcessorWriteToAudioSourceBuffer) must override this to
    // forward the marker onward, or the commit is silently dropped.
    virtual void ProcessCommit(uint32_t /*token*/, uint64_t /*offsetBytes*/, bool /*hasChannel*/, uint32_t /*channelId*/) {}
};

SPX_INTERFACE(ISpxAudioProcessorMinInput)
{
    public:
    virtual void SetMinInputSize(const uint64_t sizeInTicks) = 0;
};

SPX_INTERFACE(ISpxAudioSource)
{
    public:

    enum class State { Idle = 0, Started = 1, DataAvailable = 2, EndOfStream = 3 };
    virtual State GetState() const = 0;
    virtual SpxWaveFormatEx GetFormat() const = 0;
};

SPX_INTERFACE(ISpxAudioSourceInit)
{
    public:

    virtual void InitFromMicrophone() = 0;
    virtual void InitFromFile(const char * fileName) = 0;
    virtual void InitFromStream(std::shared_ptr<ISpxAudioStream> stream) = 0;
};

SPX_INTERFACE(ISpxAudioSessionShim)
{
    public:
    virtual void StartAudio() = 0;
    virtual void StopAudio() = 0;
    virtual SpxWaveFormatEx GetFormat() = 0;

    // Inline commit: forward a commit marker to the session's audio
    // processor. The audio pump delivers audio to the session indirectly,
    // by writing bytes into the audio-source buffer and notifying this
    // shim, which reads them back out and calls ProcessAudio on the
    // session (see AudioSourceDataAvailable). A commit marker carries no
    // bytes, so it cannot travel through that buffer; it is forwarded
    // along this parallel path instead, preserving the ordering
    // guarantee because both hops run synchronously on the pump thread.
    virtual void ProcessCommit(uint32_t token, uint64_t offsetBytes, bool hasChannel, uint32_t channelId) = 0;
};

class ISpxSynthesisResult;

SPX_INTERFACE(ISpxAudioDataStream)
{
    public:
    virtual StreamStatus GetStatus() const noexcept = 0;
    virtual void SetStatus(StreamStatus status) noexcept = 0;
    virtual CancellationReason GetCancellationReason() const noexcept = 0;
    virtual std::shared_ptr<ISpxErrorInformation> GetError() = 0;
    virtual void SetError(const std::shared_ptr<ISpxErrorInformation> &error) = 0;
    virtual bool CanReadData(uint32_t requestedSize) = 0;
    virtual bool CanReadData(uint32_t requestedSize, uint32_t pos) = 0;
    virtual uint32_t Read(uint8_t* buffer, uint32_t bufferSize) = 0;
    virtual uint32_t Read(uint8_t* buffer, uint32_t bufferSize, uint32_t pos) = 0;
    virtual void SaveToWaveFile(const char * fileName) = 0;
    virtual uint32_t GetPosition() = 0;
    virtual void SetPosition(uint32_t pos) = 0;
    virtual uint32_t GetAvailableSize() = 0;
};

SPX_INTERFACE(ISpxAudioDataStreamInit)
{
    public:
    virtual void InitFromFile(const char* fileName) = 0;
    virtual void InitFromFormat(const SPXWAVEFORMATEX* format, bool hasHeader) = 0;
};

SPX_INTERFACE(ISpxAudioDataStreamWrapper)
{
    public:
    virtual void DetachInput() = 0;
};

SPX_INTERFACE(ISpxAudioDataStreamSharedAdapterInit)
{
    public:
    virtual void InitFromAudioDataStream(const std::shared_ptr<ISpxAudioDataStream> &stream) = 0;
};

SPX_INTERFACE(ISpxAudioCodecAdapter)
{
    public:
    /// <summary>
    /// Enable throttling of the decoding process, to lower the peak CPU usage.
    /// Default is false.
    /// </summary>
    virtual void EnableThrottling(bool enableThrottling) = 0;
    virtual void SetSourceFormat(SPXWAVEFORMATEX* format) = 0;
};

SPX_INTERFACE(ISpxAudioProcessingOptions)
{
public:
    enum class ModelType { EchoCancellation, Vad, Pns };

    virtual void InitWithProcessingFlags(int audioProcessingFlags) = 0;
    virtual void InitWithPresetMicrophoneArrayGeometry(int audioProcessingFlags, Audio::PresetMicrophoneArrayGeometry microphoneArrayGeometry, Audio::SpeakerReferenceChannel speakerReferenceChannel) = 0;
    virtual void InitWithMicrophoneArrayGeometry(int audioProcessingFlags, Audio::MicrophoneArrayGeometry microphoneArrayGeometry, Audio::SpeakerReferenceChannel speakerReferenceChannel) = 0;
    virtual void InitFromJson(const std::string& audioProcessingOptionsJson) = 0;

    virtual int GetAudioProcessingFlags() = 0;
    virtual Audio::PresetMicrophoneArrayGeometry GetPresetMicrophoneArrayGeometry() = 0;
    virtual Audio::MicrophoneArrayType GetMicrophoneArrayType() = 0;
    virtual uint16_t GetBeamformingStartAngle() = 0;
    virtual uint16_t GetBeamformingEndAngle() = 0;
    virtual uint16_t GetMicrophoneCount() = 0;
    virtual std::vector<Audio::MicrophoneCoordinates> GetMicrophoneCoordinates() = 0;
    virtual Audio::SpeakerReferenceChannel GetSpeakerReferenceChannel() = 0;
    virtual std::string GetModelPath(ModelType modelType) = 0;
    virtual void SetSpeakerSignature(const std::vector<float>& signature) = 0;
    virtual std::vector<float> GetSpeakerSignature() = 0;

    virtual std::string ToJson() = 0;
};

} } } }
