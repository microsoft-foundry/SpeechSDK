//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#pragma once

#include "spxcore_common.h"
#include "interface_helpers.h"
#include "service_helpers.h"
#include "ispxinterfaces.h"
#include "stdafx.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

class CSpxLanguageListUtils
{
public:
    static void AddLangToList(const std::string& lang, std::string& languageList);
    static void RemoveLangFromList(const std::string& lang, std::string& languageList);
};
} } } } // Microsoft::CognitiveServices::Speech::Impl
