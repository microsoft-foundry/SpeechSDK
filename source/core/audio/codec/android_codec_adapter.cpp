//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#include "stdafx.h"

#include "android_codec_adapter.h"
#include "create_object_helpers.h"
#include "interfaces/read_write_buffer_init.h"
#include "site_helpers.h"

#include "media/NdkMediaCodec.h"
#include "media/NdkMediaFormat.h"
#include <exception>


namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

namespace {
// Thread-local markers used by Close() to reject calls that would self-join or deadlock.
// They are per-thread, so reading them needs no locking and does not touch the std::thread
// object that another thread may be joining at the same time.

// currentAndroidCodecWorker: the adapter whose decoder worker lambda is running on this
// thread (set by AndroidCodecWorkerContext for the lifetime of that lambda). Calling Close()
// on that adapter from this thread would make the thread join itself.
thread_local const CSpxAndroidCodecAdapter* currentAndroidCodecWorker = nullptr;

class AndroidCodecWorkerContext
{
public:
    explicit AndroidCodecWorkerContext(const CSpxAndroidCodecAdapter* adapter)
        : previous(currentAndroidCodecWorker) { currentAndroidCodecWorker = adapter; }
    ~AndroidCodecWorkerContext() { currentAndroidCodecWorker = previous; }
private:
    const CSpxAndroidCodecAdapter* previous;
};

// currentAndroidCodecClose: stack (a linked list through `previous`) of the adapters whose
// Close() is currently executing on this thread. If the input-close callback ends up calling
// Close() on the same adapter again -- directly, or through another adapter's Close() nested
// in between -- Contains() finds it here and the call is rejected instead of deadlocking on
// m_closeMutex. Forward-declared because the thread_local pointer is declared before the
// class body that it points to.
class AndroidCodecCloseContext;
thread_local const AndroidCodecCloseContext* currentAndroidCodecClose = nullptr;

class AndroidCodecCloseContext
{
public:
    explicit AndroidCodecCloseContext(const CSpxAndroidCodecAdapter* value)
        : adapter(value), previous(currentAndroidCodecClose) { currentAndroidCodecClose = this; }
    ~AndroidCodecCloseContext() { currentAndroidCodecClose = previous; }
    static bool Contains(const CSpxAndroidCodecAdapter* value)
    {
        for (auto context = currentAndroidCodecClose; context; context = context->previous)
            if (context->adapter == value) return true;
        return false;
    }
private:
    const CSpxAndroidCodecAdapter* adapter;
    const AndroidCodecCloseContext* previous;
};

}

CSpxAndroidCodecAdapter::CSpxAndroidCodecAdapter()
{
    SPX_DBG_TRACE_VERBOSE("%s", __FUNCTION__);
}

// The destructor is the last line of defense. Owners are expected to call Close() explicitly
// (CSpxSynthesizer::ExecuteSynthesis does), but if that did not happen, Close() here stops
// and joins the decoder worker, so the thread can never outlive the members it dereferences
// (formats, callbacks, error state).
CSpxAndroidCodecAdapter::~CSpxAndroidCodecAdapter()
{
    try
    {
        Close();
    }
    catch (...)
    {
        // Callback exceptions may be suppressed only after worker quiescence.
        // A failed/self join cannot be made safe by detaching the worker.
        if (m_worker.joinable()) std::terminate();
        SPX_TRACE_ERROR("Android codec destructor suppressed input close callback exception after joining");
    }
}

void CSpxAndroidCodecAdapter::SetFormat(const SPXWAVEFORMATEX* format)
{
    SPX_THROW_HR_IF(SPXERR_ALREADY_INITIALIZED, m_format != nullptr);

    // Allocate the buffer for the format
    auto formatSize = sizeof(SPXWAVEFORMATEX) + format->cbSize;
    m_format = SpxAllocWAVEFORMATEX(formatSize);

    // Copy the format
    memcpy(m_format.get(), format, formatSize);

    SetError("");

    // Evaluates `expression` exactly once, optionally traces it, and on failure records the
    // error (via the thread-safe SetError) and returns from the enclosing function. The return
    // leaves the worker lambda, so the macro is only valid inside it; owned resources are
    // released by the unique_ptr guards. Undefined after the lambda below.
    #define CHECK_MEDIA_STATUS(expression) \
        { \
            const auto mediaStatus = (expression); \
            if (mediaStatus != AMEDIA_OK) \
            { \
                SPX_TRACE_ERROR("failed with media status error code %d", mediaStatus); \
                SetError("MediaCodec error: " + std::to_string(mediaStatus)); \
                return; \
            } \
        }

    // Run decoding on a worker thread that this object owns (m_worker).
    // The worker is joined in Close() (called from the destructor and by the
    // synthesizer), so every member the lambda touches outlives the thread. The lambda
    // still captures `this`; its safety rests on that join, not on the capture.
    m_worker = std::thread([this]() {
        // Mark this thread as the adapter's worker, so that Close() can reject a self-join.
        AndroidCodecWorkerContext workerContext(this);
        // Any exception (from this code or from the read/write/close callbacks) is
        // caught at the bottom of this lambda and converted into the error message
        // that Read() reports to the owner.
        try
        {
        std::string mime;
        uint32_t inputBufferSize;
        if (this->m_sourceFormat->wFormatTag == WAVE_FORMAT_MP3)
        {
            mime = "audio/mpeg";
            // The mp3 frame size is 1152 samples per channel, see https://wiki.hydrogenaud.io/index.php?title=MP3#Polyphase_Filterbank_Formula
            // So the buffer size should be a multiple of 1152 / 8 = 144 (bytes).
            // Here we ues 8 frames to avoid small audio chunks.
            inputBufferSize = 720;
        }
        else if (this->m_sourceFormat->wFormatTag == WAVE_FORMAT_OPUS)
        {
            mime = "audio/opus";
            // Shoule align with service side,
            // https://gstreamer.freedesktop.org/documentation/opus/opusenc.html?gi-language=c#opusenc:frame-size
            inputBufferSize = this->m_sourceFormat->nAvgBytesPerSec * 20 / 1000; // 20ms length,
        }
        else
        {
            SetError("Only mp3 and opus (without container) formats are supported for decoding on Android.");
            return;
        }

        // RAII ownership of the input MediaFormat, so it is freed on every return and on exceptions.
        auto formatOwner = std::unique_ptr<AMediaFormat, decltype(&AMediaFormat_delete)>(
            AMediaFormat_new(), &AMediaFormat_delete);
        auto mediaFormat = formatOwner.get();
        if (mediaFormat == nullptr)
        {
            SPX_TRACE_ERROR("Failed to create media format.");
            SetError("Failed to create media format.");
            return;
        }

        AMediaFormat_setString(mediaFormat, AMEDIAFORMAT_KEY_MIME, mime.c_str());
        AMediaFormat_setInt32(mediaFormat, AMEDIAFORMAT_KEY_SAMPLE_RATE, this->m_sourceFormat->nSamplesPerSec);
        AMediaFormat_setInt32(mediaFormat, AMEDIAFORMAT_KEY_CHANNEL_COUNT, this->m_sourceFormat->nChannels);

        if (this->m_sourceFormat->wFormatTag == WAVE_FORMAT_OPUS)
        {
            // Opus header, see https://datatracker.ietf.org/doc/html/rfc7845#section-5.1
            // Header, pre-skip, and pre-roll should align with service opus setting.
            uint8_t opusHeader[19] = {
                // "Opus"
                0x4f, 0x70, 0x75, 0x73,
                // "Head"
                0x48, 0x65, 0x61, 0x64,
                // Version
                0x01,
                // Channel Count
                0x01,
                // Pre skip: 312 samples at 48kHz - 6.5 ms
                0x38, 0x01,
                // Input Sample Rate (Hz), eg: 16000
                0x80, 0x3e, 0x00, 0x00,
                // Output Gain (Q7.8 in dB)
                0x00, 0x00,
                // Mapping Family
                0x00};
            reinterpret_cast<uint32_t *>(opusHeader + 12)[0] = this->m_sourceFormat->nSamplesPerSec;
            AMediaFormat_setBuffer(mediaFormat, "csd-0", reinterpret_cast<void *>(opusHeader), sizeof(opusHeader));
            uint64_t preSkipNs = 6500000;
            AMediaFormat_setBuffer(mediaFormat, "csd-1", reinterpret_cast<void *>(&preSkipNs), sizeof(preSkipNs));
            uint64_t preRollNs = 80000000;
            AMediaFormat_setBuffer(mediaFormat, "csd-2", reinterpret_cast<void *>(&preRollNs), sizeof(preRollNs));
        }
        SPX_TRACE_INFO("CSpxAndroidCodecAdapter: decoding codec %s.", AMediaFormat_toString(mediaFormat));
        auto mediaCodec = AMediaCodec_createDecoderByType(mime.c_str());
        if (mediaCodec == nullptr)
        {
            SPX_TRACE_ERROR("Failed to create media codec.");
            SetError("Failed to create media codec.");
            return;
        }

        // RAII ownership of the codec. AMediaCodec_stop() is called only if start() succeeded
        // (codecStarted); delete always runs.
        bool codecStarted = false;
        auto deleteCodec = [&codecStarted](AMediaCodec* codec) {
            if (codecStarted) AMediaCodec_stop(codec);
            AMediaCodec_delete(codec);
        };
        auto codecOwner = std::unique_ptr<AMediaCodec, decltype(deleteCodec)>(mediaCodec, deleteCodec);
        CHECK_MEDIA_STATUS(AMediaCodec_configure(mediaCodec, mediaFormat, nullptr, nullptr, 0));
        // Free the input format right after configure().
        formatOwner.reset();
        CHECK_MEDIA_STATUS(AMediaCodec_start(mediaCodec));
        codecStarted = true;

        const auto timeoutUs = 2000;
        auto sawInputEOS = false;
        auto inputIdx = 0;
        auto decodingStarted = false;
        int downsampleRatio = 1;
        // Position within the current group of `downsampleRatio` decoded samples. Kept across
        // output buffers so that sample selection does not restart at every packet boundary.
        int downsamplePhase = 0;
        // The sleep interval is audio duration per decoded frame / 3 - decoding time.
        // This targets a real-time-factor of 0.3.
        const auto sleepIntervalMs = (inputBufferSize * 1000 / this->m_sourceFormat->nAvgBytesPerSec / 3) - (2 * timeoutUs / 1000);

        // m_stopRequested is set by Close(). The loop and the checks after the blocking callbacks
        // look at it so the worker exits promptly and can be joined.
        while (!m_stopRequested)
        {
            if (!sawInputEOS)
            {
                auto bufidx = AMediaCodec_dequeueInputBuffer(mediaCodec, timeoutUs);
                if (bufidx >= 0)
                {
                    size_t bufsize = 0;
                    auto buf = AMediaCodec_getInputBuffer(mediaCodec, bufidx, &bufsize);
                    if (buf == nullptr || bufsize == 0 || inputBufferSize == 0)
                    {
                        SetError("Invalid MediaCodec input buffer.");
                        break;
                    }
                    // Never ask the read callback for more than the codec's input buffer can hold:
                    // the callback writes straight into `buf`.
                    const auto readSize = bufsize < inputBufferSize ? static_cast<uint32_t>(bufsize) : inputBufferSize;
                    auto sampleSize = this->m_readCallback(buf, readSize);
                    // Close() may have unblocked the read callback, which then returns a partial
                    // or empty result. Do not queue it; leave the loop so that Close() can join.
                    if (m_stopRequested) break;
                    // Validate the callback's return value before queueing it: it is used as the
                    // number of valid bytes in the codec's input buffer (0 means end of input).
                    if (sampleSize < 0)
                    {
                        SetError("Input callback returned negative MediaCodec buffer size.");
                        break;
                    }
                    if (static_cast<uint32_t>(sampleSize) > readSize)
                    {
                        SetError("Input callback exceeded requested MediaCodec buffer size.");
                        break;
                    }
                    if (sampleSize == 0)
                    {
                        sawInputEOS = true;
                    }
                    else if (this->m_isThrottlingEnabled && inputIdx > 10)
                    {
                        // Sleep between frames to avoid CPU overload on Android. Leave first 10 frames to avoid the initial delay.
                        SPX_TRACE_VERBOSE("CSpxAndroidCodecAdapter: throttling enabled, sleeping %d ms.", sleepIntervalMs);
                        std::this_thread::sleep_for(std::chrono::milliseconds(sleepIntervalMs));
                    }
                    auto presentationTimeUs = inputBufferSize * inputIdx++ * 1000000 / this->m_sourceFormat->nAvgBytesPerSec;
                    CHECK_MEDIA_STATUS(AMediaCodec_queueInputBuffer(mediaCodec, bufidx, 0, sampleSize, presentationTimeUs,
                                                                    sawInputEOS ? AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM : 0));
                }
            }
            AMediaCodecBufferInfo info;
            auto status = AMediaCodec_dequeueOutputBuffer(mediaCodec, &info, timeoutUs);
            if(status == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
                // The output format has changed. We can get the decoded data by continuing to dequeue once, which reduces the time to wait for the next audio data to be obtained and decoded.
                status = AMediaCodec_dequeueOutputBuffer(mediaCodec, &info, timeoutUs);
            }
            if (status >= 0)
            {
                // Every dequeued output slot must be given back to the codec exactly once,
                // on every path: normal, `break`, `continue`, return and exception. The guard
                // below does that from its destructor and records the status. On the normal paths
                // the slot is released explicitly (outputSlot.reset()) so a release failure can
                // be checked and reported before EOS is signaled; on error paths the status is
                // not checked, because an error is already being reported.
                media_status_t releaseStatus = AMEDIA_OK;
                auto releaseOutput = [status, &releaseStatus](AMediaCodec* codec) {
                    releaseStatus = AMediaCodec_releaseOutputBuffer(codec, status, false);
                };
                auto outputSlot = std::unique_ptr<AMediaCodec, decltype(releaseOutput)>(mediaCodec, releaseOutput);
                const bool outputEOS = (info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) != 0;
                // Reject nonsensical ranges before they are cast to unsigned below.
                if (info.offset < 0 || info.size < 0)
                {
                    SetError("Invalid MediaCodec output buffer range.");
                    break;
                }
                // A pure end-of-stream marker without payload: release the slot, then signal that
                // decoding is complete. An EOS buffer that also carries PCM is handled further down,
                // so that its audio is not dropped.
                if (outputEOS && info.size == 0)
                {
                    outputSlot.reset();
                    CHECK_MEDIA_STATUS(releaseStatus);
                    SPX_TRACE_INFO("CSpxAndroidCodecAdapter: Decoding output ends.");
                    this->m_writerCloseCallback();
                    break;
                }

                if (!decodingStarted)
                {
                    decodingStarted = true;
                    auto outputFormatOwner = std::unique_ptr<AMediaFormat, decltype(&AMediaFormat_delete)>(
                        AMediaCodec_getOutputFormat(mediaCodec), &AMediaFormat_delete);
                    auto format = outputFormatOwner.get();
                    if (!format) { SetError("Failed to get MediaCodec output format."); break; }
                    int32_t decodingOutputSampleRate = 0;
                    AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_SAMPLE_RATE, &decodingOutputSampleRate);
                    SPX_TRACE_INFO("CSpxAndroidCodecAdapter: output format: %s.", AMediaFormat_toString(format));
                    if (decodingOutputSampleRate != static_cast<int32_t>(this->m_format->nSamplesPerSec))
                    {
                        SPX_TRACE_INFO("CSpxAndroidCodecAdapter: Downsampling after decoding from %d to %d.",
                                       decodingOutputSampleRate, this->m_format->nSamplesPerSec);
                        // Only an integer ratio can be handled by dropping samples. Check that
                        // the divisor is non-zero and divides evenly before dividing.
                        if (decodingOutputSampleRate <= 0 || this->m_format->nSamplesPerSec == 0 ||
                            static_cast<uint32_t>(decodingOutputSampleRate) % this->m_format->nSamplesPerSec != 0)
                        {
                            SetError("Downsampling ratio is not supported");
                            break;
                        }
                        // Ratio = decoded rate / REQUESTED output rate (m_format).
                        // Only ratios 2 and 3 are accepted.
                        downsampleRatio = decodingOutputSampleRate / this->m_format->nSamplesPerSec;
                        if (downsampleRatio != 2 && downsampleRatio != 3)
                        {
                            SetError("Downsampling ratio is not supported");
                            break;
                        }
                    }
                }

                size_t bufsize;
                auto buf = AMediaCodec_getOutputBuffer(mediaCodec, status, &bufsize);
                // Avoid dereferencing a null buffer. The slot guard releases on every exit.
                if (buf == nullptr)
                {
                    // A null, zero-sized non-EOS output is unexpected in buffer mode,
                    // so release the dequeued buffer and continue decoding.
                    if (info.size == 0)
                    {
                        SPX_TRACE_WARNING("CSpxAndroidCodecAdapter: MediaCodec returned a null output buffer with size 0.");
                        outputSlot.reset();
                        CHECK_MEDIA_STATUS(releaseStatus);
                        continue;
                    }

                    SPX_TRACE_ERROR("CSpxAndroidCodecAdapter: Failed to get output buffer with size %d.", info.size);
                    SetError("Failed to get MediaCodec output buffer with size " + std::to_string(info.size) + ".");
                    break;
                }
                const auto offset = static_cast<size_t>(info.offset);
                const auto size = static_cast<size_t>(info.size);
                if (offset > bufsize || size > bufsize - offset)
                {
                    SetError("Invalid MediaCodec output buffer range.");
                    break;
                }
                // The valid PCM starts at info.offset inside the buffer.
                buf += offset;
                // The Nyquist bandwidth should be align with request output sample rate, so filter is not needed here.
                uint32_t outputSize = static_cast<uint32_t>(info.size);
                if (downsampleRatio > 1)
                {
                    // Sample selection works on whole 16-bit samples (mono is assumed).
                    // Refuse a buffer that ends in the middle of a sample.
                    if (info.size % 2 != 0)
                    {
                        SetError("Incomplete 16-bit MediaCodec output sample.");
                        break;
                    }
                    // Keep every `downsampleRatio`-th sample, compacting in place (the write index
                    // never passes the read index). downsamplePhase carries over between buffers.
                    // outputSize below is the exact byte count of the kept samples.
                    uint32_t selectedSamples = 0;
                    for (uint32_t i = 0; i < static_cast<uint32_t>(info.size) / 2; ++i)
                    {
                        if (downsamplePhase == 0)
                        {
                            buf[selectedSamples * 2] = buf[i * 2];
                            buf[selectedSamples * 2 + 1] = buf[i * 2 + 1];
                            ++selectedSamples;
                        }
                        if (++downsamplePhase == downsampleRatio) downsamplePhase = 0;
                    }
                    outputSize = selectedSamples * 2;
                }
                // Once Close() has requested stop, do not deliver more audio to the write callback
                // (it reaches synthesizer state). Empty output is skipped.
                if (!m_stopRequested && outputSize != 0)
                    this->m_writeCallback(buf, outputSize);

                // Release the slot before reporting EOS, including a final nonempty payload.
                outputSlot.reset();
                CHECK_MEDIA_STATUS(releaseStatus);
                SPX_TRACE_VERBOSE("CSpxAndroidCodecAdapter: decode outputs, size %u.", outputSize);
                if (outputEOS)
                {
                    SPX_TRACE_INFO("CSpxAndroidCodecAdapter: Decoding output ends.");
                    this->m_writerCloseCallback();
                    break;
                }
            }
            else if (status == AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED)
            {
                SPX_TRACE_INFO("CSpxAndroidCodecAdapter: output buffers changed.");
            }
            else if (status == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED)
            {
                auto outputFormatOwner = std::unique_ptr<AMediaFormat, decltype(&AMediaFormat_delete)>(
                    AMediaCodec_getOutputFormat(mediaCodec), &AMediaFormat_delete);
                auto format = outputFormatOwner.get();
                if (!format) { SetError("Failed to get MediaCodec output format."); break; }
                SPX_TRACE_INFO("CSpxAndroidCodecAdapter: output format changed to: %s.", AMediaFormat_toString(format));
            }
            else if (status == AMEDIACODEC_INFO_TRY_AGAIN_LATER)
            {
                SPX_TRACE_VERBOSE("CSpxAndroidCodecAdapter: no output buffer right now.");
            }
            else
            {
                SPX_TRACE_WARNING("CSpxAndroidCodecAdapter: unexpected info code: %zd.", status);
                SetError("Unexpected info code: " + std::to_string(status));
                break;
            }
        }

        }
        catch (const std::exception& e)
        {
            // Surface the failure to the owner: Read() rethrows this message from
            // the synthesizer's wait loop.
            SetError(e.what());
        }
        catch (...)
        {
            // Non-std::exception types (e.g. thrown by foreign code under a callback).
            SetError("Unknown exception in Android codec worker.");
        }
    });
    #undef CHECK_MEDIA_STATUS
}

void CSpxAndroidCodecAdapter::SetSourceFormat(SPXWAVEFORMATEX* format)
{
    SPX_THROW_HR_IF(SPXERR_ALREADY_INITIALIZED, m_sourceFormat != nullptr);

    // Allocate the buffer for the format
    auto formatSize = sizeof(SPXWAVEFORMATEX) + format->cbSize;
    m_sourceFormat = SpxAllocWAVEFORMATEX(formatSize);

    // Copy the format
    memcpy(m_sourceFormat.get(), format, formatSize);
}

uint16_t CSpxAndroidCodecAdapter::GetFormat(SPXWAVEFORMATEX* format, uint16_t)
{
    if (format != nullptr)
    {
        format->wFormatTag = WAVE_FORMAT_PCM;
        format->nChannels = m_format->nChannels;
        format->nSamplesPerSec = m_format->nSamplesPerSec;
        format->nAvgBytesPerSec = m_format->nSamplesPerSec * (m_format->wBitsPerSample >> 3) * m_format->nChannels;
        format->nBlockAlign = m_format->nChannels * (m_format->wBitsPerSample >> 3);
        format->wBitsPerSample = m_format->wBitsPerSample;
        format->cbSize = 0;
    }
    return sizeof(SPXWAVEFORMATEX);
}

// Not a real read: the adapter has no data to return. The synthesizer polls this with
// (nullptr, 0) while waiting for decoding to finish, to learn about worker failures:
// any error recorded by the worker is rethrown here. m_errorMutex makes this safe
// against the worker writing the message at the same time (std::string is not thread-safe).
uint32_t CSpxAndroidCodecAdapter::Read(uint8_t*, uint32_t bytesToRead)
{
    std::lock_guard<std::mutex> lock(m_errorMutex);
    if (!m_errorMessage.empty())
    {
        throw std::runtime_error(m_errorMessage);
    }
    return bytesToRead;
}

// Stops the decoder worker and waits for it to exit. Sequence:
//   1. Reject calls that would self-join or deadlock (from the worker itself, or recursively
//      from the input-close callback) before touching m_closeMutex.
//   2. Under m_closeMutex (serializes concurrent owner-thread closes) the first caller sets
//      m_stopRequested and runs the input-close callback once. The callback must unblock
//      a read the worker may be blocked in (the synthesizer closes the codec input buffer).
//   3. Join the worker. An exception from the callback is held until after the join and then
//      rethrown, so the worker is quiescent even when Close() reports a failure.
// Repeated Close() calls just return once the worker has been joined. It can block
// for as long as the worker is inside a callback or a throttling sleep.
void CSpxAndroidCodecAdapter::Close()
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    // Close is an owner-thread operation; decoder callbacks must not close
    // or destroy the adapter. Never detach to work around a self-join.
    // Reject before locking: an owner may hold this mutex while joining us.
    // Thread-local context avoids racing with join on the std::thread object.
    if (currentAndroidCodecWorker == this)
        throw std::logic_error("Android codec cannot be closed from its worker thread");
    if (AndroidCodecCloseContext::Contains(this))
        throw std::logic_error("Android codec cannot be closed reentrantly from its close callback");
    // Record that this thread is inside Close() of this adapter (see Contains() above).
    AndroidCodecCloseContext closeContext(this);
    std::lock_guard<std::mutex> lock(m_closeMutex);
    // Defensive; redundant with the thread-local worker check above. It is safe to look at
    // m_worker here because m_closeMutex is held, so no other Close() can be joining it.
    if (m_worker.joinable() && m_worker.get_id() == std::this_thread::get_id())
        throw std::logic_error("Android codec cannot be closed from its worker thread");
    std::exception_ptr closeError;
    // exchange() makes only the first Close() run the input-close callback.
    if (!m_stopRequested.exchange(true) && m_closeCallback != nullptr)
    {
        try { m_closeCallback(); } // Must unblock a pending read before joining.
        catch (...) { closeError = std::current_exception(); }
    }
    if (m_worker.joinable())
        m_worker.join();
    if (closeError) std::rethrow_exception(closeError);
}

// Single write path for the error text shared between the worker (writer) and Read()
// (reader on the owner thread). The last message wins; an empty message means no error.
void CSpxAndroidCodecAdapter::SetError(const std::string& message)
{
    std::lock_guard<std::mutex> lock(m_errorMutex);
    m_errorMessage = message;
}

void CSpxAndroidCodecAdapter::SetCallbacks(ReadCallbackFunction_Type readCallback, ISpxAudioStreamReaderInitCallbacks::CloseCallbackFunction_Type closeCallback)
{
    m_readCallback = readCallback;
    m_closeCallback = closeCallback;
}

void CSpxAndroidCodecAdapter::SetPropertyCallback2(GetPropertyCallbackFunction_Type2 getPropertyCallBack)
{
    m_getPropertyCallback = getPropertyCallBack;
}

void CSpxAndroidCodecAdapter::SetWriterCallbacks(WriteCallbackFunction_Type writeCallback, ISpxAudioStreamWriterInitCallbacks::CloseCallbackFunction_Type closeCallback)
{
    m_writeCallback = writeCallback;
    m_writerCloseCallback = closeCallback;
}

SPXSTRING CSpxAndroidCodecAdapter::GetProperty(PropertyId propertyId)
{
    return m_getPropertyCallback(propertyId);
}

void CSpxAndroidCodecAdapter::EnableThrottling(bool enableThrottling)
{
    m_isThrottlingEnabled = enableThrottling;
}

} } } } // Microsoft::CognitiveServices::Speech::Impl
