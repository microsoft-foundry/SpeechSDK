//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"
#include "buffer_properties.h"
#include "create_object_helpers.h"
#include "property_id_2_name_map.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

/*
 CSpxAudioPump::PumpThread() in audio_pump.cpp writes 100 msec of audio data, 10 times a second, to the
 ring buffered implemented in buffer_data.cpp. Attached to every 100 msec of audio is optional metadata:
 time stamp and user ID (= speaker ID). Speaker ID is commonly set in conversation transcriber scenarios.

 Two buffers are used to store {property name, property value} metadata pairs. These buffers are named
 "Property Data" and "Property Value". The Property Data buffer holds fixed-size records, including the
 property name. The Property Value buffer holds variable-size records of the property value.

 For example, in a conversation transcriber scenario, the app may tag input audio any time by calling:
    pushAudio->SetProperty(PropertyId::DataBuffer_UserId, "SpeakerA");
 where pushAudio is a the PushAudioInputStream used. CSpxAudioPump::PumpThread() will read this UserId
 property and will apply it to every relevant 100 msec of audio written to the ring buffer buffer_data.cpp.
 This call will result in one record added to each of the two metadata buffers:

     Added to Property Data buffer (always a fixed-size record):
        uint64_t nameId -  Unique ID (integer) identifying the property name PropertyId::DataBuffer_UserId. Values start from 1 and increase.
        uint64_t offset -  The position (offset in bytes) in the audio data buffer (file buffer_data.cpp) where the most recent audio buffer was written.
        uint64_t valueId - The position (offset in bytes) in the Property Values buffer, where the value "SpeakerA" is stored.

     Added to the Property Value buffer (variable-size record):
        uint64_t valueSize - The length of the string, including the null terminating char (e.g. 9 for "SpeakerA")
        <<variable-size>> value - The string itself ("SpeakerA")

 Note that the same thread that writes audio and metadata to the above ring buffers, also immediately
 tries to read it and send it to the network (websocket) stack. See CSpxAudioProcessorWriteToAudioSourceBuffer::ProcessAudio().
 So in normal conditions the audio and metadata ring buffers should really only contain 100 msec of audio & metadata.
*/

// Keep in sync with the value defined in buffer_data.cpp
constexpr SizeType DEFAULT_AUDIO_SOURCE_BUFFER_DATA_SIZE_SECONDS = 3;

// Max Property Data buffer size needed for ONE SECOND of audio. Per the above comment, 10 times a second,
// time stamp and speaker ID values are written (if they exist). So the worse case scenario
// is 10 x 8 x 3 x 2 = 480 bytes per second. To be on the safe size we take a factor of 1.5 and get to 720
constexpr SizeType DEFAULT_AUDIO_SOURCE_BUFFER_PROPERTY_DATA_SIZE_PER_SECOND = 720;

// Max Property Value buffer size needed for ONE SECOND of audio. Use the same value as above, assuming
// property value strings are on average 16 chars long or less, such that a data record in the Property Data buffer is
// on average similar in size to a data record in the Property Value buffer.
constexpr SizeType DEFAULT_AUDIO_SOURCE_BUFFER_PROPERTY_VALUES_SIZE_PER_SECOND = DEFAULT_AUDIO_SOURCE_BUFFER_PROPERTY_DATA_SIZE_PER_SECOND;

CSpxBufferProperties::~CSpxBufferProperties()
{
    TermPropertyNames();
    TermPropertyDataBuffer();
    TermPropertyValuesBuffer();
    SPX_DBG_ASSERT(m_values == nullptr);
    SPX_DBG_ASSERT(m_data == nullptr);
    SPX_DBG_ASSERT(m_nameIds.size() == 0);
    SPX_DBG_ASSERT(m_nameFromId.size() == 0);
}

void CSpxBufferProperties::Term()
{
}

void CSpxBufferProperties::SetBufferProperty(const char* name, const char* value)
{
    auto offset = OffsetFromSite();
    auto nameId = IdFromName(name);
    auto valueId = IdFromValue(value);
    WritePropertyData(nameId, offset, valueId);
}

std::shared_ptr<const char> CSpxBufferProperties::GetBufferProperty(const char* name, const char* defaultValue)
{
    auto offset = OffsetFromSite();
    auto nameId = IdFromName(name);
    auto value = FindPropertyDataValue(nameId, offset, -1, nullptr);
    return value != nullptr ? value : std::shared_ptr<const char>(defaultValue, [](auto) {});
}

std::shared_ptr<const char> CSpxBufferProperties::GetBufferProperty(const char* name, OffsetType offset, int direction, OffsetType* foundAtOffset)
{
    auto nameId = IdFromName(name);
    return FindPropertyDataValue(nameId, offset, direction, foundAtOffset);
}

std::list<ISpxBufferProperties ::FoundPropertyData_Type> CSpxBufferProperties::GetBufferProperties(OffsetType offsetBegin, OffsetType offsetEnd)
{
    auto nameId = IdFromName(nullptr);
    return FindPropertyData(nameId, offsetBegin, offsetEnd);
}

std::list<ISpxBufferProperties::FoundPropertyData_Type> CSpxBufferProperties::GetBufferProperties(const char* name, OffsetType offsetBegin, OffsetType offsetEnd)
{
    auto nameId = IdFromName(name);
    return FindPropertyData(nameId, offsetBegin, offsetEnd);
}

void CSpxBufferProperties::TermPropertyNames()
{
    m_nameIds.clear();
    SPX_DBG_ASSERT(m_nameIds.size() == 0);

    m_nameFromId.clear();
    SPX_DBG_ASSERT(m_nameFromId.size() == 0);
}

void CSpxBufferProperties::EnsureInitPropertyDataBuffer()
{
    SPX_IFTRUE(m_data == nullptr, InitPropertyDataBuffer());
}

void CSpxBufferProperties::InitPropertyDataBuffer()
{
    auto data = SpxCreateObjectWithSite<ISpxReadWriteBufferInit>("CSpxReadWriteRingBuffer", this);
    data->SetName("BufferPropertyData");
    data->SetSize(GetPropertyDataBufferSize());
    data->AllowOverflow(ISpxReadWriteBufferInit::OverflowBehavior::AllowWithTraces);
    m_data = SpxQueryInterface<ISpxReadWriteBuffer>(data);
}

void CSpxBufferProperties::TermPropertyDataBuffer()
{
    SpxTermAndClear(m_data);
    SPX_DBG_ASSERT(m_data == nullptr);
}

void CSpxBufferProperties::EnsureInitPropertyValuesBuffer()
{
    SPX_IFTRUE(m_values == nullptr, InitPropertyValuesBuffer());
}

void CSpxBufferProperties::InitPropertyValuesBuffer()
{
    auto values = SpxCreateObjectWithSite<ISpxReadWriteBufferInit>("CSpxReadWriteRingBuffer", this);
    values->SetName("BufferPropertyValues");
    values->SetSize(GetPropertyValueBufferSize());
    values->AllowOverflow(ISpxReadWriteBufferInit::OverflowBehavior::AllowWithTraces);
    m_values = SpxQueryInterface<ISpxReadWriteBuffer>(values);
}

void CSpxBufferProperties::TermPropertyValuesBuffer()
{
    SpxTermAndClear(m_values);
    SPX_DBG_ASSERT(m_values == nullptr);
}

SizeType CSpxBufferProperties::GetPropertyDataBufferSize()
{
    auto properties = SpxGetSiteQueryService<ISpxNamedProperties>(this);
    auto size = properties->GetOr<SizeType>("BufferPropertyDataSizeInBytes", GetDefaultPropertyDataBufferSize());
    return size;
}

SizeType CSpxBufferProperties::GetDefaultPropertyDataBufferSize() const
{
    return DEFAULT_AUDIO_SOURCE_BUFFER_PROPERTY_DATA_SIZE_PER_SECOND * DEFAULT_AUDIO_SOURCE_BUFFER_DATA_SIZE_SECONDS;
}

SizeType CSpxBufferProperties::GetPropertyValueBufferSize()
{
    auto properties = SpxGetSiteQueryService<ISpxNamedProperties>(this);
    auto size = properties->GetOr<SizeType>("BufferPropertyValueSizeInBytes", GetDefaultPropertyValueBufferSize());
    return size;
}

SizeType CSpxBufferProperties::GetDefaultPropertyValueBufferSize() const
{
    return DEFAULT_AUDIO_SOURCE_BUFFER_PROPERTY_VALUES_SIZE_PER_SECOND * DEFAULT_AUDIO_SOURCE_BUFFER_DATA_SIZE_SECONDS;
}

uint64_t CSpxBufferProperties::OffsetFromSite()
{
    auto site = GetSite();
    return site != nullptr ? site->GetOffset() : 0;
}

uint64_t CSpxBufferProperties::IdFromName(const char* name)
{
    if (name == nullptr)
    {
        return 0;
    }
    else if (m_nameIds.count(name) > 0)
    {
        return m_nameIds[name];
    }
    auto id = m_nameIds.size() + 1;
    m_nameFromId[id] = name;
    m_nameIds[name] = id;
    return id;
}

uint64_t CSpxBufferProperties::IdFromValue(const char* value)
{
    EnsureInitPropertyValuesBuffer();

    SizeType valueSize = strlen(value) + 1;
    m_values->Write(&valueSize, sizeof(SizeType));

    auto id = m_values->GetWritePos();
    m_values->Write(value, valueSize);

    return id;
}

ISpxBufferProperties::PropertyName_Type CSpxBufferProperties::NameFromId(uint64_t id)
{
    if (m_nameFromId.count(id) != 1)
    {
        return nullptr;
    }
    auto& name = m_nameFromId[id];
    return std::shared_ptr<const char>(name.c_str(), [](auto){});
}

ISpxBufferProperties::PropertyValue_Type CSpxBufferProperties::ValueFromId(uint64_t id)
{
    EnsureInitPropertyValuesBuffer();

    if (id >= m_values->GetWritePos())
    {
        return nullptr;
    }

    auto valuePos = id;
    SizeType valueSize{0};
    auto valueSizePos = id - sizeof(valueSize);
    m_values->ReadAtBytePos(valueSizePos, &valueSize, sizeof(SizeType));
    m_values->ResetReadPos();

    return m_values->ReadSharedAtBytePos<char>(valuePos, valueSize);
}

void CSpxBufferProperties::WritePropertyData(uint64_t nameId, uint64_t offset, uint64_t valueId)
{
    EnsureInitPropertyDataBuffer();

    m_data->Write(&nameId, sizeof(nameId));
    m_data->Write(&offset, sizeof(offset));
    m_data->Write(&valueId, sizeof(valueId));
}

void CSpxBufferProperties::ReadPropertyData(uint64_t dataPos, uint64_t* nameId, uint64_t* offset, uint64_t* valueId)
{
    EnsureInitPropertyDataBuffer();

    uint64_t data[3];

    m_data->ReadAtBytePos(dataPos, &data, itemSize);
    m_data->ResetReadPos();

    if (nameId != nullptr) *nameId = data[0];
    if (offset != nullptr) *offset = data[1];
    if (valueId != nullptr) *valueId = data[2];
}

ISpxBufferProperties::PropertyValue_Type CSpxBufferProperties::FindPropertyDataValue(uint64_t nameId, uint64_t offset, int direction, OffsetType* foundAtOffset)
{
    const uint64_t notFound = UINT64_MAX;
    auto foundOffsetRead = notFound;
    auto foundValueIdRead = notFound;

    for (auto dataPos = FirstFindDataPos(); dataPos != StopFindDataPos(); dataPos = NextFindDataPos(dataPos))
    {
        uint64_t nameIdRead, offsetRead, valueIdRead;
        ReadPropertyData(dataPos, &nameIdRead, &offsetRead, &valueIdRead);

        if (direction == +1 && offsetRead < offset)
        {
            break; // looking right ... found an offset to the left ... we're outta here!!
        }
        else if (nameIdRead != nameId)
        {
            continue; // not a match ... keep looking ...
        }
        else if (offsetRead == offset)
        {
            foundValueIdRead = valueIdRead;
            foundOffsetRead = offsetRead;
            break; // direction independent ... found an exact offset match ... awesome!!
        }
        else if (direction == -1 && offsetRead < offset)
        {
            foundValueIdRead = valueIdRead;
            foundOffsetRead = offsetRead;
            break; // looking left ... found first offset to the left... we're outta here!
        }
        else if (direction == +1 && offsetRead > offset)
        {
            foundValueIdRead = valueIdRead;
            foundOffsetRead = offsetRead;
            continue; // looking right ... found an offset to the right ... we might find a better one, so keep looking ...
        }
    }

    auto value = ValueFromId(foundValueIdRead);
    if (value != nullptr && foundAtOffset != nullptr)
    {
        *foundAtOffset = foundOffsetRead;
    }

    return value;
}

std::list<ISpxBufferProperties::FoundPropertyData_Type> CSpxBufferProperties::FindPropertyData(uint64_t nameId, uint64_t offsetBegin, uint64_t offsetEnd)
{
    std::list<FoundPropertyData_Type> list;

    auto dataPos = FirstFindDataPos();
    while (dataPos != StopFindDataPos())
    {
        uint64_t nameIdRead, offsetRead, valueIdRead;
        ReadPropertyData(dataPos, &nameIdRead, &offsetRead, &valueIdRead);

        if (nameIdRead == nameId && offsetRead <= offsetEnd)
        {
            auto name = NameFromId(nameIdRead);
            auto value = ValueFromId(valueIdRead);
            list.push_front(std::make_tuple(offsetRead, name, value));

            bool atBegin = (offsetRead == offsetBegin);
            bool firstLeftOfBegin = (offsetRead < offsetBegin);
            if (atBegin || firstLeftOfBegin)
            {
                break;
            }
        }

        dataPos = NextFindDataPos(dataPos);
    }

    return list;
}

uint64_t CSpxBufferProperties::FirstFindDataPos()
{
    EnsureInitPropertyDataBuffer();
    auto writePos = m_data->GetWritePos();

    return writePos == 0
        ? StopFindDataPos()
        : (writePos - itemSize);
}

uint64_t CSpxBufferProperties::NextFindDataPos(uint64_t pos)
{
    if (pos == StopFindDataPos())
    {
        return StopFindDataPos();
    }

    auto size = m_data->GetSize();
    auto writePos = m_data->GetWritePos();
    auto firstReadPossible = size < writePos ? writePos - size : 0;

    return pos <= firstReadPossible ? StopFindDataPos() : (pos - itemSize);
}

} } } } // Microsoft::CognitiveServices::Speech::Impl
