//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// audio_processing_options.cpp: Implementation definitions for CSpxAudioProcessingOptions C++ class
//

#include "stdafx.h"
#include "audio_processing_options.h"
#include "property_id_2_name_map.h"
#include <ajv.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#elif defined(__linux__)
#include <dlfcn.h>
#endif

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

CSpxAudioProcessingOptions::CSpxAudioProcessingOptions()
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    m_audioProcessingFlags = AUDIO_INPUT_PROCESSING_NONE;
    m_microphoneArrayGeometry = PresetMicrophoneArrayGeometry::Uninitialized;
    m_microphoneArrayType = MicrophoneArrayType::Planar;
    m_beamformingStartAngle = 0;
    m_beamformingEndAngle = 360;
    m_speakerReferenceChannel = SpeakerReferenceChannel::None;
}

CSpxAudioProcessingOptions::~CSpxAudioProcessingOptions()
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
}

void CSpxAudioProcessingOptions::InitWithProcessingFlags(int audioProcessingFlags)
{
    m_audioProcessingFlags = audioProcessingFlags;
    InitModelPaths();
}

void CSpxAudioProcessingOptions::InitWithPresetMicrophoneArrayGeometry(int audioProcessingFlags, PresetMicrophoneArrayGeometry microphoneArrayGeometry, SpeakerReferenceChannel speakerReferenceChannel)
{
    const bool isPnsEnabled = (audioProcessingFlags & AUDIO_INPUT_PROCESSING_PNS_ENABLE) != 0;
    SPX_THROW_HR_IF(SPXERR_INVALID_ARG, microphoneArrayGeometry == PresetMicrophoneArrayGeometry::Uninitialized || microphoneArrayGeometry == PresetMicrophoneArrayGeometry::Custom);
    if (isPnsEnabled && microphoneArrayGeometry != PresetMicrophoneArrayGeometry::Mono)
    {
        SPX_TRACE_ERROR("Personalized Noise Suppression (PNS) only supports mono microphone input. PresetMicrophoneArrayGeometry must be set to Mono.");
        SPX_THROW_HR(SPXERR_INVALID_ARG);
    }

    m_audioProcessingFlags = audioProcessingFlags;
    m_microphoneArrayGeometry = microphoneArrayGeometry;
    if ((microphoneArrayGeometry == PresetMicrophoneArrayGeometry::Linear2) ||
        (microphoneArrayGeometry == PresetMicrophoneArrayGeometry::Linear4))
    {
        m_microphoneArrayType = MicrophoneArrayType::Linear;
        m_beamformingEndAngle = 180;
    }
    else
    {
        m_microphoneArrayType = MicrophoneArrayType::Planar;
        m_beamformingEndAngle = 360;
    }
    m_beamformingStartAngle = 0;
    m_microphoneCoordinates = ConvertPresetGeometryToCoordinates(microphoneArrayGeometry);
    m_speakerReferenceChannel = speakerReferenceChannel;
    InitModelPaths();
}

void CSpxAudioProcessingOptions::InitWithMicrophoneArrayGeometry(int audioProcessingFlags, MicrophoneArrayGeometry microphoneArrayGeometry, SpeakerReferenceChannel speakerReferenceChannel)
{
    const bool isPnsEnabled = (audioProcessingFlags & AUDIO_INPUT_PROCESSING_PNS_ENABLE) != 0;
    if (isPnsEnabled)
    {
        // PNS only supports mono input. Automatically initialize with Mono geometry.
        SPX_DBG_TRACE_WARNING("Personalized Noise Suppression (PNS) does not support custom microphone array geometry. Automatically initializing with Mono configuration.");
        m_audioProcessingFlags = audioProcessingFlags;
        m_microphoneArrayGeometry = PresetMicrophoneArrayGeometry::Mono;
        m_microphoneArrayType = MicrophoneArrayType::Planar;
        m_beamformingStartAngle = 0;
        m_beamformingEndAngle = 360;
        m_microphoneCoordinates = ConvertPresetGeometryToCoordinates(PresetMicrophoneArrayGeometry::Mono);
        m_speakerReferenceChannel = speakerReferenceChannel;
        InitModelPaths();
        return;
    }

    m_audioProcessingFlags = audioProcessingFlags;
    m_microphoneArrayGeometry = PresetMicrophoneArrayGeometry::Custom;
    m_microphoneArrayType = microphoneArrayGeometry.microphoneArrayType;
    m_beamformingStartAngle = microphoneArrayGeometry.beamformingStartAngle;
    m_beamformingEndAngle = microphoneArrayGeometry.beamformingEndAngle;
    m_microphoneCoordinates = microphoneArrayGeometry.microphoneCoordinates;
    m_speakerReferenceChannel = speakerReferenceChannel;
    InitModelPaths();
}

void CSpxAudioProcessingOptions::InitFromJson(const std::string& audioProcessingOptionsJson)
{
    auto json = ajv::json::Parse(audioProcessingOptionsJson);
    SPX_THROW_HR_IF(SPXERR_INVALID_ARG, !json.IsOk());

    m_audioProcessingFlags = json["audioProcessingFlags"].AsInt();
    const bool isPnsEnabled = (m_audioProcessingFlags & AUDIO_INPUT_PROCESSING_PNS_ENABLE) != 0;

    if (json["geometry"].IsOk())
    {
        std::string geometryStr = json["geometry"].AsString();
        m_microphoneArrayGeometry = (geometryStr == "Circular6+1") ? PresetMicrophoneArrayGeometry::Circular7 :
                                    (geometryStr == "Circular3+1") ? PresetMicrophoneArrayGeometry::Circular4 :
                                    (geometryStr == "Linear4") ? PresetMicrophoneArrayGeometry::Linear4 :
                                    (geometryStr == "Linear2") ? PresetMicrophoneArrayGeometry::Linear2 :
                                    (geometryStr == "Mono") ? PresetMicrophoneArrayGeometry::Mono :
                                    PresetMicrophoneArrayGeometry::Uninitialized;
        m_microphoneCoordinates = ConvertPresetGeometryToCoordinates(m_microphoneArrayGeometry);
    }
    else
    {
        m_microphoneArrayGeometry = PresetMicrophoneArrayGeometry::Custom;
        m_microphoneCoordinates.resize((size_t)json["numberOfMicrophones"].AsUint64());
        for (int i = 0; i < (int)m_microphoneCoordinates.size(); i++)
        {
            m_microphoneCoordinates[i] = {
                                            json["micCoord"][i]["xCoord"].AsInt(),
                                            json["micCoord"][i]["yCoord"].AsInt(),
                                            json["micCoord"][i]["zCoord"].AsInt()
                                        };
        }
    }

    // Validate PNS geometry requirement
    if (isPnsEnabled && m_microphoneArrayGeometry != PresetMicrophoneArrayGeometry::Mono)
    {
        SPX_TRACE_ERROR("Personalized Noise Suppression (PNS) only supports mono microphone input. PresetMicrophoneArrayGeometry must be set to Mono.");
        SPX_THROW_HR(SPXERR_INVALID_ARG);
    }

    m_microphoneArrayType = (json["micArrayType"].AsString() == "Linear") ? MicrophoneArrayType::Linear : MicrophoneArrayType::Planar;
    m_beamformingStartAngle = (uint16_t)json["horizontalAngleBegin"].AsUint();
    m_beamformingEndAngle = (uint16_t)json["horizontalAngleEnd"].AsUint();
    m_speakerReferenceChannel = json["hasLoopback"].AsBool() ? SpeakerReferenceChannel::LastChannel : SpeakerReferenceChannel::None;
    if (json["modelPaths"].IsOk())
    {
        if (json["modelPaths"]["echoCancellation"].IsOk())
        {
            m_modelPaths["EchoCancellationModelPath"] = json["modelPaths"]["echoCancellation"].AsString();
        }
        if (json["modelPaths"]["vad"].IsOk())
        {
            m_modelPaths["VadModelPath"] = json["modelPaths"]["vad"].AsString();
        }
        if (json["modelPaths"]["pns"].IsOk())
        {
            m_modelPaths["PnsModelPath"] = json["modelPaths"]["pns"].AsString();
        }
    }
    if (json["speakerSignature"].IsOk())
    {
        auto sigArray = json["speakerSignature"].AsArray();
        m_speakerSignature.clear();
        const int sigCount = sigArray.ValueCount();
        m_speakerSignature.reserve(static_cast<size_t>(sigCount));
        for (int i = 0; i < sigCount; ++i)
        {
            m_speakerSignature.push_back(static_cast<float>(sigArray[i].AsNumber()));
        }
    }
}

int CSpxAudioProcessingOptions::GetAudioProcessingFlags()
{
    return m_audioProcessingFlags;
}

PresetMicrophoneArrayGeometry CSpxAudioProcessingOptions::GetPresetMicrophoneArrayGeometry()
{
    return m_microphoneArrayGeometry;
}

MicrophoneArrayType CSpxAudioProcessingOptions::GetMicrophoneArrayType()
{
    return m_microphoneArrayType;
}

uint16_t CSpxAudioProcessingOptions::GetBeamformingStartAngle()
{
    return m_beamformingStartAngle;
}

uint16_t CSpxAudioProcessingOptions::GetBeamformingEndAngle()
{
    return m_beamformingEndAngle;
}

uint16_t CSpxAudioProcessingOptions::GetMicrophoneCount()
{
    return (uint16_t)m_microphoneCoordinates.size();
}

std::vector<MicrophoneCoordinates> CSpxAudioProcessingOptions::GetMicrophoneCoordinates()
{
    return m_microphoneCoordinates;
}

SpeakerReferenceChannel CSpxAudioProcessingOptions::GetSpeakerReferenceChannel()
{
    return m_speakerReferenceChannel;
}

std::string CSpxAudioProcessingOptions::GetModelPath(ModelType modelType)
{
    if (modelType == ModelType::EchoCancellation)
    {
        // Allow the user to override the default echo cancellation model path. Useful when the model file is
        // deployed to a location other than the directory containing the core library.
        const char* ecModelPathName = GetPropertyName(PropertyId::AudioProcessing_EchoCancellationModelPath);
        if (HasStringValue(ecModelPathName))
        {
            return GetStringValue(ecModelPathName);
        }
        return (m_modelPaths.find("EchoCancellationModelPath") != m_modelPaths.end()) ? m_modelPaths["EchoCancellationModelPath"] : "";
    }
    if (modelType == ModelType::Vad)
    {
        // Get the VAD model path from the property. This property is set on iOS and Android. Also, this can be set by the user
        // on Windows or Linux, if they want to override the default model path.
        if (HasStringValue("SPEECH-VadModelFilePath"))
        {
            return GetStringValue("SPEECH-VadModelFilePath");
        }
        return (m_modelPaths.find("VadModelPath") != m_modelPaths.end()) ? m_modelPaths["VadModelPath"] : "";
    }
    if (modelType == ModelType::Pns)
    {
        const char* pnsModelPathName = GetPropertyName(PropertyId::AudioProcessing_PersonalizedNoiseSuppressionModelPath);
        if (HasStringValue(pnsModelPathName))
        {
            return GetStringValue(pnsModelPathName);
        }
        return (m_modelPaths.find("PnsModelPath") != m_modelPaths.end()) ? m_modelPaths["PnsModelPath"] : "";
    }
    return "";
}

void CSpxAudioProcessingOptions::SetSpeakerSignature(const std::vector<float>& signature)
{
    SPX_THROW_HR_IF(SPXERR_INVALID_ARG, !signature.empty() && signature.size() != 128);
    m_speakerSignature = signature;
}

std::vector<float> CSpxAudioProcessingOptions::GetSpeakerSignature()
{
    return m_speakerSignature;
}

std::string CSpxAudioProcessingOptions::ToJson()
{
    auto json = ajv::json::Build();
    json["audioProcessingFlags"] = m_audioProcessingFlags;
    const bool isPnsEnabled = (m_audioProcessingFlags & AUDIO_INPUT_PROCESSING_PNS_ENABLE) != 0;

    if (!isPnsEnabled)
    {
        json["micArrayType"] = ((m_microphoneArrayGeometry == PresetMicrophoneArrayGeometry::Linear2) ||
                                (m_microphoneArrayGeometry == PresetMicrophoneArrayGeometry::Linear4)) ? "Linear" : "Planar";
        if (m_microphoneArrayGeometry != PresetMicrophoneArrayGeometry::Custom)
        {
            json["geometry"] = (m_microphoneArrayGeometry == PresetMicrophoneArrayGeometry::Circular7) ? "Circular6+1" :
                               (m_microphoneArrayGeometry == PresetMicrophoneArrayGeometry::Circular4) ? "Circular3+1" :
                               (m_microphoneArrayGeometry == PresetMicrophoneArrayGeometry::Linear4) ? "Linear4" :
                               (m_microphoneArrayGeometry == PresetMicrophoneArrayGeometry::Linear2) ? "Linear2" :
                               (m_microphoneArrayGeometry == PresetMicrophoneArrayGeometry::Mono) ? "Mono" :
                               "";
        }
        else
        {
            json["numberOfMicrophones"] = m_microphoneCoordinates.size();
            for (int i = 0; i < (int)m_microphoneCoordinates.size(); i++)
            {
                json["micCoord"][i]["xCoord"] = m_microphoneCoordinates[i].X;
                json["micCoord"][i]["yCoord"] = m_microphoneCoordinates[i].Y;
                json["micCoord"][i]["zCoord"] = m_microphoneCoordinates[i].Z;
            }
        }
        json["horizontalAngleBegin"] = m_beamformingStartAngle;
        json["horizontalAngleEnd"] = m_beamformingEndAngle;
        json["hasLoopback"] = (m_speakerReferenceChannel == SpeakerReferenceChannel::LastChannel);
    }
    else
    {
        // PNS is mono-only; include geometry for deserialization validation.
        json["geometry"] = "Mono";
        json["hasLoopback"] = (m_speakerReferenceChannel == SpeakerReferenceChannel::LastChannel);
    }
    if (!m_modelPaths.empty())
    {
        if (m_modelPaths.find("EchoCancellationModelPath") != m_modelPaths.end())
        {
            json["modelPaths"]["echoCancellation"] = m_modelPaths["EchoCancellationModelPath"];
        }
        if (m_modelPaths.find("VadModelPath") != m_modelPaths.end())
        {
            json["modelPaths"]["vad"] = m_modelPaths["VadModelPath"];
        }
        if (m_modelPaths.find("PnsModelPath") != m_modelPaths.end())
        {
            json["modelPaths"]["pns"] = m_modelPaths["PnsModelPath"];
        }
    }
    if (HasStringValue(GetPropertyName(PropertyId::AudioProcessing_EchoCancellationModelPath)))
    {
        json["modelPaths"]["echoCancellation"] = GetStringValue(GetPropertyName(PropertyId::AudioProcessing_EchoCancellationModelPath));
    }
    if (HasStringValue("SPEECH-VadModelFilePath"))
    {
        json["modelPaths"]["vad"] = GetStringValue("SPEECH-VadModelFilePath");
    }
    if (HasStringValue(GetPropertyName(PropertyId::AudioProcessing_PersonalizedNoiseSuppressionModelPath)))
    {
        json["modelPaths"]["pns"] = GetStringValue(GetPropertyName(PropertyId::AudioProcessing_PersonalizedNoiseSuppressionModelPath));
    }
    if (!m_speakerSignature.empty())
    {
        for (size_t i = 0; i < m_speakerSignature.size(); ++i)
        {
            const int sigIndex = static_cast<int>(i);
            json["speakerSignature"][sigIndex] = m_speakerSignature[i];
        }
    }

    return json.AsJson();
}

std::vector<MicrophoneCoordinates> CSpxAudioProcessingOptions::ConvertPresetGeometryToCoordinates(PresetMicrophoneArrayGeometry microphoneArrayGeometry)
{
    if (microphoneArrayGeometry == PresetMicrophoneArrayGeometry::Circular7)
    {
        return { { 0, 0, 0 }, { 40, 0, 0 }, { 20, -35, 0 }, { -20, -35, 0 }, { -40, 0, 0 }, { -20, 35, 0 }, { 20, 35, 0 } };
    }
    if (microphoneArrayGeometry == PresetMicrophoneArrayGeometry::Circular4)
    {
        return { { 0, 0, 0 }, { 40, 0, 0 }, { -20, -35, 0 }, { -20, 35, 0 } };
    }
    if (microphoneArrayGeometry == PresetMicrophoneArrayGeometry::Linear4)
    {
        return { { 0, -60, 0 }, { 0, -20, 0 }, { 0, 20, 0 }, { 0, 60, 0 } };
    }
    if (microphoneArrayGeometry == PresetMicrophoneArrayGeometry::Linear2)
    {
        return { { 0, -20, 0 }, { 0, 20, 0 } };
    }
    if (microphoneArrayGeometry == PresetMicrophoneArrayGeometry::Mono)
    {
        return { { 0, 0, 0 } };
    }
    return {};
}

#if defined(__linux__) && !defined(ANDROID) && !defined(__ANDROID__)
void DummyFunctionForDlAddr() {}
#endif

void CSpxAudioProcessingOptions::InitModelPaths()
{
#if defined(_WIN32)
    // On Windows, we look for models in the directory containing core dll.
    std::wstring modulePath(65536, '\0');
    if (::GetModuleFileNameW((HINSTANCE)&__ImageBase, &modulePath[0], (DWORD)modulePath.length()) != 0)
    {
        auto lastBackslash = modulePath.find_last_of(L'\\');
        if (lastBackslash != std::wstring::npos)
        {
            modulePath.resize(lastBackslash + 1);
        }
    }
    if ((m_audioProcessingFlags & AUDIO_INPUT_PROCESSING_ENABLE_V2) != 0)
    {
        m_modelPaths["EchoCancellationModelPath"] = Utils::ToUTF8(modulePath + L"MASmodels\\aec_v1.fpie");
    }
    if ((m_audioProcessingFlags & AUDIO_INPUT_PROCESSING_PNS_ENABLE) != 0)
    {
        m_modelPaths["PnsModelPath"] = Utils::ToUTF8(modulePath + L"MASmodels\\pns_avg4.fpie");
    }
    if ((m_audioProcessingFlags & AUDIO_INPUT_PROCESSING_ENABLE_VOICE_ACTIVITY_DETECTION) != 0)
    {
        m_modelPaths["VadModelPath"] = Utils::ToUTF8(modulePath + L"VADmodels\\VADnet-vadsnr-v15-logmel-bce_q8x16.fpie");
    }
#elif defined(__linux__) && !defined(ANDROID) && !defined(__ANDROID__)
    // On Linux, we look for models in the directory containing core so.
    std::string modulePath;
    Dl_info dl_info;
    if (dladdr((void*)DummyFunctionForDlAddr, &dl_info) != 0)
    {
        modulePath = dl_info.dli_fname;
        auto lastSlash = modulePath.find_last_of('/');
        if (lastSlash != std::string::npos)
        {
            modulePath.resize(lastSlash + 1);
        }
    }
    if ((m_audioProcessingFlags & AUDIO_INPUT_PROCESSING_ENABLE_V2) != 0)
    {
        m_modelPaths["EchoCancellationModelPath"] = modulePath + "MASmodels/aec_v1.fpie";
    }
    if ((m_audioProcessingFlags & AUDIO_INPUT_PROCESSING_PNS_ENABLE) != 0)
    {
        m_modelPaths["PnsModelPath"] = modulePath + "MASmodels/pns_avg4.fpie";
    }
    if ((m_audioProcessingFlags & AUDIO_INPUT_PROCESSING_ENABLE_VOICE_ACTIVITY_DETECTION) != 0)
    {
        m_modelPaths["VadModelPath"] = modulePath + "VADmodels/VADnet-vadsnr-v15-logmel-bce_q8x16.fpie";
    }
#endif
}

} } } } // Microsoft::CognitiveServices::Speech::Impl
