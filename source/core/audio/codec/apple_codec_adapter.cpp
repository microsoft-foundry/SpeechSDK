//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"

#include "apple_codec_adapter.h"
#include "create_object_helpers.h"
#include "site_helpers.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {


CSpxAppleCodecAdapter::CSpxAppleCodecAdapter()
{
    SPX_DBG_TRACE_VERBOSE("%s", __FUNCTION__);
}

CSpxAppleCodecAdapter::~CSpxAppleCodecAdapter()
{
}

void CSpxAppleCodecAdapter::SetFormat(const SPXWAVEFORMATEX* format)
{
    SPX_THROW_HR_IF(SPXERR_ALREADY_INITIALIZED, m_format != nullptr);

    // Allocate the buffer for the format
    auto formatSize = sizeof(SPXWAVEFORMATEX) + format->cbSize;
    m_format = SpxAllocWAVEFORMATEX(formatSize);

    // Copy the format
    memcpy(m_format.get(), format, formatSize);

    m_errorMessage = "";

    // Run in another thread
    std::thread([this]() {
        // Apple has better supports for AAC and mp3 formats, but our service doesn't suppport AAC.
        if (this->m_sourceFormat->wFormatTag != WAVE_FORMAT_MP3)
        {
            this->m_errorMessage = "Only mp3 format is supported for decoding on Apple devices.";
            return;
        }

        AudioStreamBasicDescription inFormat{};
        inFormat.mSampleRate        = this->m_sourceFormat->nSamplesPerSec;
        inFormat.mFormatID          = kAudioFormatMPEGLayer3;
        // Set to 0, need to update AudioStreamPacketDescription in the callback
        inFormat.mBytesPerPacket    = 0;
        inFormat.mChannelsPerFrame  = this->m_sourceFormat->nChannels;

        AudioStreamBasicDescription outFormat{};
        outFormat.mSampleRate       = this->m_format->nSamplesPerSec;
        outFormat.mFormatID         = kAudioFormatLinearPCM;
        outFormat.mFormatFlags      = kLinearPCMFormatFlagIsSignedInteger;
        outFormat.mBytesPerPacket   = 2 * this->m_format->nChannels; /* 2 */
        outFormat.mFramesPerPacket  = 1;
        outFormat.mBytesPerFrame    = 2 * this->m_format->nChannels; /* 2 */
        outFormat.mChannelsPerFrame = this->m_format->nChannels; /* 1 */
        outFormat.mBitsPerChannel   = this->m_format->wBitsPerSample; /* 16 */
        outFormat.mReserved         = 0;

        // Only tested for mono.
        SPX_DBG_ASSERT(outFormat.mChannelsPerFrame == 1);

        AudioConverterRef audioConverter = nullptr;

        SPX_TRACE_VERBOSE("input format: sampleRate %f, bytesPerPacket %u", inFormat.mSampleRate, inFormat.mBytesPerPacket);
        SPX_TRACE_VERBOSE("output format: sampleRate %f, bytesPerPacket %u", outFormat.mSampleRate, outFormat.mBytesPerPacket);

        OSStatus status =  AudioConverterNew(&inFormat, &outFormat, &audioConverter);
        if (status != 0) {
            SPX_TRACE_INFO("setup converter error, status: %i\n", (int)status);
        }

        UInt32 size = sizeof(inFormat);
        AudioConverterGetProperty(audioConverter, kAudioConverterCurrentInputStreamDescription, &size, &inFormat);
        AudioConverterGetProperty(audioConverter, kAudioConverterCurrentOutputStreamDescription, &size, &outFormat);

        SPX_TRACE_VERBOSE("input format: sampleRate %f, bytesPerPacket %u", inFormat.mSampleRate, inFormat.mBytesPerPacket);
        SPX_TRACE_VERBOSE("output format: sampleRate %f, bytesPerPacket %u", outFormat.mSampleRate, outFormat.mBytesPerPacket);

        auto decodingLength = this->m_format->nSamplesPerSec / 10; /* 100 ms */
        auto decodingBuffer = SpxAllocSharedAudioBuffer(decodingLength * outFormat.mBytesPerPacket);
        this->m_inputBufferSize = 576;
        this->m_inputBuffer = SpxAllocSharedAudioBuffer(m_inputBufferSize * 2);

        while (true)
        {
            AudioBufferList outBufferList;
            outBufferList.mNumberBuffers = 1;
            outBufferList.mBuffers[0].mNumberChannels = 1;
            outBufferList.mBuffers[0].mDataByteSize = decodingLength * outFormat.mBytesPerPacket;
            outBufferList.mBuffers[0].mData = decodingBuffer.get();

            UInt32 ioOutputDataPackets = decodingLength;
            OSStatus error = AudioConverterFillComplexBuffer(audioConverter, CSpxAppleCodecAdapter::InputDataProc, this, &ioOutputDataPackets, &outBufferList, nullptr);
            if (error)
            {
                SPX_TRACE_ERROR("Error decoding audio stream: %d\n", (int)error);
                m_errorMessage = "Error decoding audio stream, error code: " + std::to_string((int)error);
                break;
            }

            if (ioOutputDataPackets > 0)
            {
                SPX_TRACE_VERBOSE("decoded %u packets.", ioOutputDataPackets);
                this->m_writeCallback((const uint8_t*)outBufferList.mBuffers[0].mData, outBufferList.mBuffers[0].mDataByteSize);
            }
            else
            {
                this->m_writerCloseCallback();
                break;
            }
        }

        if (audioConverter)
        {
            AudioConverterDispose(audioConverter);
        }

    }).detach();
}

OSStatus CSpxAppleCodecAdapter::InputDataProc(AudioConverterRef aAudioConverter,
                         UInt32* aNumDataPackets /* in/out */,
                         AudioBufferList* aData /* in/out */,
                         AudioStreamPacketDescription** aPacketDesc,
                         void* aUserData)
{
    CSpxAppleCodecAdapter* obj = (CSpxAppleCodecAdapter*)aUserData;

    auto readSamples = obj->m_readCallback(obj->m_inputBuffer.get(), static_cast<uint32_t>(obj->m_inputBufferSize));
    SPX_TRACE_VERBOSE("CSpxAppleCodecAdapter: read %d samples from source.", readSamples);

    if (readSamples <= 0)
    {
        *aNumDataPackets = 0;
        return noErr;
    }

    *aNumDataPackets = 1;

    if (aPacketDesc) {
        obj->m_packetDesc.mDataByteSize = readSamples;
        obj->m_packetDesc.mStartOffset = 0;
        obj->m_packetDesc.mVariableFramesInPacket = 0;
        *aPacketDesc = &(obj->m_packetDesc);
    }

    aData->mBuffers[0].mNumberChannels = 1;
    aData->mBuffers[0].mDataByteSize = readSamples;
    aData->mBuffers[0].mData = obj->m_inputBuffer.get();

    return noErr;
}

void CSpxAppleCodecAdapter::SetSourceFormat(SPXWAVEFORMATEX* format)
{
    SPX_THROW_HR_IF(SPXERR_ALREADY_INITIALIZED, m_sourceFormat != nullptr);

    // Allocate the buffer for the format
    auto formatSize = sizeof(SPXWAVEFORMATEX) + format->cbSize;
    m_sourceFormat = SpxAllocWAVEFORMATEX(formatSize);

    // Copy the format
    memcpy(m_sourceFormat.get(), format, formatSize);
}

uint16_t CSpxAppleCodecAdapter::GetFormat(SPXWAVEFORMATEX* format, uint16_t formatSize)
{
    UNUSED(formatSize);
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

uint32_t CSpxAppleCodecAdapter::Read(uint8_t*, uint32_t bytesToRead)
{
    if (!m_errorMessage.empty())
    {
        throw std::runtime_error(m_errorMessage);
    }
    return bytesToRead;
}

void CSpxAppleCodecAdapter::Close()
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    if (m_closeCallback != nullptr)
    {
        m_closeCallback();
    }
}

void CSpxAppleCodecAdapter::SetCallbacks(ReadCallbackFunction_Type readCallback, ISpxAudioStreamReaderInitCallbacks::CloseCallbackFunction_Type closeCallback)
{
    m_readCallback = readCallback;
    m_closeCallback = closeCallback;
}

void CSpxAppleCodecAdapter::SetPropertyCallback2(GetPropertyCallbackFunction_Type2 getPropertyCallBack)
{
    m_getPropertyCallback = getPropertyCallBack;
}

void CSpxAppleCodecAdapter::SetWriterCallbacks(WriteCallbackFunction_Type writeCallback, ISpxAudioStreamWriterInitCallbacks::CloseCallbackFunction_Type closeCallback)
{
    m_writeCallback = writeCallback;
    m_writerCloseCallback = closeCallback;
}

SPXSTRING CSpxAppleCodecAdapter::GetProperty(PropertyId propertyId)
{
    return m_getPropertyCallback(propertyId);
}

} } } } // Microsoft::CognitiveServices::Speech::Impl
