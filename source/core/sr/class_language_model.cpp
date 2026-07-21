//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#include "stdafx.h"
#include "class_language_model.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

void CSpxClassLanguageModel::InitClassLanguageModel(const wchar_t* id)
{
    SPX_THROW_HR_IF(SPXERR_ALREADY_INITIALIZED, !m_id.empty());
    m_id = id;
}

void CSpxClassLanguageModel::AssignClass(const wchar_t *className, std::shared_ptr<ISpxGrammar> grammar)
{
    SPX_THROW_HR_IF(SPXERR_INVALID_ARG, !className);
    SPX_THROW_HR_IF(SPXERR_INVALID_ARG, !grammar);

    m_referencedGrammars.push_back(std::pair<std::wstring, std::shared_ptr<ISpxGrammar>>(className, grammar));
}

std::list<std::string> CSpxClassLanguageModel::GetListenForList()
{
    // The USP Reco Engine Apapter expects the reference grammars in the format
    // "{string:string[/string]*}" and will translate to string/string[/string]*

    std::list<std::string> retVal;

    for (auto classReference : m_referencedGrammars)
    {
        for (auto grammar : classReference.second->GetListenForList())
        {
            retVal.push_back("{" + PAL::ToString(m_id) + ":" + PAL::ToString(classReference.first) + "/" + grammar + "}");
        }
    }

    return retVal;
}

}}}} // Microsoft::CognitiveServices::Speech::Impl
