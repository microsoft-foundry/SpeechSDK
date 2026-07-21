//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"
#include "synthesizer_connection.h"
#include "service_helpers.h"
#include "shared_ptr_helpers.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

CSpxSynthesizerConnection::CSpxSynthesizerConnection()
    : m_synthesizer()
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
}

CSpxSynthesizerConnection::~CSpxSynthesizerConnection()
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
}

void CSpxSynthesizerConnection::Init(std::weak_ptr<ISpxSynthesizer> synthesizer)
{
    m_synthesizer = synthesizer;
    auto validSynthesizer = m_synthesizer.lock();
    if (validSynthesizer)
    {
        m_setMessageParamFromUser = validSynthesizer->GetMessageParamFromUser();
    }
}

void CSpxSynthesizerConnection::Open(bool)
{
    auto synthesizer = m_synthesizer.lock();
    if (synthesizer)
    {
        synthesizer->OpenConnection();
    }
}

void CSpxSynthesizerConnection::Close()
{
    auto synthesizer = m_synthesizer.lock();
    if (synthesizer)
    {
        synthesizer->CloseConnection();
    }
}

void CSpxSynthesizerConnection::SetParameter(const char *path, const char* name, const char* value)
{
    auto shared = m_setMessageParamFromUser.lock();
    if (shared == nullptr)
    {
        ThrowRuntimeError("Could not set message property. Please ensure the corresponding speech synthesizer is still valid.");
    }
    shared->SetParameter(path, name, value);
}

CSpxAsyncOp<bool> CSpxSynthesizerConnection::SendNetworkMessage(const char *path, std::string&& payload)
{
    auto shared = m_setMessageParamFromUser.lock();
    if (shared == nullptr)
    {
        ThrowRuntimeError("Could not send network message. Please ensure the corresponding speech synthesizer is still valid.");
    }
    return shared->SendNetworkMessage(path, std::move(payload));
}

CSpxAsyncOp<bool> CSpxSynthesizerConnection::SendNetworkMessage(const char *path, std::vector<uint8_t>&& payload)
{
    auto shared = m_setMessageParamFromUser.lock();
    if (shared == nullptr)
    {
        ThrowRuntimeError("Could not send network message. Please ensure the corresponding speech synthesizer is still valid.");
    }
    return shared->SendNetworkMessage(path, std::move(payload));
}

std::shared_ptr<ISpxRecognizer> CSpxSynthesizerConnection::GetRecognizer()
{
    SPX_TRACE_WARNING("cannot get recognizer form connection of synthesizer.");
    return nullptr;
}

std::shared_ptr<ISpxSynthesizer> CSpxSynthesizerConnection::GetSynthesizer()
{
    auto shared = m_synthesizer.lock();
    SPX_TRACE_WARNING_IF(shared == nullptr, "%s from connection: synthesizer is no longer valid", __FUNCTION__);

    return shared;
}

std::shared_ptr<ISpxTtsEngineAdapter> CSpxSynthesizerConnection::GetTtsEngineAdapter()
{
    auto synthesizer = GetSynthesizer();
    SPX_THROW_HR_IF(SPXERR_RUNTIME_ERROR, synthesizer == nullptr);
    auto adapterSite = synthesizer->QueryInterface<ISpxTtsEngineAdapterSite>();
    auto adapter = adapterSite->GetTtsEngineAdapter();
    SPX_THROW_HR_IF(SPXERR_RUNTIME_ERROR, adapter == nullptr);
    return adapter;
}

}}}} // Microsoft::CognitiveServices::Speech::Impl
