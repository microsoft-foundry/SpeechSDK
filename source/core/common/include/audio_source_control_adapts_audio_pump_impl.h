//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// audio_source_control_adapts_audio_pump_impl.h: Implementation declarations for ISpxAudioSourceControlAdaptsAudioPumpImpl
//

#pragma once

#include <mutex>
#include <chrono>

#include "spxcore_common.h"
#include "interface_helpers.h"
#include "service_helpers.h"
#include "audio_pump_delegate_helper.h"
#include "audio_processor_simple_impl.h"
#include "audio_source_simple_impl.h"
#include "audio_source_control_simple_impl.h"
#include "create_object_helpers.h"
#include "exception_helpers.h"
#include "speechapi_cxx_utils.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

template <class T>
class ISpxAudioSourceControlAdaptsAudioPumpImpl :
    public ISpxAudioSourceControlSimpleImpl,
    public ISpxAudioProcessorNotifyMe,
    public ISpxAudioSourceSimpleImpl,
    public ISpxAudioPumpSite
{
    private:

    using SourceControl_Base = ISpxAudioSourceControlSimpleImpl;

public:

    ISpxAudioSourceControlAdaptsAudioPumpImpl() = default;
    inline virtual ~ISpxAudioSourceControlAdaptsAudioPumpImpl()
    {
        SPX_DBG_ASSERT(m_audioPump.IsClear());
        SPX_DBG_ASSERT(m_processor == nullptr);
        SPX_DBG_ASSERT(m_audioSource == nullptr);
        SPX_DBG_ASSERT(m_bufferData == nullptr);
    }

    inline bool IsInitialized()
    {
        return !m_audioPump.IsClear();
    }

    inline void StartAudio(std::shared_ptr<ISpxAudioSourceNotifyMe> notify) override
    {
        using namespace std::chrono_literals;
        ThrowHRIfTrue(m_audioPump.IsClear(), SPXERR_UNINITIALIZED);
        SourceControl_Base::StartAudio(notify);
        InitNotifySource(true);
        StartPump();
        std::unique_lock<std::mutex> lk{ m_stateMutex };
        auto state = GetState();
        if ((state != ISpxAudioSource::State::Started) && (state != ISpxAudioSource::State::DataAvailable))
        {
            m_stateCV.wait_for(lk, 500ms, [&]
            {
                auto state = GetState();
                auto flag = (state == ISpxAudioSource::State::Started) || (state == ISpxAudioSource::State::DataAvailable);
                return flag;
            });
        }
    }

    inline void StopAudio() override
    {
        using namespace std::chrono_literals;
        ThrowHRIfTrue(m_audioPump.IsClear(), SPXERR_UNINITIALIZED);
        StopPump();
        std::unique_lock<std::mutex> lk{ m_stateMutex };
        auto state = GetState();
        if (state != ISpxAudioSource::State::Idle)
        {
            m_stateCV.wait_for(lk, 500ms, [&]
            {
                auto state = GetState();
                auto flag = (state == ISpxAudioSource::State::Idle);
                return flag;
            });
        }
    }

    inline void NotifyMe(const std::shared_ptr<ISpxAudioProcessor>& processor) override
    {
        auto state = GetState();
        auto guard = Utils::MakeScopeGuard([&]
        {
            m_stateCV.notify_all();
        });
        std::lock_guard<std::mutex> lk{ m_stateMutex };
        switch (state)
        {
        case ISpxAudioSource::State::Idle:
            if (processor == nullptr)
            {
                /* Already stopped, ignoring... */
                return;
            }
            else
            {
                /* Start audio was called, now to complete */
                ISpxAudioSourceSimpleImpl::SetState(ISpxAudioSource::State::Started);
                NotifyAudioSourceRequestor(m_audioSource, m_bufferData);
            }
            break;
        case ISpxAudioSource::State::Started:
        case ISpxAudioSource::State::DataAvailable:
            if (processor != nullptr)
            {
                ISpxAudioSourceSimpleImpl::SetState(ISpxAudioSource::State::DataAvailable);
                NotifyAudioSourceRequestor(m_audioSource, m_bufferData);
            }
            else
            {
                ISpxAudioSourceSimpleImpl::SetState(ISpxAudioSource::State::Idle);
                NotifyAudioSourceRequestor(m_audioSource, m_bufferData);
                ISpxAudioSourceControlSimpleImpl::StopAudio();
                InitNotifySource(false);
            }
            break;
        default:
            break;
        }
    }

    inline SpxWaveFormatEx GetFormat() const override
    {
        ThrowHRIfTrue(m_audioPump.IsClear(), SPXERR_UNINITIALIZED);
        auto formatSize = m_audioPump.DelegateGetFormat(nullptr, 0);
        SpxWaveFormatEx format{ formatSize };
        m_audioPump.DelegateGetFormat(format.get(), formatSize);
        return format;
    }

protected:

    SPX_INTERFACE_MAP_BEGIN_WITH_CUSTOM_FUNCTION(QueryInterfacePumpImpl)
        SPX_INTERFACE_MAP_ENTRY(ISpxAudioSourceControl)
        SPX_INTERFACE_MAP_ENTRY(ISpxAudioProcessorNotifyMe)
        SPX_INTERFACE_MAP_ENTRY(ISpxAudioPumpSite)
    SPX_INTERFACE_MAP_END()

    SPX_SERVICE_MAP_BEGIN_WITH_CUSTOM_FUNCTION(QueryServicePumpImpl)
        SPX_SERVICE_MAP_ENTRY(ISpxAudioProcessorNotifyMe)
        SPX_SERVICE_MAP_ENTRY_OBJECT(ISpxBufferDataWriter, GetAudioSourceBuffer())
    SPX_SERVICE_MAP_END()

    inline void InitMicrophonePump(const char* pumpClassName)
    {
        auto pump = SpxCreateObjectWithSite<ISpxAudioPump>(pumpClassName, this);
        SPX_THROW_HR_IF(SPXERR_MAS_LIBRARY_NOT_FOUND, (PAL::stricmp("CSpxMasAudioPump", pumpClassName) == 0) && (pump == nullptr));
        SPX_THROW_HR_IF(SPXERR_RUNTIME_ERROR, pump == nullptr);
        m_audioPump.SetDelegate(pump);
        InvokeOnService<ISpxNamedProperties>(this, [&](ISpxNamedProperties& properties)
        {
            constexpr auto micNiceNamePropName = "SPEECH-MicrophoneNiceName";
            auto niceName = pump->GetPropertyValue(micNiceNamePropName);
            properties.SetStringValue(micNiceNamePropName, niceName.c_str());
        });
    }

    inline void InitAudioFilePump(const char* pumpClassName, const char * filename)
    {
        auto audioFile = SpxCreateObjectWithSite<ISpxAudioFile>(pumpClassName, this);
        SPX_THROW_HR_IF(SPXERR_RUNTIME_ERROR, audioFile == nullptr);
        audioFile->Open(filename);
        auto pump = audioFile->template QueryInterface<ISpxAudioPump>();
        SPX_THROW_HR_IF(SPXERR_RUNTIME_ERROR, pump == nullptr);
        m_audioPump.SetDelegate(pump);
    }

    inline void InitAudioStreamPump(const char* pumpClassName, std::shared_ptr<ISpxAudioStream> stream)
    {
        auto pump = SpxCreateObjectWithSite<ISpxAudioPump>(pumpClassName, this);
        SPX_THROW_HR_IF(SPXERR_RUNTIME_ERROR, pump == nullptr);
        auto reader = stream->template  QueryInterface<ISpxAudioStreamReader>();
        SPX_THROW_HR_IF(SPXERR_RUNTIME_ERROR, reader == nullptr);
        auto pumpInit = pump->template  QueryInterface<ISpxAudioPumpInit>();
        SPX_THROW_HR_IF(SPXERR_RUNTIME_ERROR, pumpInit == nullptr);
        pumpInit->SetReader(reader);
        m_audioPump.SetDelegate(pump);
    }

    inline virtual void TermPump()
    {
        TermPumpInternal();
        TermAudioProcessor();
    }

    inline virtual void StartPump()
    {
        auto processor = InitAudioProcessor();

        SPX_DBG_ASSERT(m_audioPump.IsReady());
        m_audioPump.DelegateStartPump(processor);
    }

    inline virtual void StopPump()
    {
        m_audioPump.DelegateStopPump();
    }

    inline void Error(const std::string& error) final
    {
        auto shim = SpxQueryService<ISpxAudioSessionShim>(ISpxAudioSourceControl::shared_from_this());
        if (!shim)
        {
            return;
        }
        auto pumpSite = SpxQueryService<ISpxAudioPumpSite>(shim);
        if (pumpSite)
        {
            pumpSite->Error(error);
        }
    }

private:

    DISABLE_COPY_AND_MOVE(ISpxAudioSourceControlAdaptsAudioPumpImpl);

    inline std::shared_ptr<ISpxAudioProcessor> InitAudioProcessor()
    {
        if(m_processor != nullptr)
        {
            return m_processor;
        }
        return m_processor = SpxCreateObjectWithSite<ISpxAudioProcessor>("CSpxAudioProcessorWriteToAudioSourceBuffer", this);
    }

    inline void InitNotifySource(bool init)
    {
        m_audioSource = init ? SpxQueryInterface<ISpxAudioSource>(SpxSharedPtrFromThis<ISpxAudioSourceControl>(this)) : nullptr;
        m_bufferData = init ? SpxQueryService<ISpxBufferData>(m_audioSource, Type<ISpxBufferData>::Id) : nullptr;

        SPX_IFFALSE(init, SPX_DBG_ASSERT(m_audioSource == nullptr));
        SPX_IFFALSE(init, SPX_DBG_ASSERT(m_bufferData == nullptr));
    }

    inline void TermPumpInternal()
    {
        if (m_audioPump.IsClear())
        {
            return;
        }

        auto state = m_audioPump.DelegateGetState();
        UNUSED(state); /* Asserts go away when compiling release */
        SPX_DBG_ASSERT(state != ISpxAudioPump::State::Processing);
        SPX_DBG_ASSERT(state != ISpxAudioPump::State::Paused);

        SpxTermAndClearDelegate(m_audioPump);
        SPX_DBG_ASSERT(m_audioPump.IsClear());
    }

    inline void TermAudioProcessor()
    {
        SpxTermAndClear(m_processor);
        SPX_DBG_ASSERT(m_processor == nullptr);
    }

    inline std::shared_ptr<ISpxBufferData> GetAudioSourceBuffer()
    {
        return m_bufferData;
    }

    CSpxAudioPumpDelegateHelper<> m_audioPump;
    std::shared_ptr<ISpxAudioProcessor> m_processor;

    std::shared_ptr<ISpxAudioSource> m_audioSource;
    std::shared_ptr<ISpxBufferData> m_bufferData;
    std::mutex m_stateMutex;
    std::condition_variable m_stateCV;
};

} } } } // Microsoft::CognitiveServices::Speech::Impl
