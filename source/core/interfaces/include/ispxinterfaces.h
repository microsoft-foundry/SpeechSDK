//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// ISpxInterfaces.h: Implementation declarations for all ISpx* C++ interface classes
//
#pragma once

#include <memory>
#include <vector>
#include <map>
#include <chrono>
#include "spxcore_common.h"
#include "platform.h"
#include "asyncop.h"
#include "speechapi_cxx_eventsignalbase.h"
#include "speechapi_cxx_enums.h"
#include "speechapi_cxx_string_helpers.h"
#include "shared_ptr_helpers.h"
#include "spxdebug.h"
#include <cstring>
#include "task_helpers.h"

#include <interfaces/aggregates.h>
#include <interfaces/audio.h>
#include <interfaces/base.h>
#include <interfaces/containers.h>
#include <interfaces/conversation.h>
#include <interfaces/meeting.h>
#include <interfaces/data.h>
#include <interfaces/errors.h>
#include <interfaces/event_args.h>
#include <interfaces/http_event_args.h>
#include <interfaces/keyword.h>
#include <interfaces/notify_me.h>
#include <interfaces/recognizers.h>
#include <interfaces/results.h>
#include <interfaces/thread_service.h>
#include <interfaces/types.h>
#include <interfaces/service_provider.h>
#include <interfaces/utils.h>
#include "interfaces/named_properties.h"
#include "interfaces/audio_output.h"
#include "interfaces/synthesis_request.h"

#include "interfaces/object_factory.h"
#include "interfaces/object_init.h"
#include "interfaces/object_with_site.h"
#include "interfaces/generic_site.h"
#include "interfaces/types.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

SPX_INTERFACE(ISpxAddServiceProvider)
{
    public:
    template <class T>
    void AddService(std::shared_ptr<T> service)
    {
        AddService(Type<T>::Id, service);
    }

    virtual void AddService(uint64_t serviceTypeId, std::shared_ptr<ISpxInterfaceBase> service) = 0;
};

using SpxWAVEFORMATEX_Type = std::shared_ptr<SPXWAVEFORMATEX>;
inline SpxWAVEFORMATEX_Type SpxAllocWAVEFORMATEX(size_t sizeInBytes)
{
    return SpxAllocSharedBuffer<SPXWAVEFORMATEX>(sizeInBytes);
}

inline SpxWAVEFORMATEX_Type SpxCopyWAVEFORMATEX(const SPXWAVEFORMATEX* format)
{
    if (format == nullptr)
    {
        return nullptr;
    }

    auto size = sizeof(SPXWAVEFORMATEX) + format->cbSize;
    auto copy = SpxAllocWAVEFORMATEX(size);
    memcpy(copy.get(), format, size);

    return copy;
}

inline SpxWAVEFORMATEX_Type SpxCopyWAVEFORMATEX(const SPXWAVEFORMATEX& format)
{
    return SpxCopyWAVEFORMATEX(&format);
}

inline SpxWAVEFORMATEX_Type SpxCopyWAVEFORMATEX(const SpxWAVEFORMATEX_Type& format)
{
    return SpxCopyWAVEFORMATEX(format.get());
}

inline uint16_t SpxCopyWAVEFORMATEX(SpxWAVEFORMATEX_Type source, SPXWAVEFORMATEX* dest, uint16_t destSize)
{
    auto sourceSize = uint16_t(sizeof(SPXWAVEFORMATEX) + source->cbSize);
    if (dest != nullptr)
    {
        auto copySize = std::min<uint16_t>(destSize, sourceSize);
        memcpy(dest, source.get(), copySize);
        return copySize;
    }
    return sourceSize;
}

#define SPX_TRACE_VERBOSE_WAVEFORMAT(format) \
    SPX_TRACE_VERBOSE_IF(format == nullptr, "%s - format == nullptr", __FUNCTION__); \
    SPX_TRACE_VERBOSE_IF(format != nullptr, "%s\n  wFormatTag:      %s\n  nChannels:       %d\n  nSamplesPerSec:  %d\n  nAvgBytesPerSec: %d\n  nBlockAlign:     %d\n  wBitsPerSample:  %d\n  cbSize:          %d", \
        __FUNCTION__, \
        format->wFormatTag == WAVE_FORMAT_PCM ? "PCM" : std::to_string(format->wFormatTag).c_str(), \
        format->nChannels, \
        format->nSamplesPerSec, \
        format->nAvgBytesPerSec, \
        format->nBlockAlign, \
        format->wBitsPerSample, \
        format->cbSize);

using SpxSharedAudioBuffer_Type = SpxSharedUint8Buffer_Type;
inline SpxSharedAudioBuffer_Type SpxAllocSharedAudioBuffer(size_t sizeInBytes)
{
    return SpxAllocSharedUint8Buffer(sizeInBytes);
}



SPX_INTERFACE(ISpxAudioOutputFormat)
{
    public:
    virtual bool HasHeader() = 0;
    virtual std::string GetFormatString() = 0;
    virtual SpxWAVEFORMATEX_Type GetFormat() = 0;
};

SPX_INTERFACE(ISpxAudioOutputInitFormat)
{
    public:
    virtual void SetHeader(bool hasHeader) = 0;
    virtual void SetFormatString(const std::string& formatString) = 0;
};

SPX_INTERFACE(ISpxAudioStreamReaderInitCallbacks)
{
    public:

    using ReadCallbackFunction_Type = std::function<int(uint8_t*, uint32_t)>;
    using CloseCallbackFunction_Type = std::function<void()>;
    using GetPropertyCallbackFunction_Type = std::function<void(PropertyId, uint8_t*, uint32_t)>;
    using GetPropertyCallbackFunction_Type2 = std::function<SPXSTRING(PropertyId)>;

    virtual void SetCallbacks(ReadCallbackFunction_Type readCallback, CloseCallbackFunction_Type closeCallback) = 0;

    virtual void SetPropertyCallback(GetPropertyCallbackFunction_Type getPropertyCallBack) { UNUSED(getPropertyCallBack); }
    virtual void SetPropertyCallback2(GetPropertyCallbackFunction_Type2 getPropertyCallBack) { UNUSED(getPropertyCallBack); }
};

SPX_INTERFACE(ISpxAudioStreamWriterInitCallbacks)
{
    public:
    using WriteCallbackFunction_Type = std::function<int(const uint8_t*, uint32_t)>;
    using CloseCallbackFunction_Type = std::function<void()>;

    virtual void SetWriterCallbacks(WriteCallbackFunction_Type writeCallback, CloseCallbackFunction_Type closeCallback) = 0;
};

SPX_INTERFACE(ISpxAudioStreamReader)
{
    public:
    virtual uint16_t GetFormat(SPXWAVEFORMATEX* pformat, uint16_t cbFormat) = 0;
    virtual uint32_t Read(uint8_t* pbuffer, uint32_t cbBuffer) = 0;
    // We pull this so that the user can give us timestamp or speaker id that are associated with the audio data.
    virtual SPXSTRING GetProperty(PropertyId propertyId) { UNUSED(propertyId); return ""; }
    virtual void SetShouldWaitForPendingData(bool shouldWait) { UNUSED(shouldWait); }
    virtual void Close() = 0;

    // Inline commit: pop the next pending commit marker if the reader has
    // delivered all the audio that precedes it. Returns true
    // and fills out the token and byte-offset if a marker was popped; false
    // otherwise. Default: no-op (readers that do not support inline commit).
    virtual bool PopPendingCommitMarker(uint32_t* /*outToken*/, uint64_t* /*outOffsetBytes*/, bool* /*outHasChannel*/, uint32_t* /*outChannelId*/) { return false; }

    // Inline commit: re-base the byte domain that commit anchors are
    // expressed in, so that a marker's offsetBytes counts from the start of
    // the session that is about to consume this reader rather than from the
    // creation of the stream.
    //
    // The session compares a commit's anchor against the audio it has sent on
    // the current connection, which it counts from zero per session (see
    // CSpxAudioStreamSession::m_sentAudioOffsetBytes and the audio buffer's
    // absolute offset). A stream that already carried audio for an earlier
    // session would otherwise stamp anchors that are larger than that count by
    // exactly the number of bytes it had already written, and the reconnect
    // comparisons - the obsolete-commit discard and the re-emission gate -
    // would both stop working.
    //
    // Called by the session at the point it creates the audio buffer the
    // anchors are measured against, so that both baselines are established
    // together. It must NOT be called on every pump start: the audio buffer
    // deliberately survives stop/restart on the same recognizer, and re-basing
    // without recreating it would reintroduce the same divergence in the
    // opposite direction.
    //
    // Default: no-op (readers that do not support inline commit).
    virtual void ResetCommitAnchorBase() {}
};

SPX_INTERFACE(ISpxAudioStreamReaderFactory)
{
    public:
    virtual std::shared_ptr<ISpxAudioStreamReader> CreateReader() = 0;
};

SPX_INTERFACE(ISpxSingleToManyStreamReaderAdapter)
{
    public:
    // The singleton is expected to implement ISpxAudioStreamReader and ISpxObjectInit so it can be reopened
    virtual void SetSingletonReader(std::shared_ptr<ISpxAudioStreamReader> singletonReader) = 0;
};

SPX_INTERFACE(ISpxSingleToManyStreamReaderAdapterSite)
{
    public:
    virtual void ReconnectClient(long clientId, std::shared_ptr<ISpxAudioStreamReader>&& reader) = 0;
    virtual void DisconnectClient(long clientId) = 0;
};

SPX_INTERFACE(ISpxSetErrorInfo)
{
    public:
    virtual void SetError(const std::string& error) = 0;
};

SPX_INTERFACE(ISpxAudioStreamWriter)
{
    public:
    virtual void Write(uint8_t* buffer, uint32_t size) = 0;
    virtual void SetProperty(PropertyId propertyId, const SPXSTRING& value) = 0;
    virtual void SetProperty(const SPXSTRING& name, const SPXSTRING& value) = 0;

    // Inline commit: queue a commit request anchored at the current write
    // position. Returns a token that identifies this commit; the token is
    // reported back on the recognizer's Recognized event via the CommitToken
    // field on the SpeechRecognitionResult when the service acknowledges the
    // commit. Default: unsupported.
    //
    // The trailing return is unreachable, but it is required. Whether a
    // compiler can see that SPX_THROW_HR never returns depends on include
    // order, not on the compiler: SPX_THROW_HR routes to the [[noreturn]]
    // ThrowWithCallstack only in translation units that reach this header
    // through spxcore_common.h. Many do not, and there the throw expands to a
    // plain call with no [[noreturn]], so GCC, Clang and MSVC alike report
    // the function as falling off the end (-Wreturn-type). In the units that
    // do see [[noreturn]], the same return becomes unreachable code, which
    // MSVC reports as C4702. Both are errors under warnings-as-errors, so
    // keep the return and suppress C4702.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4702)
#endif
    virtual uint32_t Commit() { SPX_THROW_HR(SPXERR_NOT_IMPL); return 0; }

    // Inline commit: same as Commit() but scoped to a single channel of a
    // multichannel input. The service acknowledges
    // on the named channel only, still via a Recognized event carrying the
    // token. Default: unsupported.
    virtual uint32_t Commit(uint32_t /*channelId*/) { SPX_THROW_HR(SPXERR_NOT_IMPL); return 0; }
#ifdef _MSC_VER
#pragma warning(pop)
#endif
};

SPX_INTERFACE(ISpxAudioFile)
{
    public:
    virtual void Open(const char * fileName) = 0;
    virtual void Close() = 0;

    virtual bool IsOpen() const = 0;

    virtual void SetContinuousLoop(bool value) = 0;
    virtual void SetIterativeLoop(bool value) = 0;
};

SPX_INTERFACE(ISpxAudioOutputReader)
{
    public:
    virtual uint32_t Read(uint8_t* buffer, uint32_t bufferSize) = 0;
    virtual uint32_t AvailableSize() = 0;
};

SPX_INTERFACE(ISpxUser)
{
    public:
    virtual void InitFromUserId(const char* pszUserId) = 0;
    virtual std::string GetId() const = 0;
};

SPX_INTERFACE(ISpxAudioConfig)
{
    public:
    virtual void InitFromDefaultDevice() = 0;
    virtual void InitFromFile(const char * fileName) = 0;
    virtual void InitFromStream(std::shared_ptr<ISpxAudioStream> stream) = 0;

    virtual std::string GetFileName() const = 0;
    virtual std::shared_ptr<ISpxAudioStream> GetStream() = 0;
};

SPX_INTERFACE(ISpxAudioReplayer)
{
    public:
    virtual void ShrinkReplayBuffer(uint64_t newBaseOffset) = 0;
    virtual void GetCurrentAudioBufferOffset(uint64_t* offsetInTicks, uint64_t* offsetInBytes) = 0;
    virtual void GetCurrentAudioContinuationOffset(uint64_t * offsetInTicks) = 0;
    virtual void GetMultiChannelProcessingMode(bool* useMultiChannelProcessing) = 0;
};

SPX_INTERFACE(ISpxInternalAudioCodecAdapter)
{
public:
    using SPXCompressedDataCallback = std::function<void(const uint8_t * outData, size_t nBytesOut)>;

    virtual SPXHR Load(const std::string& modulename, const std::string& codec, SPXCompressedDataCallback dataCallback) = 0;
    virtual void InitCodec(const SPXWAVEFORMATEX* inputFormat) = 0;
    virtual std::string GetContentType() = 0;
    virtual void Encode(const uint8_t* buffer, size_t bufferSize) = 0;
    virtual void Flush() = 0;
    virtual void CloseEncodingStream() = 0;
};

SPX_INTERFACE(ISpxAudioPump)
{
    public:
    virtual uint16_t GetFormat(SPXWAVEFORMATEX* pformat, uint16_t cbFormat) const = 0;
    virtual void SetFormat(const SPXWAVEFORMATEX* pformat, uint16_t cbFormat) = 0;

    virtual void StartPump(std::shared_ptr<ISpxAudioProcessor> pISpxAudioProcessor) = 0;
    virtual void PausePump() = 0;
    virtual void StopPump() = 0;

    enum class State { NoInput, Idle, Paused, Processing };
    virtual State GetState() const = 0;

    virtual std::string GetPropertyValue(const std::string& key) const = 0;
};

SPX_INTERFACE(ISpxAudioPumpInit)
{
    public:
    virtual void SetReader(std::shared_ptr<ISpxAudioStreamReader> reader) = 0;
};

#define REASON_CANCELED_NONE static_cast<CancellationReason>(0)
#define NO_MATCH_REASON_NONE static_cast<NoMatchReason>(0)

SPX_INTERFACE(ISpxKeywordRecognitionResultInit)
{
    public:
    virtual void InitKeywordResult(const double confidence, const uint64_t offset, const uint64_t duration, const char* keyword, ResultReason reason, std::shared_ptr<ISpxAudioDataStream> stream) = 0;
};

class ISpxSynthesizerEvents;

using SpxAudioData_Type = std::shared_ptr<std::vector<uint8_t>>;
SPX_INTERFACE(ISpxSynthesisResult)
{
    public:
    virtual std::string GetResultId() = 0;
    virtual std::string GetRequestId() = 0;
    virtual ResultReason GetReason() = 0;
    virtual CancellationReason GetCancellationReason() = 0;
    virtual const std::shared_ptr<ISpxErrorInformation>& GetError() = 0;
    virtual uint32_t GetAudioLength() = 0;
    virtual uint64_t GetAudioDuration() = 0;
    virtual SpxAudioData_Type GetAudioData() = 0;
    virtual SpxAudioData_Type GetRawAudioData() = 0;
    virtual std::shared_ptr<ISpxAudioDataStream> GetAudioDataStream() = 0;
    virtual SpxWAVEFORMATEX_Type GetFormat() = 0;
    virtual bool HasHeader() = 0;
};

SPX_INTERFACE(ISpxSynthesisResultInit)
{
    public:
    virtual void InitSynthesisResult(const std::string &requestId, ResultReason reason,
                                     const std::shared_ptr<ISpxErrorInformation> &error) = 0;
    virtual void SetAudioData(std::shared_ptr<std::vector<uint8_t>>, uint64_t duration) = 0;
    virtual void SetAudioFormat(std::shared_ptr<SPXWAVEFORMATEX> format, bool hasHeader) = 0;
    virtual void SetAudioDataStream(const std::shared_ptr<ISpxAudioDataStream> &stream) = 0;
    virtual void UpdateError(const std::shared_ptr<ISpxErrorInformation> &error) = 0;
    virtual void Reset() = 0;
};

enum class InteractionIdPurpose { Speech = 0, Activity };

SPX_INTERFACE(ISpxInteractionIdProvider)
{
    public:
    virtual std::string NextInteractionId(InteractionIdPurpose purpose) noexcept = 0;
    virtual std::string GetInteractionId(InteractionIdPurpose purpose) const noexcept = 0;
};

SPX_INTERFACE(ISpxDialogServiceConnector)
{
    public:
    virtual CSpxAsyncOp<void> ConnectAsync() = 0;
    virtual CSpxAsyncOp<void> DisconnectAsync() = 0;

    virtual CSpxAsyncOp<std::string> SendActivityAsync(std::string activity) = 0;

    virtual CSpxAsyncOp<void> StartContinuousListeningAsync() = 0;
    virtual CSpxAsyncOp<void> StopContinuousListeningAsync() = 0;

    virtual CSpxAsyncOp<void> StartKeywordRecognitionAsync(std::shared_ptr<ISpxKwsModel> model) = 0;
    virtual CSpxAsyncOp<void> StopKeywordRecognitionAsync() = 0;

    /* TODO: Change promise type back to void */
    virtual CSpxAsyncOp<std::shared_ptr<ISpxRecognitionResult>> ListenOnceAsync() = 0;
};

SPX_INTERFACE(ISpxMessageParamFromUser)
{
    public:
    virtual void SetParameter(const char *path, const char *name, const char *value) = 0;
    virtual CSpxAsyncOp<bool> SendNetworkMessage(const char *path, std::string&& payload) = 0;
    virtual CSpxAsyncOp<bool> SendNetworkMessage(const char *path, std::vector<uint8_t>&& payload) = 0;
};

SPX_INTERFACE(ISpxSynthesizer)
{
public:
    virtual void SetOutput(std::shared_ptr<ISpxAudioOutput> output) = 0;
    virtual std::shared_ptr<ISpxSynthesisResult> Speak(const std::string &text, bool isSsml,
                                                       const std::shared_ptr<ISpxSynthesisRequest> &request = nullptr) = 0;
    virtual CSpxAsyncOp<std::shared_ptr<ISpxSynthesisResult>> SpeakAsync(const std::string &text, bool isSsml,
                                                                         const std::shared_ptr<ISpxSynthesisRequest> &request = nullptr) = 0;
    virtual std::shared_ptr<ISpxSynthesisResult> StartSpeaking(const std::string &text, bool isSsml,
                                                               const std::shared_ptr<ISpxSynthesisRequest> &request = nullptr) = 0;
    virtual CSpxAsyncOp<std::shared_ptr<ISpxSynthesisResult>> StartSpeakingAsync(const std::string &text, bool isSsml,
                                                                                 const std::shared_ptr<ISpxSynthesisRequest> &request = nullptr) = 0;
    virtual void StopSpeaking() = 0;
    virtual CSpxAsyncOp<void> StopSpeakingAsync() = 0;
    virtual CSpxAsyncOp<std::shared_ptr<ISpxSynthesisVoicesResult>> GetVoicesAsync(const std::string& locale) = 0;
    virtual std::shared_ptr<ISpxMessageParamFromUser> GetMessageParamFromUser() = 0;

    virtual void Close() = 0;
    virtual void SetDisposing() = 0;

    virtual std::shared_ptr<std::string> FormatWordBoundaryForCache() = 0;

    virtual void EnsureValidToken() = 0;

    virtual void OpenConnection() = 0;
    virtual void CloseConnection() = 0;
};

SPX_INTERFACE(ISpxSynthesizerConnection)
{
    public:
    /// <summary>
    /// Initialises the speech synthesizer connection
    /// </summary>
    /// <param name="synthesizer">The speech synthesizer instance this connection is for</param>
    virtual void Init(std::weak_ptr<ISpxSynthesizer> synthesizer) = 0;

    /// <summary>
    /// Gets the speech synthesizer instance
    /// </summary>
    /// <returns>The instance, or a nullptr if the instance is no longer valid</returns>
    virtual std::shared_ptr<ISpxSynthesizer> GetSynthesizer() = 0;
};

SPX_INTERFACE(ISpxGetUspMessageParamsFromUser)
{
    public:
    virtual CSpxStringMap GetParametersFromUser(std::string&& path) = 0;
};

SPX_INTERFACE(ISpxGetUspMessageParamsFromRecognizer)
{
public:
    virtual CSpxStringMap GetParametersFromRecognizer(std::string && path) = 0;
};

SPX_INTERFACE(ISpxConnection)
{
    public:
    virtual void Open(bool forContinuousRecognition) = 0;
    virtual void Close() = 0;
    virtual std::shared_ptr<ISpxRecognizer> GetRecognizer() = 0;
};

SPX_INTERFACE(ISpxConnectionInit)
{
    public:
    virtual void Init(std::weak_ptr<ISpxRecognizer> recognizer, std::weak_ptr<ISpxMessageParamFromUser> setter) = 0;
};

SPX_INTERFACE(ISpxConnectionFromRecognizer)
{
    public:
    virtual std::shared_ptr<ISpxConnection> GetConnection() = 0;
};

SPX_INTERFACE(ISpxActivityEventArgsInit)
{
    public:

    virtual void Init(std::string activity) = 0;
    virtual void Init(std::string activity, std::shared_ptr<ISpxAudioOutput> audio) = 0;
};

SPX_INTERFACE(ISpxTurnStatusEventArgsInit)
{
    public:

    virtual void Init(std::string interactionId, std::string conversationId, int statusCode) = 0;
};

SPX_INTERFACE(ISpxConnectionMessage)
{
    public:

    virtual const std::string& GetHeaders() const = 0;
    virtual const std::string& GetPath() const = 0;

    virtual const uint8_t* GetBuffer() const = 0;
    virtual uint32_t GetBufferSize() const = 0;
    virtual bool IsBufferBinary() const = 0;
};

SPX_INTERFACE(ISpxConnectionMessageInit)
{
    public:
    virtual void Init(const std::string& headers, const std::string& path, const uint8_t* buffer, uint32_t bufferSize, bool bufferIsBinary) = 0;
};

SPX_INTERFACE(ISpxConnectionMessageEventArgs)
{
    public:
    virtual std::shared_ptr<ISpxConnectionMessage> GetMessage() const = 0;
};

SPX_INTERFACE(ISpxConnectionMessageEventArgsInit)
{
    public:

    virtual void Init(std::shared_ptr<ISpxConnectionMessage> message) = 0;
};

SPX_INTERFACE(ISpxActivityEventArgs)
{
    public:

    virtual const std::string& GetActivity() const = 0;
    virtual bool HasAudio() const = 0;
    virtual std::shared_ptr<ISpxAudioOutput> GetAudio() const = 0;
};

SPX_INTERFACE(ISpxTurnStatusEventArgs)
{
    public:

    virtual const std::string& GetInteractionId() const = 0;
    virtual const std::string& GetConversationId() const = 0;
    virtual int GetStatusCode() const = 0;
};

SPX_INTERFACE(ISpxRecognizerEvents)
{
public:
    using RecoEvent_Type = EventSignalBase<std::shared_ptr<ISpxRecognitionEventArgs>>;
    using SessionEvent_Type = EventSignalBase<std::shared_ptr<ISpxSessionEventArgs>>;
    using ConnectionEvent_Type = EventSignalBase<std::shared_ptr<ISpxConnectionEventArgs>>;
    using ConnectionMessageEvent_Type = EventSignalBase<std::shared_ptr<ISpxConnectionMessageEventArgs>>;

    virtual void FireSessionStarted(const std::wstring& sessionId) = 0;
    virtual void FireSessionStopped(const std::wstring& sessionId) = 0;

    virtual void FireConnected(const std::wstring& sessionId) = 0;
    virtual void FireDisconnected(const std::wstring& sessionId) = 0;

    virtual void FireSpeechStartDetected(const std::wstring& sessionId, uint64_t offset) = 0;
    virtual void FireSpeechEndDetected(const std::wstring& sessionId, uint64_t offset) = 0;
    virtual void FireConnectionMessageReceived(const std::string& headers, const std::string& path, const uint8_t* buffer, uint32_t bufferSize, bool isBufferBinary) = 0;

    virtual void FireResultEvent(const std::wstring& sessionId, std::shared_ptr<ISpxRecognitionResult> result) = 0;
    virtual void FireTokenRequest(const std::wstring & sessionId) = 0;

    SessionEvent_Type SessionStarted;
    SessionEvent_Type SessionStopped;
    SessionEvent_Type TokenRequested;

    ConnectionEvent_Type Connected;
    ConnectionEvent_Type Disconnected;
    ConnectionMessageEvent_Type ConnectionMessageReceived;

    RecoEvent_Type SpeechStartDetected;
    RecoEvent_Type SpeechEndDetected;

    RecoEvent_Type IntermediateResult;
    RecoEvent_Type FinalResult;
    RecoEvent_Type Canceled;
    RecoEvent_Type TranslationSynthesisResult;
};

SPX_INTERFACE(ISpxDialogServiceConnectorEvents)
{
public:
    using ActivityReceivedEvent_Type = EventSignalBase<std::shared_ptr<ISpxActivityEventArgs>>;
    using TurnStatusEvent_Type = EventSignalBase<std::shared_ptr<ISpxTurnStatusEventArgs>>;

    ActivityReceivedEvent_Type ActivityReceived;
    TurnStatusEvent_Type TurnStatusReceived;

    virtual void FireActivityReceived(const std::wstring& sessionId, std::string activity, std::shared_ptr<ISpxAudioOutput> audio) = 0;
    virtual void FireTurnStatus(const std::string& interactionId, const std::string& conversationId, int statusCode) = 0;
};

SPX_INTERFACE(ISpxSynthesisEventArgs)
{
    public:
    virtual std::shared_ptr<ISpxSynthesisResult> GetResult() = 0;
};

SPX_INTERFACE(ISpxSynthesisEventArgsInit)
{
    public:
    virtual void Init(std::shared_ptr<ISpxSynthesisResult> result) = 0;
};

SPX_INTERFACE(ISpxSynthesizerEvents)
{
    public:
    using SynthEvent_Type = EventSignalBase<std::shared_ptr<ISpxSynthesisEventArgs>>;
    using WordBoundaryEvent_Type = EventSignalBase<std::shared_ptr<ISpxWordBoundaryEventArgs>>;
    using VisemeEvent_Type = EventSignalBase<std::shared_ptr<ISpxVisemeEventArgs>>;
    using BookmarkEvent_Type = EventSignalBase<std::shared_ptr<ISpxBookmarkEventArgs>>;
    using SynthesisCallbackFunction_Type = std::function<void(std::shared_ptr<ISpxSynthesisEventArgs>)>;
    using ConnectionEvent_Type = EventSignalBase<std::shared_ptr<ISpxConnectionEventArgs>>;
    using SessionEvent_Type = EventSignalBase<std::shared_ptr<ISpxSessionEventArgs>>;

    virtual void FireResultEvent(std::shared_ptr<ISpxSynthesisResult> result, std::shared_ptr<CountDownLatch> eventsSyncLatch) = 0;
    virtual void FireWordBoundary(std::string resultId, uint64_t audioOffset, uint64_t duration, uint32_t textOffset,
                                  uint32_t wordLength, std::string text,
                                  SpeechSynthesisBoundaryType boundaryType) = 0;
    virtual void FireVisemeReceived(std::string resultId, uint64_t audioOffset, uint32_t visemeId, std::string animation) = 0;
    virtual void FireBookmarkReached(std::string resultId, uint64_t audioOffset, std::string text) = 0;
    virtual void FireConnectionChanged(bool connected) = 0;
    virtual void FireTokenRequest() = 0;

    SynthEvent_Type SynthesisStarted;
    SynthEvent_Type Synthesizing;
    SynthEvent_Type SynthesisCompleted;
    SynthEvent_Type SynthesisCanceled;
    SessionEvent_Type TokenRequested;
    WordBoundaryEvent_Type WordBoundary;
    VisemeEvent_Type VisemeReceived;
    BookmarkEvent_Type BookmarkReached;
    ConnectionEvent_Type Connected;
    ConnectionEvent_Type Disconnected;
};

SPX_INTERFACE(ISpxConversationWithImpl)
{
    public:
    virtual std::shared_ptr<ISpxConversation> GetConversationImpl() = 0;
};

SPX_INTERFACE(ISpxMeetingWithImpl)
{
public:
    virtual std::shared_ptr<ISpxMeeting> GetMeetingImpl() = 0;
};

SPX_INTERFACE(ISpxSession)
{
    public:
    virtual const std::wstring& GetSessionId() const = 0;
    virtual bool IsStreaming() = 0;

    virtual void AddRecognizer(std::shared_ptr<ISpxRecognizer> recognizer) = 0;
    virtual void RemoveRecognizer(ISpxRecognizer* recognizer) = 0;

    virtual CSpxAsyncOp<std::shared_ptr<ISpxRecognitionResult>> RecognizeAsync() = 0;
    virtual CSpxAsyncOp<std::shared_ptr<ISpxRecognitionResult>> RecognizeAsyncWithVAD() = 0;
    virtual CSpxAsyncOp<void> StartContinuousRecognitionAsync() = 0;
    virtual CSpxAsyncOp<void> StopContinuousRecognitionAsync() = 0;

    virtual CSpxAsyncOp<void> StartContinuousRecognitionAsyncWithVAD() = 0;
    virtual CSpxAsyncOp<void> StopContinuousRecognitionAsyncWithVAD() = 0;

    virtual CSpxAsyncOp<std::shared_ptr<ISpxRecognitionResult>> RecognizeAsync(std::shared_ptr<ISpxKwsModel> model) = 0;
    virtual CSpxAsyncOp<void> StartKeywordRecognitionAsync(std::shared_ptr<ISpxKwsModel> model) = 0;
    virtual CSpxAsyncOp<void> StopKeywordRecognitionAsync() = 0;

    virtual void OpenConnection(bool forContinuousRecognition) = 0;
    virtual void CloseConnection() = 0;

    virtual CSpxAsyncOp<std::string> SendActivityAsync(std::string activity) = 0;

    virtual void WriteTelemetryLatency(uint64_t latencyInTicks, bool isPhraseLatency, bool isFirstHypothesisLatency) = 0;
    virtual void SendSpeechEventMessage(std::string&& payload) = 0;
    virtual CSpxAsyncOp<bool> SendNetworkMessage(const char *path, std::string&& payload, bool alwaysSend = true) = 0;
    virtual CSpxAsyncOp<bool> SendNetworkMessage(const char *path, std::vector<uint8_t>&& payload, bool alwaysSend = true) = 0;

    virtual void SetConversation(std::shared_ptr<ISpxConversation> conversation) = 0;
    virtual void SetMeeting(std::shared_ptr<ISpxMeeting> meeting) = 0;
    virtual void SetDisposing() = 0;
};

SPX_INTERFACE(ISpxAudioStreamSessionInit)
{
    public:
    virtual void InitFromFile(const char * fileName) = 0;
    virtual void InitFromMicrophone() = 0;
    virtual void InitFromStream(std::shared_ptr<ISpxAudioStream> stream) = 0;
};

SPX_INTERFACE(ISpxAudioSourceControl)
{
    public:

    virtual void StartAudio(std::shared_ptr<ISpxAudioSourceNotifyMe> target) = 0;
    virtual void StopAudio() = 0;
};

SPX_INTERFACE(ISpxSessionFromRecognizer)
{
    public:
    virtual std::shared_ptr<ISpxSession> GetDefaultSession() = 0;
};

SPX_INTERFACE(ISpxRecoEngineAdapter)
    , public ISpxAudioProcessor
{
    public:
    virtual void SetAdapterMode(bool singleShot) = 0;
    virtual void SetKeyword(const std::string&) {};
    virtual void OpenConnection(bool) {};
    virtual void CloseConnection() {};
    virtual void SendAgentMessage(const std::string &) {};

    virtual void WriteTelemetryLatency(uint64_t, bool, bool) {};
    virtual void FlushTelemetry() {};

    virtual void SendSpeechEventMessage(std::string&&) {};
    virtual void SendNetworkMessage(const char*, std::string&&, const std::shared_ptr<std::promise<bool>>& ) {}
    virtual void SendNetworkMessage(const char*, std::vector<uint8_t>&&, const std::shared_ptr<std::promise<bool>>&) {}

    // Inline commit: send an audio.commit message carrying the app-visible
    // token via the X-Client-Commit-Token header. Default:
    // unsupported (adapter has no commit-capable transport).
    virtual void SendCommit(uint32_t /*token*/, bool /*hasChannel*/, uint32_t /*channelId*/) {}
};

SPX_INTERFACE(ISpxActivityResultAdapter)
{
    public:
    virtual void FireActivityResult(std::string activity, std::shared_ptr<ISpxAudioOutput> audio) = 0;
};

SPX_INTERFACE(ISpxRecoEngineAdapterSite)
{
    public:
    using ResultPayload_Type = std::shared_ptr<ISpxRecognitionResult>;
    using AdditionalMessagePayload_Type = void*;

    virtual void GetScenarioCount(uint16_t* countSpeech, uint16_t* countTranslation, uint16_t* countDialog, uint16_t* countConversationTranscriber, uint16_t* countConversationTranscriberV2, uint16_t * countMeetingTranscriber, uint16_t* countLanguageId) = 0;

    virtual std::list<std::string> GetListenForList() = 0;
    virtual std::shared_ptr<ISpxRecognitionResult> GetSpottedKeywordResult() = 0;

    virtual void AdapterStartingTurn(ISpxRecoEngineAdapter* adapter) = 0;
    virtual void AdapterStartedTurn(ISpxRecoEngineAdapter* adapter, const std::string& id, OffsetType adapterStartOffset = 0) = 0;
    virtual void AdapterStoppedTurn(ISpxRecoEngineAdapter* adapter, bool isRestarting = false) = 0;
    virtual bool IsExpectingAdapterStoppedTurn(ISpxRecoEngineAdapter* adapter) = 0;

    virtual void AdapterDetectedSpeechStart(ISpxRecoEngineAdapter* adapter, uint64_t offset) = 0;
    virtual void AdapterDetectedSpeechEnd(ISpxRecoEngineAdapter* adapter, uint64_t offset) = 0;

    virtual void AdapterDetectedSoundStart(ISpxRecoEngineAdapter* adapter, uint64_t offset) = 0;
    virtual void AdapterDetectedSoundEnd(ISpxRecoEngineAdapter* adapter, uint64_t offset) = 0;

    virtual void FireAdapterResult_Intermediate(uint64_t offset, ResultPayload_Type payload) = 0;
    virtual void FireAdapterResult_KeywordResult(uint64_t offset, ResultPayload_Type payload, bool isAccepted) = 0;
    virtual void FireAdapterResult_FinalResult(uint64_t offset, ResultPayload_Type payload) = 0;
    virtual void FireAdapterResult_TranslationSynthesis(ResultPayload_Type payload) = 0;
    virtual void FireAdapterResult_ActivityReceived(std::string activity, std::shared_ptr<ISpxAudioOutput> audio) = 0;
    virtual void FireAdapterResult_TurnStatusReceived(std::wstring interactionId, std::string conversationId, int statusCode) = 0;
    virtual void AdapterEndOfDictation(ISpxRecoEngineAdapter* adapter, uint64_t offset, uint64_t duration) = 0;
    virtual void AdapterConnected(const std::string&) = 0;
    virtual void AdapterDisconnected(std::shared_ptr<ISpxErrorInformation> payload) = 0;
    virtual void FireConnectionMessageReceived(const std::string& headers, const std::string& path, const uint8_t* buffer, uint32_t bufferSize, bool isBufferBinary) = 0;

    virtual void AdapterRequestingAudioMute(ISpxRecoEngineAdapter* adapter, bool mute) = 0;
    virtual void AdapterCompletedSetFormatStop(ISpxRecoEngineAdapter* adapter) = 0;

    // Inline commit: adapter forwards a service-side acknowledgment of an
    // audio.commit request. Session pops the matching entry from its
    // unacknowledged-commits FIFO and fires a Recognized event carrying the
    // commit token on the SpeechRecognitionResult.
    // Default: no-op (adapter-site wrappers that do not host an
    // unacknowledged-commits FIFO simply ignore the signal; only
    // CSpxAudioStreamSession overrides).
    virtual void AdapterCommitAcknowledged(ISpxRecoEngineAdapter* /*adapter*/, uint32_t /*token*/, uint64_t /*offset*/, uint64_t /*duration*/) {}

    virtual void AdditionalMessage(ISpxRecoEngineAdapter* adapter, uint64_t offset, AdditionalMessagePayload_Type payload) = 0;

    virtual void Error(ISpxRecoEngineAdapter* adapter, std::shared_ptr<ISpxErrorInformation> payload) = 0;
};

class ISpxTtsEngineAdapterSite;

SPX_INTERFACE(ISpxTtsEngineAdapter)
{
    public:
    virtual void SetOutput(const std::shared_ptr<ISpxAudioOutput>& output) = 0;
    virtual std::shared_ptr<ISpxSynthesisResult> Speak(const std::string& text, bool isSsml, const std::string& requestId, bool retry) = 0;
    virtual std::shared_ptr<ISpxSynthesisResult> Speak(std::shared_ptr<ISpxSynthesisRequestReader>, bool retry) = 0;
    virtual void StopSpeaking(const std::shared_ptr<ISpxErrorInformation> &reason = nullptr) = 0;
    virtual std::shared_ptr<ISpxSynthesisVoicesResult> GetVoices(const std::string& locale) = 0;

    /// <summary>
    /// Connects and/or reconnects to the speech synthesis service
    /// </summary>
    virtual void Connect() = 0;

    /// <summary>
    /// Close the connection to speech synthesis service.
    /// <param name="async">Wait for close handshake finished if set to false.</param>
    /// </summary>
    virtual void Disconnect(bool async = false) = 0;
};

SPX_INTERFACE(ISpxTtsEngineAdapterSite)
{
    public:
    virtual uint32_t Write(ISpxTtsEngineAdapter* adapter, const std::string& requestId, uint8_t* buffer, uint32_t size, std::shared_ptr<std::map<std::string, std::string>> properties) = 0;
    virtual std::shared_ptr<ISpxSynthesizerEvents> GetEventsSite() = 0;
    virtual std::shared_ptr<ISpxSynthesisResult> CreateEmptySynthesisResult() = 0;
    virtual std::shared_ptr<ISpxSynthesisVoicesResult> CreateEmptySynthesisVoicesResult() = 0;
    virtual std::shared_ptr<ISpxVoiceInfo> CreateEmptyVoiceInfo() = 0;
    virtual void SetAdapterFormat(const ISpxTtsEngineAdapter *adapter, const std::shared_ptr<SPXWAVEFORMATEX> &format) = 0;
    virtual void FireAdapterResult_WordBoundary(ISpxTtsEngineAdapter * adapter, uint64_t audioOffset,
                                                uint64_t duration, uint32_t textOffset, uint32_t wordLength,
                                                const std::string &text, SpeechSynthesisBoundaryType boundaryType) = 0;
    virtual void FireAdapterResult_VisemeReceived(ISpxTtsEngineAdapter * adapter, uint64_t audioOffset, uint32_t visemeId, std::string animation) = 0;
    virtual void FireAdapterResult_BookmarkReached(ISpxTtsEngineAdapter* adapter, uint64_t audioOffset, const std::string& text) = 0;
    virtual void FireAdapterResult_ConnectionChanged(ISpxTtsEngineAdapter* adapter, bool connected) = 0;
    /// <summary>
    /// Called when the adapter is starting a turn.
    /// For USP, this means the service echo message (`turn.start`) is received.
    /// </summary>
    virtual void FireAdapterResult_TurnStarted(ISpxTtsEngineAdapter* adapter) = 0;
    /// <summary>
    /// Called when the synthesis turn in finished.
    /// </summary>
    virtual void EndOfTurn(ISpxTtsEngineAdapter* adapter) = 0;
    virtual void SetSynthesisResultAudioDuration(ISpxTtsEngineAdapter* adapter, uint64_t duration) = 0;
    virtual size_t AudioLengthOfCurrentTurn() = 0;
    virtual std::shared_ptr<ISpxTtsEngineAdapter> GetTtsEngineAdapter() = 0;
    virtual bool IsStopping() = 0;
    virtual void SetBackendName(const std::string & backend) = 0;
};

SPX_INTERFACE(ISpxAudioPumpSite)
{
    public:
    virtual void Error(const std::string& msg) = 0;
};

SPX_INTERFACE(ISpxSpeechAudioProcessorAdapter)
    , public ISpxAudioProcessor
{
    public:
    virtual void SetSpeechDetectionThreshold(uint32_t threshold) = 0;
    virtual void SetSpeechDetectionSilenceMs(uint32_t duration) = 0;
    virtual void SetSpeechDetectionSkipMs(uint32_t duration) = 0;
    virtual void SetSpeechDetectionBaselineMs(uint32_t duration) = 0;
};

SPX_INTERFACE(ISpxSpeechAudioProcessorAdapterSite)
{
    public:
    virtual void SpeechStartDetected(uint64_t offset) = 0;
    virtual void SpeechEndDetected(uint64_t offset) = 0;
};

SPX_INTERFACE(ISpxDetectorEngineAdapter)
    , public ISpxAudioProcessor
{
};

SPX_INTERFACE(ISpxDetectorEngineAdapterSite)
{
    public:
    virtual void OnDetected(ISpxDetectorEngineAdapter* adapter, uint64_t offset, uint64_t duration, double confidence, const std::string& keyword, const DataChunkPtr& audioChunk) = 0;
    virtual void AdapterCompletedSetFormatStop(ISpxDetectorEngineAdapter* adapter) = 0;
    virtual void GatingAdapterFireInitialSilenceTimeout() = 0;
};

SPX_INTERFACE(ISpxRecoResultFactory)
{
    public:
    virtual std::shared_ptr<ISpxRecognitionResult> CreateIntermediateResult(const char* text, uint64_t offset, uint64_t duration, const char* phraseId) = 0;
    virtual std::shared_ptr<ISpxRecognitionResult> CreateFinalResult(
        ResultReason reason,
        NoMatchReason noMatchReason,
        const char* text,
        uint64_t offset,
        uint64_t duration,
        const char* phraseId,
        const char* userId = nullptr) = 0;
    virtual std::shared_ptr<ISpxRecognitionResult> CreateKeywordResult(const double confidence, const uint64_t offset, const uint64_t duration, const char* keyword, ResultReason reason, std::shared_ptr<ISpxAudioDataStream> stream) = 0;
    virtual std::shared_ptr<ISpxRecognitionResult> CreateErrorResult(const std::shared_ptr<ISpxErrorInformation>& error) = 0;
    virtual std::shared_ptr<ISpxRecognitionResult> CreateEndOfStreamResult() = 0;
};

SPX_INTERFACE(ISpxKeywordRecognitionResult)
{
    public:
    virtual double GetConfidence() = 0;
};

SPX_INTERFACE(ISpxEventArgsFactory)
{
    public:
    virtual std::shared_ptr<ISpxSessionEventArgs> CreateSessionEventArgs(const std::wstring& sessionId) = 0;
    virtual std::shared_ptr<ISpxConnectionEventArgs> CreateConnectionEventArgs(const std::wstring& sessionId) = 0;
    virtual std::shared_ptr<ISpxConnectionMessageEventArgs> CreateConnectionMessageEventArgs(const std::string& headers, const std::string& path, const uint8_t* buffer, uint32_t bufferSize, bool isBufferBinary) = 0;
    virtual std::shared_ptr<ISpxRecognitionEventArgs> CreateRecognitionEventArgs(const std::wstring& sessionId, uint64_t offset) = 0;
    virtual std::shared_ptr<ISpxRecognitionEventArgs> CreateRecognitionEventArgs(const std::wstring& sessionId, std::shared_ptr<ISpxRecognitionResult> result) = 0;
    virtual std::shared_ptr<ISpxActivityEventArgs> CreateActivityEventArgs(std::string activity, std::shared_ptr<ISpxAudioOutput> audio) = 0;
    virtual std::shared_ptr<ISpxTurnStatusEventArgs> CreateTurnStatusEventArgs(const std::string& interactionId, const std::string& conversationId, int statusCode) = 0;
    virtual std::shared_ptr<ISpxSessionEventArgs> CreateTokenRequestEventArgs(const std::wstring & sessionId) = 0;
};

SPX_INTERFACE(ISpxRecognizerSite)
{
    public:
    virtual std::shared_ptr<ISpxSession> GetDefaultSession() = 0;
};

SPX_INTERFACE(ISpxSpeechApiFactory)
{
    public:
    virtual std::shared_ptr<ISpxRecognizer> CreateSpeechRecognizerFromConfig(std::shared_ptr<ISpxAudioConfig> audioInput) = 0;
    virtual std::shared_ptr<ISpxRecognizer> CreateSourceLanguageRecognizerFromConfig(std::shared_ptr<ISpxAudioConfig> audioInput) = 0;
    virtual std::shared_ptr<ISpxDialogServiceConnector> CreateDialogServiceConnectorFromConfig(std::shared_ptr<ISpxAudioConfig> audioInput) = 0;
    virtual std::shared_ptr<ISpxRecognizer> CreateTranslationRecognizerFromConfig(std::shared_ptr<ISpxAudioConfig> audioInput) = 0;
    virtual std::shared_ptr<ISpxRecognizer> CreateConversationTranscriberV2FromConfig(std::shared_ptr<ISpxAudioConfig> audioInput) = 0;
    virtual std::shared_ptr<ISpxMeeting> CreateMeetingFromConfig(const char* id) = 0;
    virtual void InitSessionFromAudioInputConfig(std::shared_ptr<ISpxAudioStreamSessionInit> session, std::shared_ptr<ISpxAudioConfig> audioInput) = 0;
};

SPX_INTERFACE(ISpxSpeechSynthesisApiFactory)
{
    public:
    virtual std::shared_ptr<ISpxSynthesizer> CreateSpeechSynthesizerFromConfig(std::shared_ptr<ISpxAudioConfig> audioConfig) = 0;
};

SPX_INTERFACE(ISpxTranslationSynthesisResult)
{
    public:
    virtual const uint8_t* GetAudio() const = 0;
    virtual size_t GetLength() const = 0;
    virtual std::string GetRequestId() const = 0;
};

SPX_INTERFACE(ISpxTranslationSynthesisResultInit)
{
    public:
    virtual void InitTranslationSynthesisResult(const uint8_t* audioData, size_t audioLength, const std::string& requestId) = 0;
};

SPX_INTERFACE(ISpxPhrase)
{
    public:

    virtual void InitPhrase(const wchar_t* phrase) = 0;
    virtual std::wstring GetPhrase() const = 0;
};

SPX_INTERFACE(ISpxPhraseList)
{
    public:

    virtual void InitPhraseList(const wchar_t* name) = 0;
    virtual std::wstring GetName() = 0;

    virtual void AddPhrase(std::shared_ptr<ISpxPhrase> phrase) = 0;
    virtual void AddPhrase(std::string phrase) = 0;
    virtual void SetWeight(double weight) = 0;
    virtual double GetWeight() = 0;
    virtual void Clear() = 0;
};

SPX_INTERFACE(ISpxGrammar)
{
    public:

    virtual std::list<std::string> GetListenForList() = 0;
};

// Represents a grammar that is persisted in (cloud) storage and has a storage ID available.
SPX_INTERFACE(ISpxStoredGrammar)
{
    public:
    virtual void InitStoredGrammar(const wchar_t* id) = 0;
};

SPX_INTERFACE(ISpxClassLanguageModel)
{
    public:

    virtual void InitClassLanguageModel(const wchar_t* id) = 0;
    virtual void AssignClass(const wchar_t* className, std::shared_ptr<ISpxGrammar> grammar) = 0;
};

SPX_INTERFACE(ISpxGrammarList)
{
    public:

    virtual std::shared_ptr<ISpxGrammar> GetPhraseListGrammar(const wchar_t* name) = 0;
    virtual void AddGrammar(std::shared_ptr<ISpxGrammar> grammar) = 0;
    virtual void SetRecognitionFactor(double factor) = 0;
};

SPX_INTERFACE(ISpxConversationTranscriber)
{
    public:
    virtual void JoinConversation(std::weak_ptr<ISpxConversation> conversation) = 0;
    virtual void LeaveConversation() = 0;
};

SPX_INTERFACE(ISpxConversationTranscriberV2)
{
};

SPX_INTERFACE(ISpxMeetingTranscriber)
{
public:
    virtual void JoinMeeting(std::weak_ptr<ISpxMeeting> meeting) = 0;
    virtual void LeaveMeeting() = 0;
};

SPX_INTERFACE(ISpxObjectWithAudioConfig)
{
    public:
    virtual void SetAudioConfig(std::weak_ptr<ISpxAudioConfig> audioConfig) = 0;
    virtual std::shared_ptr<ISpxAudioConfig> GetAudioConfig() = 0;
};

SPX_INTERFACE(ISpxSpeechEventPayloadProvider)
{
    public:
    virtual std::string GetSpeechEventPayload(bool startMeeting) = 0;
};

SPX_INTERFACE(ISpxTranslationRecognizer)
{
    public:
    virtual void AddTargetLanguage(const std::string& lang) = 0;
    virtual void RemoveTargetLanguage(const std::string& lang) = 0;
};

SPX_INTERFACE(ISpxSourceLanguageRecognizer)
{
};

SPX_INTERFACE(ISpxRecognitionResultProcessor)
{
    public:
    virtual void ProcessResult(std::shared_ptr<ISpxRecognitionResult> result) = 0;
};

SPX_INTERFACE(ISpxSpeechConfig)
{
    public:
    virtual void InitFromSubscription(const char * subscription, const char* region) = 0;
    virtual void InitFromEndpoint(const char * endpoint, const char* subscription) = 0;
    virtual void InitFromHost(const char * host, const char* subscription) = 0;
    virtual void InitAuthorizationToken(const char * authToken, const char * region) = 0;
    virtual void InitEmbedded() = 0;
    virtual void SetServiceProperty(const std::string& name, const std::string& value, ServicePropertyChannel channel) = 0;
    virtual void SetProfanity(ProfanityOption profanity) = 0;
};

SPX_INTERFACE(ISpxSpeechRecognitionModel)
{
    public:
    virtual const std::string& GetName() = 0;
    virtual const std::vector<std::string>& GetLocales() = 0;
    virtual const std::string& GetPath() = 0;
    virtual const std::string& GetVersion() = 0;
};

SPX_INTERFACE(ISpxSpeechRecognitionModelInit)
{
    public:
    virtual void InitModel(std::string&& name, std::vector<std::string>&& locales, std::string&& version) = 0;
    virtual void SetModelPath(std::string&& path) = 0;
};

SPX_INTERFACE(ISpxSpeechTranslationModel)
{
public:
    virtual const std::string& GetName() = 0;
    virtual const std::vector<std::string>& GetSourceLanguages() = 0;
    virtual const std::vector<std::string>& GetTargetLanguages() = 0;
    virtual const std::string& GetDefaultTargetLanguage() = 0;
    virtual const std::string& GetPath() = 0;
    virtual const std::string& GetVersion() = 0;
};

SPX_INTERFACE(ISpxSpeechTranslationModelInit)
{
public:
    virtual void InitModel(
        std::string&& name,
        std::vector<std::string>&& sourceLanguages,
        std::vector<std::string>&& targetLanguages,
        std::string&& defaultTargetLanguage,
        std::string&& version) = 0;
    virtual void SetModelPath(std::string&& path) = 0;
};

SPX_INTERFACE(ISpxEmbeddedSpeechConfig)
{
    public:
    virtual void Init() = 0;
    virtual void AddSearchPath(const char* path) = 0;
    virtual std::string GetSearchPathList() = 0;
    virtual std::uint32_t GetNumSpeechRecognitionModels() = 0;
    virtual std::shared_ptr<ISpxSpeechRecognitionModel> GetSpeechRecognitionModel(uint32_t index) = 0;
    virtual std::shared_ptr<ISpxSpeechRecognitionModel> GetSpeechRecognitionModel(const std::string& name) = 0;
    virtual std::uint32_t GetNumSpeechTranslationModels() = 0;
    virtual std::shared_ptr<ISpxSpeechTranslationModel> GetSpeechTranslationModel(uint32_t index) = 0;
    virtual std::shared_ptr<ISpxSpeechTranslationModel> GetSpeechTranslationModel(const std::string & name) = 0;
    virtual std::shared_ptr<ISpxSpeechRecognitionModel> GetKeywordRecognitionModel(const std::string & name) = 0;
};

SPX_INTERFACE(ISpxSpeechTranslationConfig)
{
    public:
    virtual void AddTargetLanguage(const std::string& lang) = 0;
    virtual void RemoveTargetLanguage(const std::string& lang) = 0;
    virtual void SetCustomModelCategoryId(const std::string & categoryId) = 0;

};

SPX_INTERFACE(ISpxSourceLanguageConfig)
{
    public:
    virtual void InitFromLanguage(const char* language) = 0;
    virtual void InitFromLanguageAndEndpointId(const char* language, const char* endpointId) = 0;
    virtual std::string GetLanguage() = 0;
    virtual std::string GetEndpointId() = 0;
};

SPX_INTERFACE(ISpxAutoDetectSourceLangConfig)
{
    public:
    virtual void InitFromOpenRange() = 0;
    virtual void InitFromLanguages(const char* languages) = 0;
    virtual void AddSourceLanguageConfig(std::shared_ptr<ISpxSourceLanguageConfig> sourceLanguageConfig) = 0;
};

SPX_INTERFACE(ISpxPronunciationAssessmentConfig)
{
    public:
    virtual void InitWithParameters(const char* referenceText, PronunciationAssessmentGradingSystem gradingSystem,
                      PronunciationAssessmentGranularity granularity, bool enableMiscue) = 0;
    virtual void InitFromJson(const char* json) = 0;
    virtual void UpdateJson() = 0;
};

SPX_INTERFACE(ISpxRetrievable)
{
    public:
    virtual void MarkAsRetrieved() noexcept = 0;
    virtual bool WasRetrieved() const noexcept = 0;
};

SPX_INTERFACE(ISpxTelemetryManager)
{
public:
    /// <summary>
    /// If the customer wants to sample telemetry events, the events with Normal category will be sampled.
    /// </summary>
    enum class Category { Normal, Critical };

    virtual void Init(bool useUTCMode, const std::string& telemetryRegion, double samplingRatio) = 0;
    virtual void LogEvent(const std::string &eventName, std::map<std::string, std::string> properties, Category category) = 0;
};

SPX_INTERFACE(ISpxFileCache)
{
public:
    struct CacheItem
    {
        std::string cacheKey;
        std::shared_ptr<std::vector<uint8_t>> data;
        std::shared_ptr<std::string> wordBoundaryData;
    };

    virtual void Init(size_t maxSize, const std::string &cachePath, int64_t cacheCapacityInBytes, int64_t maxSizeOfOneItem) = 0;
    /// <summary>
    /// Put the data into the cache.
    /// </summary>
    /// <param name="text">The text to be cached.</param>
    /// <param name="format">The format of the data.</param>
    /// <param name="data">The data to be cached.</param>
    /// <param name="expires">The expiration time of the cache item.</param>
    /// <param name="wordBoundaryData">The word boundary data to be cached.</param>
    /// <returns>The cache key of the cached item.</returns>
    virtual std::string PutCache(const std::string& text, const std::string& format, const std::shared_ptr<std::vector<uint8_t>>& data, std::chrono::seconds expires, const std::shared_ptr<std::string> wordBoundaryData) = 0;
    virtual CacheItem GetCache(const std::string& text, const std::string& format, bool isWithWordBoundary) = 0;
    virtual void SaveCacheTable() = 0;
};

} } } } // Microsoft::CognitiveServices::Speech::Impl
