//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"
#include "language_list_utils.h"
#include <algorithm>
#include <sstream>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {
    void CSpxLanguageListUtils::AddLangToList(const std::string& lang, std::string& languageList)
    {
        if (lang.empty() || lang.find(CommaDelim) != std::string::npos)
        {
            ThrowInvalidArgumentException("Only one non-empty language name is allowed.");
        }

        // Compare whole, delimiter-separated tokens (rather than doing a substring
        // search that would incorrectly treat e.g. "fi" as already present in "fil").
        auto langVector = PAL::split(languageList, CommaDelim);
        bool alreadyPresent = std::find(langVector.begin(), langVector.end(), lang) != langVector.end();

        if (alreadyPresent)
        {
            SPX_DBG_TRACE_INFO("%s: The language to be added %s already in target languages: %s", __FUNCTION__, lang.c_str(), languageList.c_str());
        }
        else
        {
            if (languageList.empty())
            {
                languageList = lang;
            }
            else
            {
                languageList += std::string(1, CommaDelim) + lang;
            }
        }
    }

    void CSpxLanguageListUtils::RemoveLangFromList(const std::string& lang, std::string& languageList)
    {
        if (lang.empty() || lang.find(CommaDelim) != std::string::npos)
        {
            ThrowInvalidArgumentException("Only one non-empty language name is allowed.");
        }

        // Rebuild the list from whole, delimiter-separated tokens, dropping any that
        // match exactly.
        std::ostringstream oss;
        auto langVector = PAL::split(languageList, CommaDelim);
        bool firstItem = true;
        bool removedAny = false;
        for (auto item : langVector)
        {
            if (item == lang)
            {
                removedAny = true;
                continue;
            }
            if (firstItem)
            {
                oss << item;
                firstItem = false;
            }
            else
            {
                oss << CommaDelim << item;
            }
        }

        if (removedAny)
        {
            languageList = oss.str();
        }
        else
        {
            SPX_DBG_TRACE_INFO("%s: The language to be removed %s is not in target languages: %s", __FUNCTION__, lang.c_str(), languageList.c_str());
        }
    }
}}}} // Microsoft::CognitiveServices::Speech::Impl
