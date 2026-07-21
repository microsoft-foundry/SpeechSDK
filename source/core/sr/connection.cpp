//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// connection.cpp: Implementation definitions for CSpxConnection C++ class
//

#include "stdafx.h"
#include "connection.h"
#include <service_helpers.h>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

using namespace std;

CSpxConnection::CSpxConnection()
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
}

CSpxConnection::~CSpxConnection()
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);
}

void CSpxConnection::Init(std::weak_ptr<ISpxRecognizer> recognizer, std::weak_ptr<ISpxMessageParamFromUser> setter)
{
    m_recognizer = recognizer;
    m_setMessageParamFromUser = setter;
}

std::shared_ptr<ISpxRecognizer> CSpxConnection::GetRecognizer()
{
    auto shared = m_recognizer.lock();
    if (shared == nullptr)
    {
        SPX_TRACE_WARNING("GetRecognizer from connection: recognizer is no longer valid");
    }
    return shared;
}

void CSpxConnection::Open(bool forContinuousRecognition)
{
    auto shared = GetRecognizer();
    SPX_THROW_HR_IF(SPXERR_INVALID_RECOGNIZER, shared == nullptr);
    shared->OpenConnection(forContinuousRecognition);
}

void CSpxConnection::Close()
{
    auto shared = GetRecognizer();
    if (shared)
    {
        shared->CloseConnection();
    }
}

void CSpxConnection::SetParameter(const char *path, const char* name, const char* value)
{
    auto shared = m_setMessageParamFromUser.lock();
    if (shared == nullptr)
    {
        ThrowRuntimeError("Could not get ISpxMessageParamFromUser.");
    }
    SPX_THROW_HR_IF(SPXERR_INVALID_RECOGNIZER, shared == nullptr);
    shared->SetParameter(path, name, value);
}

CSpxAsyncOp<bool> CSpxConnection::SendNetworkMessage(const char *path, std::string&& payload)
{
    auto shared = m_setMessageParamFromUser.lock();
    if (shared == nullptr)
    {
        ThrowRuntimeError("Could not get ISpxMessageParamFromUser.");
    }
    return shared->SendNetworkMessage(path, std::move(payload));
}

CSpxAsyncOp<bool> CSpxConnection::SendNetworkMessage(const char *path, std::vector<uint8_t>&& payload)
{
    auto shared = m_setMessageParamFromUser.lock();
    if (shared == nullptr)
    {
        ThrowRuntimeError("Could not get ISpxMessageParamFromUser.");
    }
    return shared->SendNetworkMessage(path, std::move(payload));
}

std::shared_ptr<ISpxNamedProperties> CSpxConnection::GetParentProperties() const
{
    return SpxQueryService<ISpxNamedProperties>(GetSite());
}

} } } } // Microsoft::CognitiveServices::Speech::Impl
