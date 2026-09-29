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


namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {


CSpxAndroidCodecAdapter::CSpxAndroidCodecAdapter()
{
    SPX_DBG_TRACE_VERBOSE("%s", __FUNCTION__);
}

CSpxAndroidCodecAdapter::~CSpxAndroidCodecAdapter()
{
}

void CSpxAndroidCodecAdapter::SetFormat(const SPXWAVEFORMATEX* format)
{
    SPX_THROW_HR_IF(SPXERR_ALREADY_INITIALIZED, m_format != nullptr);

    // Allocate the buffer for the format
    auto formatSize = sizeof(SPXWAVEFORMATEX) + format->cbSize;
    m_format = SpxAllocWAVEFORMATEX(formatSize);

    // Copy the format
    memcpy(m_format.get(), format, formatSize);

    m_errorMessage = "";

    #define CHECK_MEDIA_STATUS(status) \
        if (status != AMEDIA_OK) \
        { \
            SPX_TRACE_ERROR("failed with meida status error code %d", status); \
            this->m_errorMessage = "MediaCodec error: " + std::to_string(status); \
            return; \
        }

    // Run in another thread
    std::thread([this]() {
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
            this->m_errorMessage = "Only mp3 and opus (without container) formats are supported for decoding on Android.";
            return;
        }

        AMediaFormat* mediaFormat = AMediaFormat_new();
        if (mediaFormat == nullptr)
        {
            SPX_TRACE_ERROR("Failed to create media format.");
            this->m_errorMessage = "Failed to create media format.";
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
            this->m_errorMessage = "Failed to create media codec.";
            return;
        }

        CHECK_MEDIA_STATUS(AMediaCodec_configure(mediaCodec, mediaFormat, nullptr, nullptr, 0));
        CHECK_MEDIA_STATUS(AMediaFormat_delete(mediaFormat));
        CHECK_MEDIA_STATUS(AMediaCodec_start(mediaCodec));

        const auto timeoutUs = 2000;
        auto sawInputEOS = false;
        auto inputIdx = 0;
        auto decodingStarted = false;
        int downsampleRatio = 1;
        // The sleep interval is audio duration per decoded frame / 3 - decoding time.
        // This targets a real-time-factor of 0.3.
        const auto sleepIntervalMs = (inputBufferSize * 1000 / this->m_sourceFormat->nAvgBytesPerSec / 3) - (2 * timeoutUs / 1000);

        while (true)
        {
            if (!sawInputEOS)
            {
                auto bufidx = AMediaCodec_dequeueInputBuffer(mediaCodec, timeoutUs);
                if (bufidx >= 0)
                {
                    size_t bufsize;
                    auto buf = AMediaCodec_getInputBuffer(mediaCodec, bufidx, &bufsize);
                    auto sampleSize = this->m_readCallback(buf, inputBufferSize);
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
                if (info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM)
                {
                    SPX_TRACE_INFO("CSpxAndroidCodecAdapter: Decoding output ends.");
                    this->m_writerCloseCallback();
                    break;
                }

                if (!decodingStarted)
                {
                    decodingStarted = true;
                    auto format = AMediaCodec_getOutputFormat(mediaCodec);
                    int32_t decodingOutputSampleRate = 0;
                    AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_SAMPLE_RATE, &decodingOutputSampleRate);
                    SPX_TRACE_INFO("CSpxAndroidCodecAdapter: output format: %s.", AMediaFormat_toString(format));
                    if (decodingOutputSampleRate != static_cast<int32_t>(this->m_format->nSamplesPerSec))
                    {
                        SPX_TRACE_INFO("CSpxAndroidCodecAdapter: Downsampling after decoding from %d to %d.",
                                       decodingOutputSampleRate, this->m_format->nSamplesPerSec);
                        downsampleRatio = decodingOutputSampleRate / this->m_sourceFormat->nSamplesPerSec;
                        if (downsampleRatio != 2 && downsampleRatio != 3)
                        {
                            this->m_errorMessage = "Downsampling ratio is not supported";
                            break;
                        }
                    }
                    AMediaFormat_delete(format);
                }

                size_t bufsize;
                auto buf = AMediaCodec_getOutputBuffer(mediaCodec, status, &bufsize);
                // Avoid dereferencing a null buffer. Release the dequeued slot, skip empty output,
                // and fail if the codec reports data without returning an accessible buffer.
                if (buf == nullptr)
                {
                    AMediaCodec_releaseOutputBuffer(mediaCodec, status, false);
                    // EOS is handled above. A null, zero-sized non-EOS output is unexpected in buffer mode,
                    // so release the dequeued buffer and continue decoding.
                    if (info.size == 0)
                    {
                        SPX_TRACE_WARNING("CSpxAndroidCodecAdapter: MediaCodec returned a null output buffer with size 0.");
                        continue;
                    }

                    SPX_TRACE_ERROR("CSpxAndroidCodecAdapter: Failed to get output buffer with size %d.", info.size);
                    this->m_errorMessage = "Failed to get MediaCodec output buffer with size " + std::to_string(info.size) + ".";
                    break;
                }
                // The Nyquist bandwidth should be align with request output sample rate, so filter is not needed here.
                if (downsampleRatio > 1)
                {
                    for (auto i = 0; i < info.size / (2 * downsampleRatio); i++)
                    {
                        buf[2 * i] = buf[2 * downsampleRatio * i];
                        buf[2 * i + 1] = buf[2 * downsampleRatio * i + 1];
                    }
                }
                this->m_writeCallback(buf, info.size / downsampleRatio);

                // No output surface is configured, so return the buffer to the codec without rendering.
                AMediaCodec_releaseOutputBuffer(mediaCodec, status, false);
                SPX_TRACE_VERBOSE("CSpxAndroidCodecAdapter: decode outputs, size %d.", info.size / downsampleRatio);
            }
            else if (status == AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED)
            {
                SPX_TRACE_INFO("CSpxAndroidCodecAdapter: output buffers changed.");
            }
            else if (status == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED)
            {
                auto format = AMediaCodec_getOutputFormat(mediaCodec);
                SPX_TRACE_INFO("CSpxAndroidCodecAdapter: output format changed to: %s.", AMediaFormat_toString(format));
                AMediaFormat_delete(format);
            }
            else if (status == AMEDIACODEC_INFO_TRY_AGAIN_LATER)
            {
                SPX_TRACE_VERBOSE("CSpxAndroidCodecAdapter: no output buffer right now.");
            }
            else
            {
                SPX_TRACE_WARNING("CSpxAndroidCodecAdapter: unexpected info code: %zd.", status);
                m_errorMessage = "Unexpected info code: " + std::to_string(status);
                break;
            }
        }

        AMediaCodec_stop(mediaCodec);
        AMediaCodec_delete(mediaCodec);
    }).detach();
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

uint32_t CSpxAndroidCodecAdapter::Read(uint8_t*, uint32_t bytesToRead)
{
    if (!m_errorMessage.empty())
    {
        throw std::runtime_error(m_errorMessage);
    }
    return bytesToRead;
}

void CSpxAndroidCodecAdapter::Close()
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    if (m_closeCallback != nullptr)
    {
        m_closeCallback();
    }
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
