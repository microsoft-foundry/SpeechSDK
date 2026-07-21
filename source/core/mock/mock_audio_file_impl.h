//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// mock_audio_file_impl.h: Implementation definitions for ISpxMockAudioFileImpl C++ class
//

#pragma once
#include "stdafx.h"
#include "ispxinterfaces.h"


namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {


SPX_INTERFACE(ISpxMockAudioFileImpl)
    , public ISpxAudioFile
{
    public:

    ISpxMockAudioFileImpl() : m_isOpen(false) { }

    // --- ISpxAudioFile ---
    void Open(const char * filename) override { UNUSED(filename); m_isOpen = true; }
    void Close() override { m_isOpen = false; }

    bool IsOpen() const override { return m_isOpen; }

    void SetContinuousLoop(bool value) override { UNUSED(value); }
    void SetIterativeLoop(bool value) override { UNUSED(value); }

protected:

    ISpxMockAudioFileImpl(ISpxMockAudioFileImpl&&) = delete;
    ISpxMockAudioFileImpl(const ISpxMockAudioFileImpl&) = delete;
    ISpxMockAudioFileImpl& operator=(ISpxMockAudioFileImpl&&) = delete;
    ISpxMockAudioFileImpl& operator=(const ISpxMockAudioFileImpl&) = delete;

    bool m_isOpen;
};


} } } } // Microsoft::CognitiveServices::Speech::Impl
