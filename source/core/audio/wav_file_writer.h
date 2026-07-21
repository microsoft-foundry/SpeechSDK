//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// wav_file_write.h: Implementation declarations for CSpxWavFileWriter C++ class
//

#pragma once
#include "null_audio_output.h"
#include "spxcore_common.h"
#include "interface_helpers.h"


namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {


class CSpxWavFileWriter :
    public ISpxAudioFile,
    public CSpxNullAudioOutput
{
public:

    // --- ctor/dtor

    CSpxWavFileWriter();
    virtual ~CSpxWavFileWriter();

    SPX_INTERFACE_MAP_BEGIN()
        SPX_INTERFACE_MAP_ENTRY(ISpxAudioFile)
        SPX_INTERFACE_MAP_ENTRY(ISpxAudioOutput)
        SPX_INTERFACE_MAP_ENTRY(ISpxAudioStream)
        SPX_INTERFACE_MAP_ENTRY(ISpxAudioStreamInitFormat)
        SPX_INTERFACE_MAP_ENTRY(ISpxAudioOutputFormat)
        SPX_INTERFACE_MAP_ENTRY(ISpxAudioOutputInitFormat)
    SPX_INTERFACE_MAP_END()

    // --- ISpxAudioFile ---

    void Open(const char * fileName) override;
    void Close() override;

    bool IsOpen() const override;

    void SetContinuousLoop(bool value) override;
    void SetIterativeLoop(bool value) override;

    // --- ISpxAudioOutput ---

    uint32_t Write(const uint8_t* buffer, uint32_t size) override;
    void WaitUntilDone() override;
    void ClearUnread() override {}

protected:

    DISABLE_COPY_AND_MOVE(CSpxWavFileWriter);

private:

    using WavFile_Type = std::fstream;

    void EnsureRiffHeader();

    void WriteRiffHeader(uint32_t cData, uint32_t cEventData) const;

    void UpdateWaveBodySize(uint32_t size) const;

    std::string m_fileName;
    std::unique_ptr<WavFile_Type> m_file;

    bool m_bHeaderIsWritten = false;

    uint32_t m_nWrittenBytes = 0;
};


} } } } // Microsoft::CognitiveServices::Speech::Impl
