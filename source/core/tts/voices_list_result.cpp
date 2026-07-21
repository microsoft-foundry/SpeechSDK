//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// voices_list_result.cpp: Implementation definitions for CSpxSynthesisVoicesResult C++ class
//

#include "stdafx.h"
#include "voices_list_result.h"
#include "site_helpers.h"
#include "create_object_helpers.h"
#include "property_id_2_name_map.h"
#include "guid_utils.h"
#include "synthesis_helper.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

using namespace std;

CSpxSynthesisVoicesResult::CSpxSynthesisVoicesResult()
    : m_reason(), m_error{ nullptr }
{
    SPX_DBG_TRACE_FUNCTION();
    m_voices = make_shared<vector<std::shared_ptr<ISpxVoiceInfo>>>();
}

CSpxSynthesisVoicesResult::~CSpxSynthesisVoicesResult()
{
    SPX_DBG_TRACE_FUNCTION();
}

std::shared_ptr<std::vector<std::shared_ptr<ISpxVoiceInfo>>> CSpxSynthesisVoicesResult::GetVoices()
{
    return m_voices;
}

std::string CSpxSynthesisVoicesResult::GetResultId()
{
    return m_resultId;
}

ResultReason CSpxSynthesisVoicesResult::GetReason()
{
    return m_reason;
}

const std::shared_ptr<ISpxErrorInformation>& CSpxSynthesisVoicesResult::GetError()
{
    return m_error;
}

void CSpxSynthesisVoicesResult::InitSuccessResult(const std::string& resultId)
{
    SPX_THROW_HR_IF(SPXERR_ALREADY_INITIALIZED, !m_resultId.empty());
    m_resultId = resultId;
    m_reason = ResultReason::VoicesListRetrieved;
}

void CSpxSynthesisVoicesResult::InitErrorResult(const std::shared_ptr<ISpxErrorInformation>& error, const std::string& resultId)
{
    SPX_THROW_HR_IF(SPXERR_ALREADY_INITIALIZED, !m_resultId.empty());
    m_error = error;
    if (m_error != nullptr)
    {
        Set(PropertyId::CancellationDetails_ReasonDetailedText, error->GetDetails().c_str());
    }

    m_resultId = resultId;
    m_reason = ResultReason::Canceled;
}

void CSpxSynthesisVoicesResult::AppendVoice(const std::shared_ptr<ISpxVoiceInfo>& voice)
{
    SPX_THROW_HR_IF(SPXERR_UNINITIALIZED, m_voices == nullptr);
    m_voices->emplace_back(voice);
}

void CSpxSynthesisVoicesResult::SetStringValue(const char* name, const char* value)
{
    ISpxPropertyBagImpl::SetStringValue(name, value);
}

void CSpxSynthesisVoicesResult::SetBinaryValue(const char* name, std::shared_ptr<uint8_t> value, size_t size)
{
    ISpxPropertyBagImpl::SetBinaryValue(name, value, size);
}

} } } } // Microsoft::CognitiveServices::Speech::Impl
