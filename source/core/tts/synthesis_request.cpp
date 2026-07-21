//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// synthesis_result.cpp: Implementation definitions for CSpxSynthesisResult C++ class
//

#include "stdafx.h"
#include "synthesis_request.h"
#include "site_helpers.h"
#include "create_object_helpers.h"
#include "property_id_2_name_map.h"
#include "synthesis_helper.h"
#include "error_info.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {


CSpxSynthesisRequest::CSpxSynthesisRequest()
    : m_inputType(SynthesisRequestInputType::None),
    m_inputFinished(false),
    m_initialized(false)
{
    SPX_DBG_TRACE_FUNCTION();
}

CSpxSynthesisRequest::~CSpxSynthesisRequest()
{
    SPX_DBG_TRACE_FUNCTION();
}

void CSpxSynthesisRequest::Init(SynthesisRequestInputType inputType, const std::string& inputContent, const std::string& requestId)
{
    SPX_DBG_TRACE_FUNCTION();
    SPX_THROW_HR_IF(AZAC_ERR_ALREADY_INITIALIZED, m_initialized);
    m_initialized = true;
    m_inputType = inputType;
    m_inputContent = inputContent;
    m_requestId = requestId;
}

void CSpxSynthesisRequest::SetVoiceName(const std::string& voiceName, SynthesisRequestVoiceType voiceType, const std::string& modelName)
{
    SPX_THROW_HR_IF(AZAC_ERR_UNINITIALIZED, !m_initialized);
    m_voiceName = voiceName;
    m_voiceType = voiceType;
    m_modelName = modelName;
}

void CSpxSynthesisRequest::SetRequestId(const std::string& requestId)
{
    SPX_THROW_HR_IF(AZAC_ERR_UNINITIALIZED, !m_initialized);
    m_requestId = requestId;
}

void CSpxSynthesisRequest::SendTextPiece(const std::string& textPiece)
{
    SPX_DBG_TRACE_FUNCTION();
    SPX_THROW_HR_IF(AZAC_ERR_UNINITIALIZED, !m_initialized);
    SPX_THROW_HR_IF(AZAC_ERR_INVALID_ARG, m_inputType != SynthesisRequestInputType::TextStream);
    SPX_THROW_HR_IF(AZAC_ERR_INVALID_ARG, textPiece.empty());
    SPX_THROW_HR_IF(AZAC_ERR_INVALID_ARG, m_inputFinished);
    std::lock_guard<std::mutex> lock(m_textPiecesMutex);
    m_textPieces.push(textPiece);
    m_textPiecesCV.notify_one();
}

void CSpxSynthesisRequest::FinishInput()
{
    SPX_DBG_TRACE_FUNCTION();
    SPX_THROW_HR_IF(AZAC_ERR_UNINITIALIZED, !m_initialized);
    SPX_THROW_HR_IF(AZAC_ERR_INVALID_ARG, m_inputFinished);
    m_inputFinished = true;
    m_textPiecesCV.notify_one();
}

SynthesisRequestInputType CSpxSynthesisRequest::GetInputType() const
{
    return m_inputType;
}

std::tuple<std::string, SynthesisRequestVoiceType, std::string> CSpxSynthesisRequest::GetVoiceName()
{
    SPX_DBG_TRACE_FUNCTION();
    SPX_THROW_HR_IF(AZAC_ERR_UNINITIALIZED, !m_initialized);
    return std::make_tuple(m_voiceName, m_voiceType, m_modelName);
}

std::string& CSpxSynthesisRequest::GetInputContent()
{
    SPX_DBG_TRACE_FUNCTION();
    SPX_THROW_HR_IF(AZAC_ERR_UNINITIALIZED, !m_initialized);
    if (m_inputType == SynthesisRequestInputType::TextStream)
    {
        SPX_TRACE_ERROR("GetInputContent() is not supported for TextStream mode.");
        SPX_THROW_HR(AZAC_ERR_ABORT);
    }
    return m_inputContent;
}

std::string& CSpxSynthesisRequest::GetRequestId()
{
    return m_requestId;
}

std::tuple<bool, std::string> CSpxSynthesisRequest::GetNextTextPiece()
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
    SPX_THROW_HR_IF(AZAC_ERR_UNINITIALIZED, !m_initialized);
    SPX_THROW_HR_IF(AZAC_ERR_INVALID_ARG, m_inputType != SynthesisRequestInputType::TextStream);

    // Wait for the next text piece to be available
    std::unique_lock<std::mutex> lock(m_textPiecesMutex);

    if (m_textPieces.empty() && !m_inputFinished)
    {
        m_textPiecesCV.wait(lock, [this] { return !m_textPieces.empty() || m_inputFinished; });
    }

    if (m_inputFinished && m_textPieces.empty())
    {
        return std::make_tuple(false, std::string());
    }

    if (m_textPieces.empty())
    {
        SPX_THROW_HR(AZAC_ERR_ABORT);
    }

    std::string textPiece = m_textPieces.front();
    m_textPieces.pop();
    return std::make_tuple(true, textPiece);
}

} } } } // Microsoft::CognitiveServices::Speech::Impl
