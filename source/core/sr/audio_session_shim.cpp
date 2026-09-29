//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#include "stdafx.h"
#include <inttypes.h>

#include "audio_session_shim.h"
#include "property_id_2_name_map.h"

using namespace Microsoft::CognitiveServices::Speech::Impl;

void CSpxAudioSessionShim::Init()
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);

    InitSiteKeepAlive();
}

void CSpxAudioSessionShim::Term()
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    EnsureStopAudioSource();
    TermAudioSource();

    TermSiteKeepAlive();
}

void CSpxAudioSessionShim::StartAudio()
{
    EnsureStartAudioSource();
}

void CSpxAudioSessionShim::StopAudio()
{
    EnsureStopAudioSource();
}

void CSpxAudioSessionShim::ProcessCommit(uint32_t token, uint64_t offsetBytes, bool hasChannel, uint32_t channelId)
{
    // Inline commit: hand the commit marker to the session's audio
    // processor, mirroring how AudioSourceDataAvailable below hands it
    // audio chunks. Both run synchronously on the pump thread, so the
    // commit arrives at the session after all audio that precedes it.
    auto site = GetSite();
    auto ptr = site->QueryInterface<ISpxAudioProcessor>();
    if (ptr == nullptr)
    {
        SPX_TRACE_ERROR("%s: no audio processor on site; dropping commit token=%" PRIu32, __FUNCTION__, token);
        return;
    }

    SPX_DBG_TRACE_VERBOSE("%s: delivering commit token=%" PRIu32 " offsetBytes=%" PRIu64, __FUNCTION__, token, offsetBytes);
    ptr->ProcessCommit(token, offsetBytes, hasChannel, channelId);
}

/// <summary>
/// CSpxAudioSessionShim::InitSiteKeepAlive - Ensures the Session's Site is kept alive for the duration of the lifetime of this Session.
/// </summary>
/// <remarks> Due to current ownership model, and our late-into-the-cycle changes for SpeechConfig objects
/// the CSpxSession is sited to the CSpxApiFactory. This ApiFactory is not held by the dev user at or above
/// the CAPI. Thus ... we must hold it alive in order for the properties to be obtainable via the standard
/// ISpxNamedProperties mechanisms... It will be released on ::Term()
/// </remarks>
void CSpxAudioSessionShim::InitSiteKeepAlive()
{
    m_siteKeepAlive = GetSite();
}

/// <summary>
/// CSpxAudioSessionShim::TermSiteKeepAlive - Ensures the Session's Site is released.
/// </summary>
void CSpxAudioSessionShim::TermSiteKeepAlive()
{
    m_siteKeepAlive.reset();
    SPX_DBG_ASSERT(m_siteKeepAlive == nullptr);
}

/// <summary>
/// ISpxAudioSourceInitDelegateImpl::InitDelegatePtr - Initialize the AudioSourceInit DelegatePtr on first use.
/// </summary>
void CSpxAudioSessionShim::InitDelegatePtr(std::shared_ptr<ISpxAudioSourceInit>& ptr)
{
    // Obtain the ISpxAudioSourceInit interface from the underlying AudioSource object
    ptr = SpxQueryInterface<ISpxAudioSourceInit>(CSpxSessionAudioSourceHelper::EnsureInitAudioSource());
}

void CSpxAudioSessionShim::InitDelegatePtr(std::shared_ptr<ISpxAudioSourceNotifyMe>& ptr)
{
    ptr = SpxQueryInterface<ISpxAudioSourceNotifyMe>(CSpxSessionAudioSourceHelper::EnsureInitAudioSource());
}

/// <summary>
/// CSpxSession::TermAudioSource - Terminate the underlying AudioSource after releasing all references obtained from it.
/// </summary>
void CSpxAudioSessionShim::TermAudioSource()
{
    // Zombie and Clear the AudioSourceInit DelegatePtr
    AudioSourceInitDelegate::Zombie(true);
    AudioSourceInitDelegate::Clear();
    SPX_DBG_ASSERT(AudioSourceInitDelegate::IsClear());

    // Terminate the underlying AudioSource
    CSpxSessionAudioSourceHelper::TermAudioSource();
    SPX_DBG_ASSERT(CSpxSessionAudioSourceHelper::IsClear());
}

void CSpxAudioSessionShim::AudioSourceDataAvailable(bool /* first */)
{
    auto site = GetSite();
    auto ptr = site->QueryInterface<ISpxAudioProcessor>();
    if (ptr != nullptr)
    {
        std::string userId{};
        std::string timestamp{};
        TryQueryInterface<ISpxBufferProperties>(m_data, [&userId, &timestamp](ISpxBufferProperties& props)
        {
            auto propValue = props.GetBufferProperty(GetPropertyName(PropertyId::DataBuffer_UserId), "");
            userId = propValue != nullptr ? propValue.get() : "";
            propValue = props.GetBufferProperty(GetPropertyName(PropertyId::DataBuffer_TimeStamp), "");
            timestamp = propValue != nullptr ? propValue.get() : "";
        });

        auto ready = static_cast<uint32_t>(m_data->GetBytesReady());
        auto buffer = SpxAllocSharedBuffer<uint8_t>(ready);
        m_data->Read(buffer.get(), ready);
        auto chunk = std::make_shared<DataChunk>(std::move(buffer), ready, std::move(timestamp), std::move(userId));
        ptr->ProcessAudio(chunk);
    }
}
