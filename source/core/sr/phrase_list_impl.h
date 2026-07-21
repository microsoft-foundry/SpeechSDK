//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once
#include <string>
#include "ispxinterfaces.h"
#include "interface_helpers.h"


namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {


class ISpxPhraseListImpl : public ISpxPhraseList, public ISpxGrammar
{
public:

    // --- ISpxPhraseList ---
    void InitPhraseList(const wchar_t* name) override
    {
        SPX_THROW_HR_IF(SPXERR_ALREADY_INITIALIZED, !m_name.empty());
        m_name = name;
        m_weight = 1.0; // use default biasing
    }

    std::wstring GetName() override
    {
        return m_name;
    }

    void AddPhrase(std::shared_ptr<ISpxPhrase> phrase) override
    {
        m_phrases.push_back(phrase);
    }

    void AddPhrase(std::string phrase) override
    {
        m_phraseStrings.push_back(phrase);
    }

    void SetWeight(double weight) override
    {
        m_weight = weight;
    }

    double GetWeight() override
    {
        return m_weight;
    }

    void Clear() override
    {
        m_phrases.clear();
        m_phraseStrings.clear();
    }

    // --- ISpxGrammar ---
    std::list<std::string> GetListenForList() override
    {
        std::list<std::string> listenForList;
        for (auto item = m_phrases.begin(); item!= m_phrases.end(); item++)
        {
            auto phrase = *item;
            listenForList.push_back(PAL::ToString(phrase->GetPhrase()));
        }
        for (auto item = m_phraseStrings.begin(); item!= m_phraseStrings.end(); item++)
        {
            listenForList.push_back(*item);
        }
        return listenForList;
    }

private:

    std::wstring m_name;
    std::list<std::shared_ptr<ISpxPhrase>> m_phrases;
    std::list<std::string> m_phraseStrings;
    double m_weight;
};


} } } } // Microsoft::CognitiveServices::Speech::Impl
