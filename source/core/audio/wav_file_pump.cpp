//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// wav_file_pump.cpp: Implementation declarations for CSpxWavFilePump C++ class
//

#include "stdafx.h"
#include "ispxinterfaces.h"
#include "create_object_helpers.h"
#include "service_helpers.h"
#include "wav_file_pump.h"


namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {


CSpxWavFilePump::CSpxWavFilePump()
{
}

void CSpxWavFilePump::Open(const char * fileName)
{
    EnsureFile(fileName);
    EnsurePump();
}

void CSpxWavFilePump::EnsureFile(const char * fileName)
{
    SPX_THROW_HR_IF(SPXERR_ALREADY_INITIALIZED, m_delegateToAudioFile != nullptr);

    // Create the reader...
    auto audioFile = SpxCreateObjectWithSite<ISpxAudioFile>("CSpxWavFileReader", GetSite());

    // Open the file...
    audioFile->Open(fileName);

    // And ... We're finished
    m_delegateToAudioFile = audioFile;
}

void CSpxWavFilePump::EnsurePump()
{
    SPX_THROW_HR_IF(SPXERR_UNINITIALIZED, m_delegateToAudioFile == nullptr);
    SPX_THROW_HR_IF(SPXERR_ALREADY_INITIALIZED, m_delegateToAudioPump != nullptr);

    // Create the pump ...
    auto pumpInit = SpxCreateObjectWithSite<ISpxAudioPumpInit>("CSpxAudioPump", GetSite());

    // Set the reader...
    auto fileAsReader = SpxQueryInterface<ISpxAudioStreamReader>(m_delegateToAudioFile);
    pumpInit->SetReader(fileAsReader);

    // And ... We're finished
    m_delegateToAudioPump = SpxQueryInterface<ISpxAudioPump>(pumpInit);
}


} } } } // Microsoft::CognitiveServices::Speech::Impl
