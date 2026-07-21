//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once
#include "spxcore_common.h"
#include "ispxinterfaces.h"
#include "interface_helpers.h"
#include "speechapi_cxx_audio_stream_format.h"
#include <AudioToolbox/AudioConverter.h>

using namespace Microsoft::CognitiveServices::Speech::Audio;

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

class CSpxAppleCodecAdapter :
    public ISpxGenericSite,
    public ISpxAudioStreamInitFormat,
    public ISpxAudioStream,
    public ISpxAudioStreamReader,
    public ISpxAudioStreamReaderInitCallbacks,
    public ISpxAudioStreamWriterInitCallbacks,
    public ISpxAudioCodecAdapter
{
public:

    CSpxAppleCodecAdapter();
    virtual ~CSpxAppleCodecAdapter();

    SPX_INTERFACE_MAP_BEGIN()
        SPX_INTERFACE_MAP_ENTRY(ISpxAudioStreamInitFormat)
        SPX_INTERFACE_MAP_ENTRY(ISpxGenericSite)
        SPX_INTERFACE_MAP_ENTRY(ISpxAudioStream)
        SPX_INTERFACE_MAP_ENTRY(ISpxAudioStreamReader)
        SPX_INTERFACE_MAP_ENTRY(ISpxAudioStreamReaderInitCallbacks)
        SPX_INTERFACE_MAP_ENTRY(ISpxAudioStreamWriterInitCallbacks)
        SPX_INTERFACE_MAP_ENTRY(ISpxAudioCodecAdapter)
    SPX_INTERFACE_MAP_END()

    // --- ISpxAudioStreamReader ---
    uint16_t GetFormat(SPXWAVEFORMATEX* format, uint16_t formatSize) override;
    uint32_t Read(uint8_t* pbuffer, uint32_t cbBuffer) override;
    SPXSTRING GetProperty(PropertyId propertyId) override;
    void Close() override;
    // --- ISpxAudioStreamInitFormat ---
    void SetFormat(const SPXWAVEFORMATEX* format) override;
    // --- ISpxAudioStreamReaderInitCallbacks ---
    void SetCallbacks(ISpxAudioStreamReaderInitCallbacks::ReadCallbackFunction_Type readCallback, ISpxAudioStreamReaderInitCallbacks::CloseCallbackFunction_Type closeCallback) override;
    void SetPropertyCallback2(GetPropertyCallbackFunction_Type2 getPropertyCallBack) override;
    // --- ISpxAudioStreamWriterInitCallbacks ---
    void SetWriterCallbacks(ISpxAudioStreamWriterInitCallbacks::WriteCallbackFunction_Type writeCallback, ISpxAudioStreamWriterInitCallbacks::CloseCallbackFunction_Type closeCallback) override;
    // --- ISpxAudioCodecAdapter ---
    void EnableThrottling(bool) override {}; // Not supported on macOS
    void SetSourceFormat(SPXWAVEFORMATEX* format) override;

private:

    DISABLE_COPY_AND_MOVE(CSpxAppleCodecAdapter);

    /// <summary>
    /// AudioConverterComplexInputDataProc
    /// https://developer.apple.com/documentation/audiotoolbox/audioconvertercomplexinputdataproc?language=objc
    /// A callback function that supplies audio data to convert.
    /// This callback is invoked repeatedly as the converter is ready for new input data.
    /// </summary>
    static OSStatus InputDataProc(AudioConverterRef aAudioConverter,
                                  UInt32 *aNumDataPackets /* in/out */,
                                  AudioBufferList *aData /* in/out */,
                                  AudioStreamPacketDescription **aPacketDesc,
                                  void *aUserData);

    std::shared_ptr<SPXWAVEFORMATEX> m_format;
    std::shared_ptr<SPXWAVEFORMATEX> m_sourceFormat;

    ReadCallbackFunction_Type m_readCallback;
    ISpxAudioStreamReaderInitCallbacks::CloseCallbackFunction_Type m_closeCallback;
    WriteCallbackFunction_Type m_writeCallback;
    ISpxAudioStreamWriterInitCallbacks::CloseCallbackFunction_Type m_writerCloseCallback;
    GetPropertyCallbackFunction_Type2 m_getPropertyCallback;

    AudioStreamPacketDescription m_packetDesc{};
    SpxSharedAudioBuffer_Type m_inputBuffer;
    size_t m_inputBufferSize;

    std::string m_errorMessage;
};

} } } } // Microsoft::CognitiveServices::Speech::Impl
