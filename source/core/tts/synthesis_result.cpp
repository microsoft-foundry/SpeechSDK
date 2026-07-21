//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// synthesis_result.cpp: Implementation definitions for CSpxSynthesisResult C++ class
//

#include "stdafx.h"
#include "synthesis_result.h"
#include "site_helpers.h"
#include "create_object_helpers.h"
#include "property_id_2_name_map.h"
#include "synthesis_helper.h"
#include "error_info.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {


CSpxSynthesisResult::CSpxSynthesisResult()
    : m_reason(), m_cancellationReason(), m_error{nullptr}
{
    SPX_DBG_TRACE_FUNCTION();
}

CSpxSynthesisResult::~CSpxSynthesisResult()
{
    SPX_DBG_TRACE_FUNCTION();
}

std::string CSpxSynthesisResult::GetResultId()
{
    return m_requestId;
}

std::string CSpxSynthesisResult::GetRequestId()
{
    return m_requestId;
}

ResultReason CSpxSynthesisResult::GetReason()
{
    return m_reason;
}

CancellationReason CSpxSynthesisResult::GetCancellationReason()
{
    return m_cancellationReason;
}

const std::shared_ptr<ISpxErrorInformation>& CSpxSynthesisResult::GetError()
{
    return m_error;
}

uint32_t CSpxSynthesisResult::GetAudioLength()
{
    if (m_audioData == nullptr)
    {
        return 0;
    }

    return static_cast<uint32_t>(m_audioData->size());
}

uint64_t CSpxSynthesisResult::GetAudioDuration()
{
    return m_audioDuration;
}

std::shared_ptr<std::vector<uint8_t>> CSpxSynthesisResult::GetAudioData()
{
    return m_audioData;
}

std::shared_ptr<std::vector<uint8_t>> CSpxSynthesisResult::GetRawAudioData()
{
    return m_rawAudioData;
}

std::shared_ptr<ISpxAudioDataStream> CSpxSynthesisResult::GetAudioDataStream()
{
    auto sharedStream = SpxCreateObjectWithSite<ISpxAudioDataStream>("CSpxAudioDataStreamSharedAdapter", SpxGetRootSite());
    auto streamInit = SpxQueryInterface<ISpxAudioDataStreamSharedAdapterInit>(sharedStream);
    streamInit->InitFromAudioDataStream(m_coreStream);
    return sharedStream;
}

SpxWAVEFORMATEX_Type CSpxSynthesisResult::GetFormat()
{
    SPX_THROW_HR_IF(SPXERR_UNINITIALIZED, m_audioFormat == nullptr);
    return m_audioFormat;
}

bool CSpxSynthesisResult::HasHeader()
{
    return m_hasHeader;
}

void CSpxSynthesisResult::InitSynthesisResult(const std::string& requestId,
    ResultReason reason,
    const std::shared_ptr<ISpxErrorInformation>& error)
{
    SPX_THROW_HR_IF(SPXERR_ALREADY_INITIALIZED, m_audioData != nullptr);
    SPX_THROW_HR_IF(SPXERR_ALREADY_INITIALIZED, m_audioFormat != nullptr);

    // Set request ID
    m_requestId = requestId;

    // Set reason
    m_reason = reason;

    m_error = error;
    if (m_error != nullptr)
    {
        m_cancellationReason = error->GetCancellationReason();
        Set(PropertyId::CancellationDetails_ReasonDetailedText, error->GetDetails().c_str());
    }
}

void CSpxSynthesisResult::SetAudioData(std::shared_ptr<std::vector<uint8_t>> audio, uint64_t duration)
{
    if (audio && !audio->empty())
    {
        m_rawAudioData = audio;
        if (m_hasHeader)
        {
            auto headerVector = CSpxSynthesisHelper::BuildRiffHeader(static_cast<uint32_t>(audio->size()), 0, m_audioFormat);
            m_headerLength = headerVector->size();
            if (m_audioData != nullptr)
            {
                m_audioData->resize(audio->size() + m_headerLength);
            }
            else
            {
                m_audioData = std::make_shared<std::vector<uint8_t>>(audio->size() + m_headerLength);
            }

            memcpy(m_audioData->data(), headerVector->data(), m_headerLength);
            memcpy(m_audioData->data() + m_headerLength, audio->data(), audio->size());
        }
        else
        {
            m_audioData = audio;
        }
    }

    m_audioDuration = duration;
}

void CSpxSynthesisResult::SetAudioFormat(std::shared_ptr<SPXWAVEFORMATEX> format, bool hasHeader)
{
    m_audioFormat = format;
    m_hasHeader = hasHeader;
}

void CSpxSynthesisResult::SetAudioDataStream(const std::shared_ptr<ISpxAudioDataStream>& stream)
{
    SPX_THROW_HR_IF(SPXERR_ALREADY_INITIALIZED, m_coreStream != nullptr);
    m_coreStream = stream;
}

void CSpxSynthesisResult::UpdateError(const std::shared_ptr<ISpxErrorInformation>& error)
{
    SPX_DBG_ASSERT(error != nullptr);
    if (m_reason != ResultReason::Canceled)
    {
        m_reason = ResultReason::Canceled;
        m_error = error;
        m_cancellationReason = error->GetCancellationReason();
    }
    else
    {
        m_error = ErrorInfo::FromErrorWithAppendedDetails(m_error, error->GetDetails());
    }

    Set(PropertyId::CancellationDetails_ReasonDetailedText, m_error->GetDetails().c_str());
}

void CSpxSynthesisResult::Reset()
{
    m_requestId.clear();
    m_reason = static_cast<ResultReason>(0);
    m_cancellationReason = static_cast<CancellationReason>(0);
    m_error = nullptr;
    m_audioData.reset();
    m_audioFormat = nullptr;
    m_hasHeader = true;
    m_headerLength = 0;
    m_audioStream = nullptr;
}

void CSpxSynthesisResult::SetStringValue(const char* name, const char* value)
{
    auto parent = GetParentProperties();
    if (parent != nullptr)
    {
        parent->SetStringValue(name, value);
    }
    else
    {
        ISpxPropertyBagImpl::SetStringValue(name, value);
    }
}

void CSpxSynthesisResult::SetBinaryValue(const char* name, std::shared_ptr<uint8_t> value, size_t size)
{
    auto parent = GetParentProperties();
    if (parent != nullptr)
    {
        parent->SetBinaryValue(name, value, size);
    }
    else
    {
        ISpxPropertyBagImpl::SetBinaryValue(name, value, size);
    }
}

std::shared_ptr<ISpxNamedProperties> CSpxSynthesisResult::GetParentProperties() const
{
    if (m_coreStream)
    {
        return SpxQueryInterface<ISpxNamedProperties>(m_coreStream);
    }
    else
    {
        return nullptr;
    }
}


} } } } // Microsoft::CognitiveServices::Speech::Impl
