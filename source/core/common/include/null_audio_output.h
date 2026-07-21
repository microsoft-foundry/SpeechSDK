//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// null_audio_output.h: Implementation declarations for CSpxNullAudioOutput C++ class
//

#pragma once
#include "spxcore_common.h"
#include "interface_helpers.h"
#include "service_helpers.h"
#include "property_bag_impl.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {


class CSpxNullAudioOutput :
    public ISpxAudioOutput,
    public ISpxAudioStream,
    public ISpxAudioStreamInitFormat,
    public ISpxAudioOutputFormat,
    public ISpxAudioOutputInitFormat
{
public:

    CSpxNullAudioOutput() = default;
    virtual ~CSpxNullAudioOutput() = default;

    SPX_INTERFACE_MAP_BEGIN()
        SPX_INTERFACE_MAP_ENTRY(ISpxAudioOutput)
        SPX_INTERFACE_MAP_ENTRY(ISpxAudioStream)
        SPX_INTERFACE_MAP_ENTRY(ISpxAudioStreamInitFormat)
        SPX_INTERFACE_MAP_ENTRY(ISpxAudioOutputFormat)
        SPX_INTERFACE_MAP_ENTRY(ISpxAudioOutputInitFormat)
    SPX_INTERFACE_MAP_END()

    // --- ISpxAudioOutput ---

    uint32_t Write(const uint8_t* buffer, uint32_t size) override
    {
        UNUSED(buffer);
        return size;
    }

    void WaitUntilDone() override { }
    void ClearUnread() override { }
    void Close() override { }

    // --- ISpxAudioStream ---
    uint16_t GetFormat(SPXWAVEFORMATEX* formatBuffer, uint16_t formatSize) override
    {
        uint16_t formatSizeRequired = sizeof(SPXWAVEFORMATEX) + m_format->cbSize;
        SPX_DBG_TRACE_VERBOSE("%s is called formatBuffer is %s formatSize=%d", __FUNCTION__, formatBuffer ? "not null" : "null", formatSize);

        if (formatBuffer != nullptr)
        {
            size_t size = std::min(formatSize, formatSizeRequired);
            memcpy(formatBuffer, m_format.get(), size);
        }

        return formatSizeRequired;
    }

    // --- ISpxAudioOutputFormat ---

    bool HasHeader() override
    {
        return m_hasHeader;
    }

    std::string GetFormatString() override
    {
        return m_formatString;
    }

    SpxWAVEFORMATEX_Type GetFormat() override
    {
        return m_format;
    }

    // --- ISpxAudioStreamInitFormat ---

    void SetFormat(const SPXWAVEFORMATEX* pformat) override
    {
        SPX_THROW_HR_IF(SPXERR_INVALID_ARG, pformat == nullptr);

        // Allocate the buffer for the format
        auto formatSize = sizeof(SPXWAVEFORMATEX) + pformat->cbSize;
        m_format = SpxAllocWAVEFORMATEX(formatSize);
        SPX_DBG_TRACE_VERBOSE("%s is called with format 0x%p", __FUNCTION__, (void*)pformat);

        // Copy the format
        memcpy(m_format.get(), pformat, formatSize);
    }

    // --- ISpxAudioOutputInitFormat ---

    void SetHeader(bool hasHeader) override
    {
        m_hasHeader = hasHeader;
    }

    void SetFormatString(const std::string& formatString) override
    {
        m_formatString = formatString;
    }

protected:

    SpxWAVEFORMATEX_Type m_format;
    bool m_hasHeader = false;
    std::string m_formatString;
    std::string m_rawFormatString;
};


} } } } // Microsoft::CognitiveServices::Speech::Impl
