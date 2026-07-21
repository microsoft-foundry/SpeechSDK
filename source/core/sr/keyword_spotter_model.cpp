//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#include "stdafx.h"
#include "keyword_spotter_model.h"
#include "error_info.h"
#include "file_utils.h"
#include "string_utils.h"


namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {


void CSpxKwsModel::InitFromFile(const wchar_t* fileName)
{
    if (fileName) {
        SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);

        SPX_THROW_HR_IF(SPXERR_INVALID_ARG, fileName == nullptr || fileName[0] == '\0');
        SPX_THROW_HR_IF(SPXERR_ALREADY_INITIALIZED, !m_fileName.empty());

        m_fileName = fileName;

        FILE* file = nullptr;
        auto errNum = PAL::fopen_s(&file, PAL::ToString(std::wstring(fileName)).c_str(), "rb");
        if (file != nullptr) fclose(file);

        if (file == nullptr)
        {
            if (errNum > 0)
            {
                auto errMsg = GetSystemErrorMsg(errNum);
                ThrowRuntimeError("Failed to open keyword model file '" + PAL::ToString(fileName) + "' [errno " + std::to_string(errNum) + ": " + errMsg + "]");
            }
            else
            {
                SPX_TRACE_ERROR("Failed to open keyword model file '%ls'", fileName);
                SPX_THROW_HR(SPXERR_KEYWORD_MODEL_FILE_OPEN_FAILED);
            }
        }
    }
}


} } } } // Microsoft::CognitiveServices::Speech::Impl
