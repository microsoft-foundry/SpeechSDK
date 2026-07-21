//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// wav_file_writer.cpp: Implementation definitions for CSpxWavFileWriter C++ class
//

#include "stdafx.h"
#include <istream>
#include <fstream>
#include <cstring>
#include "platform.h"
#include "file_utils.h"
#include "wav_file_writer.h"
#include "synthesis_helper.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {


CSpxWavFileWriter::CSpxWavFileWriter()
{}

CSpxWavFileWriter::~CSpxWavFileWriter()
{
    CSpxWavFileWriter::Close();
}

void CSpxWavFileWriter::Open(const char * fileName)
{
    m_fileName = fileName;
    SPX_TRACE_VERBOSE("Opening WAV file '%s'", fileName);

    auto file = std::make_unique<std::fstream>();
    PAL::OpenStream(*file, fileName, false);

    SPX_THROW_HR_IF(SPXERR_WAV_WRITER_FILE_OPEN_FAILED, !file->good());

    m_file = std::move(file);
}

void CSpxWavFileWriter::Close()
{
    SPX_TRACE_VERBOSE("Closing WAV file '%s'; written size (without header) %d", m_fileName.c_str(), m_nWrittenBytes);

    if (m_file != nullptr)
    {
        m_file->close();
        m_file.reset();
    }

    m_fileName.clear();
    m_format = nullptr;
}

bool CSpxWavFileWriter::IsOpen() const
{
    return m_file != nullptr;
}

void CSpxWavFileWriter::SetContinuousLoop(bool value)
{
    UNUSED(value);
    SPX_THROW_HR(SPXERR_NOT_IMPL);
}

void CSpxWavFileWriter::SetIterativeLoop(bool value)
{
    UNUSED(value);
    SPX_THROW_HR(SPXERR_NOT_IMPL);
}

uint32_t CSpxWavFileWriter::Write(const uint8_t* buffer, uint32_t size)
{
    SPX_THROW_HR_IF(SPXERR_UNINITIALIZED, !IsOpen());
    SPX_THROW_HR_IF(SPXERR_UNINITIALIZED, m_format.get() == nullptr);

    EnsureRiffHeader(); // Make sure the wave header is written to the file

    m_file->seekp(0, WavFile_Type::end); // Seek to end of file
    m_file->write(reinterpret_cast<const char *>(buffer), size); // Write the audio data

    m_nWrittenBytes += size;
    UpdateWaveBodySize(m_nWrittenBytes); // Update wave header with the body size

    return size;
}

void CSpxWavFileWriter::WaitUntilDone()
{
    m_file->flush();
}

void CSpxWavFileWriter::EnsureRiffHeader()
{
    if (m_hasHeader && !m_bHeaderIsWritten)
    {
        WriteRiffHeader(0, 0);
        m_bHeaderIsWritten = true;
    }
}

void CSpxWavFileWriter::WriteRiffHeader(uint32_t cData, uint32_t cEventData) const
{
    SPX_THROW_HR_IF(SPXERR_UNINITIALIZED, !IsOpen());

    auto headerVector = CSpxSynthesisHelper::BuildRiffHeader(cData, cEventData, m_format);

    m_file->seekp(0, WavFile_Type::beg);
    m_file->write(reinterpret_cast<const char *>(&headerVector->front()), headerVector->size());
}

void CSpxWavFileWriter::UpdateWaveBodySize(uint32_t size) const
{
    SPX_THROW_HR_IF(SPXERR_UNINITIALIZED, !IsOpen());

    if (m_hasHeader)
    {
        WriteRiffHeader(size, 0);
        m_file->seekp(0, WavFile_Type::end);
    }
}

} } } } // Microsoft::CognitiveServices::Speech::Impl
