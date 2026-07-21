//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// wav_file_reader.cpp: Implementation definitions for CSpxWavFileReader C++ class
//

#include "stdafx.h"
#include <iostream>
#include <istream>
#include <fstream>
#include <cstring>
#include <thread>
#include "platform.h"
#include "file_utils.h"
#include "wav_file_reader.h"


namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {


CSpxWavFileReader::CSpxWavFileReader() :
    m_firstSeekDataChunkPos(0),
    m_dataChunkBytesLeft(0)
{
}

CSpxWavFileReader::~CSpxWavFileReader()
{
    Close(); // WavFile_Type may close via it's dtor; force close now for proper telemetry/tracing from ::Close method
}

void CSpxWavFileReader::Open(const char * fileName)
{
    m_fileName = fileName;

    SPX_TRACE_VERBOSE("Opening WAV file '%s'", fileName);

    auto file = std::make_unique<WavFile_Type>();
    PAL::OpenStream(*file.get(), fileName, true);

    SPX_THROW_HR_IF(SPXERR_WAV_READER_FILE_OPEN_FAILED, !file->good());
    SPX_THROW_HR_IF(SPXERR_UNEXPECTED_EOF, file->eof());

    m_file = std::move(file);
    // test code calls CSpxWavFileReader without setting a site for it.
    auto properties = SpxQueryService<ISpxNamedProperties>(GetSite());
    if (properties != nullptr)
    {
        m_simulateRealtimePercentage = properties->GetOr<uint8_t>("CARBON-INTERNAL-MOCK-WaveFileRealTimeAudioPercentage", 0);
    }
}

void CSpxWavFileReader::Close()
{
    SPX_TRACE_VERBOSE("Closing WAV file");

    if (m_file.get() != nullptr)
    {
        m_file->close();
        m_file.reset();
    }

    m_fileName.clear();
    m_waveformat = nullptr;
}

bool CSpxWavFileReader::IsOpen() const
{
    return m_file.get() != nullptr;
}

void CSpxWavFileReader::SetContinuousLoop(bool value)
{
    m_continuousAudioLoop = value;
}

void CSpxWavFileReader::SetIterativeLoop(bool value)
{
    m_iterativeAudioLoop = value;
}

uint16_t CSpxWavFileReader::GetFormat(SPXWAVEFORMATEX* format, uint16_t cbFormat)
{
    SPX_THROW_HR_IF(SPXERR_UNINITIALIZED, !IsOpen());

    EnsureGetFormat();
    SPX_TRACE_ERROR_IF(m_waveformat.get() == nullptr, "IsOpen() returned true; EnsureGetFormat() didn't throw; we should have a SPXWAVEFORMAT now...");
    SPX_THROW_HR_IF(SPXERR_UNSUPPORTED_FORMAT, m_waveformat.get() == nullptr);

    uint16_t cbFormatRequired = sizeof(SPXWAVEFORMATEX) + m_waveformat->cbSize;

    if (format != nullptr) // Calling with GetFormat(nullptr, ???) is valid; we don't copy bits, only return sizeof block required
    {
        size_t cb = std::min(cbFormat, cbFormatRequired);
        std::memcpy(format, m_waveformat.get(), cb);
    }

    return cbFormatRequired;
}

uint32_t CSpxWavFileReader::Read(uint8_t* pbuffer, uint32_t cbBuffer)
{
    SPX_THROW_HR_IF(SPXERR_UNINITIALIZED, !IsOpen());

    EnsureGetFormat();
    SPX_DBG_ASSERT_WITH_MESSAGE(m_waveformat.get() != nullptr, "IsOpen() returned true; EnsureGetFormat() didn't throw; we should have a SPXWAVEFORMAT now...");

    uint32_t cbRead = 0;
    while (cbBuffer > 0 && !m_file->eof())
    {
        EnsureDataChunk();
        cbRead += ReadFromDataChunk(&pbuffer, &cbBuffer);
    }

    if (cbBuffer > 0 && cbRead == 0 && m_iterativeAudioLoop)
    {
        SPX_DBG_TRACE_VERBOSE("ITERATIVE AUDIO LOOP: Auto-rewinding...");
        m_file->clear();
        m_file->seekg(m_firstSeekDataChunkPos, WavFile_Type::beg);
    }

    if (m_simulateRealtimePercentage > 0)
    {
        auto sleepDuration = std::chrono::milliseconds(
            cbRead * 1000 / m_waveformat->nAvgBytesPerSec * m_simulateRealtimePercentage / 100);
        m_realtimeSleepWrapper.sleep_for(sleepDuration);
    }

    return cbRead;
}

void CSpxWavFileReader::EnsureGetFormat()
{
    SPX_THROW_HR_IF(SPXERR_UNINITIALIZED, !IsOpen());

    if (m_waveformat.get() == nullptr)
    {
        FindFormatAndDataChunks();
    }
}

void CSpxWavFileReader::FindFormatAndDataChunks()
{
    SPX_THROW_HR_IF(SPXERR_ALREADY_INITIALIZED, m_waveformat.get() != nullptr);
    SPX_THROW_HR_IF(SPXERR_UNINITIALIZED, !IsOpen());

    uint8_t tag[cbTag];
    uint8_t chunkType[cbChunkType];
    uint8_t chunkSizeBuffer[cbChunkSize];
    uint32_t chunkSize = 0;

    // RIFF tag MUST be 'RIFF'
    SPX_THROW_HR_IF(SPXERR_UNEXPECTED_EOF, !m_file->read((char*)tag, cbTag));
    SPX_THROW_HR_IF(SPXERR_UNEXPECTED_EOF, m_file->eof());
    SPX_THROW_HR_IF(SPXERR_INVALID_HEADER, 0 != std::memcmp(tag, "RIFF", 4));

    // RIFF chunk size comes next
    SPX_THROW_HR_IF(SPXERR_UNEXPECTED_EOF, !m_file->read((char*)chunkSizeBuffer, cbChunkSize));
    SPX_THROW_HR_IF(SPXERR_UNEXPECTED_EOF, m_file->eof());

    // Format chunk MUST be 'WAVE'
    SPX_THROW_HR_IF(SPXERR_UNEXPECTED_EOF, !m_file->read((char*)chunkType, cbChunkType));
    SPX_THROW_HR_IF(SPXERR_UNEXPECTED_EOF, m_file->eof());
    SPX_THROW_HR_IF(SPXERR_INVALID_HEADER, 0 != std::memcmp(chunkType, "WAVE", 4));

    // Initialize the first data chunk seek position to zero
    m_firstSeekDataChunkPos = 0;

    // Read chunks until we've read the SPXWAVEFORMAT and found the 'data' chunk position
    while ((m_waveformat.get() == nullptr || m_firstSeekDataChunkPos == 0) && ReadChunkTypeAndSize(chunkType, &chunkSize))
    {
        if (0 == std::memcmp(chunkType, "fmt ", cbChunkType)) // Is this the format chunk?
        {
            ReadFormatChunk(chunkSize);
        }
        else if (0 == std::memcmp(chunkType, "data", cbChunkType)) // Is this a 'data' chunk?
        {
            m_firstSeekDataChunkPos = m_file->tellg();
            m_firstSeekDataChunkPos -= sizeof(chunkType) + sizeof(chunkSizeBuffer); // account for bytes read w/ReadChunkTypeAndSize()
        }
        else // We don't care about this chunk; let's ignore it and move to the next...
        {
            m_file->seekg(chunkSize, WavFile_Type::cur);
        }
    }

    // Did we find everything we needed?
    SPX_THROW_HR_IF(SPXERR_UNEXPECTED_EOF, m_waveformat.get() == nullptr || m_firstSeekDataChunkPos == 0);

    // Finally, move back to the very beginning of the 'data' chunk...
    m_file->seekg(m_firstSeekDataChunkPos, WavFile_Type::beg);
}

bool CSpxWavFileReader::ReadChunkTypeAndSize(uint8_t* pchunkType, uint32_t* pchunkSize)
{
    bool fSuccess = false;

    if (m_file->read((char*)pchunkType, cbChunkType) && !m_file->eof())
    {
        // Read the chunk type
        SPX_THROW_HR_IF(SPXERR_UNEXPECTED_EOF, m_file->gcount() < cbChunkType);
        SPX_THROW_HR_IF(SPXERR_UNEXPECTED_EOF, m_file->eof());

        // Read the chunk size
        uint8_t chunkSizeBuffer[cbChunkSize];
        SPX_THROW_HR_IF(SPXERR_UNEXPECTED_EOF, !m_file->read((char*)chunkSizeBuffer, cbChunkSize));
        SPX_THROW_HR_IF(SPXERR_UNEXPECTED_EOF, m_file->eof());

        // chunk size is little endian
        *pchunkSize = ((uint32_t)chunkSizeBuffer[3] << 24) |
                      ((uint32_t)chunkSizeBuffer[2] << 16) |
                      ((uint32_t)chunkSizeBuffer[1] <<  8) |
                       (uint32_t)chunkSizeBuffer[0];

        fSuccess = true; // we're done!
    }

    return fSuccess;
}

void CSpxWavFileReader::ReadFormatChunk(uint32_t chunkSize)
{
    SPX_THROW_HR_IF(SPXERR_INVALID_HEADER, chunkSize < sizeof(SPXWAVEFORMATEX) && chunkSize != sizeof(SPXWAVEFORMAT));

    auto cbAllocate = std::max((size_t)chunkSize, sizeof(SPXWAVEFORMATEX)); // allocate space for EX structure, no matter what
    auto waveformat = SpxAllocWAVEFORMATEX(cbAllocate);
    waveformat->cbSize = 0;

    // Read the SPXWAVEFORMAT/SPXWAVEFORMATEX
    SPX_THROW_HR_IF(SPXERR_UNEXPECTED_EOF, !m_file->read((char*)waveformat.get(), chunkSize));
    SPX_DBG_TRACE_VERBOSE_IF(m_file->eof(), "It's very uncommon, but possible, to hit EOF after reading SPXWAVEFORMAT/SPXWAVEFORMATEX");

    // Finally, store the format
    m_waveformat = waveformat;
}

void CSpxWavFileReader::EnsureDataChunk()
{
    uint8_t chunkType[cbChunkType];
    uint32_t chunkSize = 0;

    auto fileEndPos = GetFileEndPos();
    while (!m_file->eof() && m_dataChunkBytesLeft == 0)
    {
        if (ReadChunkTypeAndSize(chunkType, &chunkSize))
        {
            auto currentPos = m_file->tellg();
            auto chunkEndPos = currentPos + (std::streamoff)chunkSize;

            if (0 == std::memcmp(chunkType, "data", cbChunkType))
            {
                SPX_TRACE_INFO("AUDIO Data Chunk @%u length=%u", (uint32_t)currentPos, chunkSize);
                m_dataChunkBytesLeft = chunkSize;
                m_lastDataChunkDataEndPos = chunkEndPos;
            }
            else if (chunkEndPos <= fileEndPos)
            {
                SPX_TRACE_INFO("OTHER Data Chunk @%u length=%u; SKIPPING...", (uint32_t)currentPos, chunkSize);
                m_file->seekg(chunkSize, WavFile_Type::cur);
            }
            else if (m_lastDataChunkDataEndPos < fileEndPos)
            {
                SPX_TRACE_WARNING("OTHER Data Chunk @%u length=%u; CAN'T SKIP ... Rewind to end of last data chunk and assume all remainder of file is actually part of that DATA CHUNK...", (uint32_t)currentPos, chunkSize);
                m_file->seekg(m_lastDataChunkDataEndPos, WavFile_Type::beg);
                m_dataChunkBytesLeft = (uint32_t)(fileEndPos - m_lastDataChunkDataEndPos);
                m_lastDataChunkDataEndPos += m_dataChunkBytesLeft;
                SPX_TRACE_WARNING("INCREASED Data Chunk size by %u byte(s)", (uint32_t)m_dataChunkBytesLeft);
            }
        }
        else if (m_file->eof() && m_continuousAudioLoop)
        {
            SPX_DBG_TRACE_VERBOSE("CONTINUOUS AUDIO LOOP: Auto-rewinding...");
            m_file->clear();
            m_file->seekg(m_firstSeekDataChunkPos, WavFile_Type::beg);
        }
    }
}

uint32_t CSpxWavFileReader::ReadFromDataChunk(uint8_t** ppbuffer, uint32_t* pcbBuffer)
{
    auto bytesToRequest = std::min(*pcbBuffer, m_dataChunkBytesLeft);
    auto bytesActuallyRead = 0;

    m_file->read((char*)*ppbuffer, bytesToRequest);

    if (m_file->fail())
    {
        auto underlyingIOErrorDuringRead = m_file->bad();
        bytesActuallyRead = (uint32_t)m_file->gcount();

        SPX_TRACE_INFO(
            "AUDIO Data chunk read - NO MORE DATA!! Requested: %d, Actual: %d %s%s",
            bytesToRequest,
            bytesActuallyRead,
            underlyingIOErrorDuringRead ? "(Bad stream integrity)" : "",
            m_file->eof() ? "(End of stream)" : "");

        // Read failure is only an error condition if an underlying failure happened. If the requested number of bytes
        // just weren't available (due to end of file, etc.) but the underlying read was OK, that's not an error.
        SPX_THROW_HR_IF(SPXERR_ABORT, underlyingIOErrorDuringRead);
    }
    else
    {
        bytesActuallyRead = bytesToRequest;
    }

    *ppbuffer += bytesActuallyRead; // move the buffer forward
    *pcbBuffer -= bytesActuallyRead; // reduce the count of bytes left to read

    m_dataChunkBytesLeft -= bytesActuallyRead;

    return bytesActuallyRead;
}

std::streamoff CSpxWavFileReader::GetFileEndPos()
{
    auto currentPos = m_file->tellg();
    m_file->seekg(0, WavFile_Type::end);

    auto fileEndPos = m_file->tellg();
    m_file->seekg(currentPos, WavFile_Type::beg);

    return fileEndPos;
}


} } } } // Microsoft::CognitiveServices::Speech::Impl
