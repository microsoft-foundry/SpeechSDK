//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// usp_metrics.h: send and process telemetry messages using the USP web socket connection.
//

#include "stdafx.h"
#include "usp_metrics.h"
#include "uspcommon.h"

#if defined(_MSC_VER)
#include <windows.h>
#else
#include <sys/time.h>
#endif

#include <tuple>
#include <time.h>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace USP {

constexpr int MaxMessagesToRecord{ 50 };
constexpr size_t MaxInbandTelemetryQueueSize{ 16 };

const std::array<std::tuple<IncomingMsgType, const char*>, static_cast<size_t>(countOfMsgTypes)> message_mappings{ {
    std::make_tuple(turnStart, path::turnStart),
    std::make_tuple(turnEnd, path::turnEnd),
    std::make_tuple(speechStartDetected, path::speechStartDetected),
    std::make_tuple(speechEndDetected, path::speechEndDetected),
    std::make_tuple(speechHypothesis, path::speechHypothesis),
    std::make_tuple(speechTentativePhrase, path::speechTentativePhrase),
    std::make_tuple(speechFragment, path::speechFragment),
    std::make_tuple(speechPhrase, path::speechPhrase),
    std::make_tuple(translationHypothesis, path::translationHypothesis),
    std::make_tuple(translationPhrase, path::translationPhrase),
    std::make_tuple(translationSynthesis, path::translationSynthesis),
    std::make_tuple(translationSynthesisEnd, path::translationSynthesisEnd),
    std::make_tuple(translationResponse, path::translationResponse),
    std::make_tuple(audio, event::keys::received::Audio),
    std::make_tuple(audioMetadata, event::keys::received::AudioMetadata),
    std::make_tuple(response, event::keys::received::Response),
    std::make_tuple(audioStart, path::audioStart),
    std::make_tuple(audioEnd, path::audioEnd)
} };



const char* get_message_name(const IncomingMsgType type)
{
    /* No point in using a map to such a small collection, loss of locality and heap copies probably worse than O(n) vs O(log(n)) */
    for (auto& m : message_mappings)
    {
        if (std::get<0>(m) == type)
        {
            return std::get<1>(m);
        }
    }
    return nullptr;
}

IncomingMsgType message_from_name(const std::string& name)
{
    for (const auto& m : message_mappings)
    {
        const auto& n = std::get<1>(m);
        if (name == n)
        {
            return std::get<0>(m);
        }
    }
    return countOfMsgTypes;
}

static ajv::JsonBuilder PropertybagInitializeWithKeyValue(const std::string& key, const ajv::JsonBuilder& value)
{
    ajv::JsonBuilder pb;
    if (!key.empty())
    {
        pb[key] = value;
    }
    return pb;
}

namespace MetricObjectKeys
{
    const std::string Name{ "Name" };
    const std::string Id{ "Id" };
}

static void populate_metric_object(ajv::JsonBuilder& object, const std::string& eventName, const std::string& id)
{
    if (!eventName.empty())
    {
        object[MetricObjectKeys::Name] = eventName;
    }
    if (!id.empty())
    {
        object[MetricObjectKeys::Id] = id;
    }
 }

int GetISO8601Time(char *buffer, unsigned int length)
{
    // TODO: refactor using azure-c-shared functions.
    if (length < TIME_STRING_MAX_SIZE)
    {
        return -1;
    }
    size_t timeStringLength = 0;
    time_t rawtime;
    struct tm timeinfo;

    time(&rawtime);
#ifdef _MSC_VER
    gmtime_s(&timeinfo, &rawtime);
#else
    gmtime_r(&rawtime, &timeinfo);
#endif

    timeStringLength += strftime(buffer, length, "%FT%T.", &timeinfo);

#if defined(WIN32)
    SYSTEMTIME sysTime;
    GetSystemTime(&sysTime);
    timeStringLength += snprintf(buffer + 20, 5, "%03uZ", sysTime.wMilliseconds);
#else
    struct timeval curTime;
    gettimeofday(&curTime, NULL);
    timeStringLength += snprintf(buffer + 20, 5, "%03ldZ", static_cast<long int>(curTime.tv_usec / 1000));
#endif

    return (int)timeStringLength;
}

static bool populate_event_timestamp(ajv::JsonBuilder& object, const std::string& eventName, const std::string& id, const std::string& key)
{
    if (eventName.empty() || id.empty())
    {
        return false;
    }
    populate_metric_object(object, eventName, id);
    if (object.IsNull())
    {
        return false;
    }
    char timeString[TIME_STRING_MAX_SIZE];
    if (-1 == GetISO8601Time(timeString, TIME_STRING_MAX_SIZE))
    {
        return false;
    }
    object[key] = std::string{ timeString };
    return true;
}

bool populate_event_key_value(ajv::JsonBuilder& pBag, const std::string& eventName, const std::string& id, const std::string& key, const std::string& value)
{
    if (eventName.empty())
    {
        SPX_TRACE_ERROR("Telemetry: event name is empty.");
        return false;
    }
    if (key.empty())
    {
        SPX_TRACE_ERROR("Telemetry: key name is empty.");
        return false;
    }

    populate_metric_object(pBag, eventName, id);
    if (pBag.IsEmpty())
    {
        return false;
    }

    pBag[key] = value;

    return true;
}

/* This will be inlined by the compiler */
bool push_if_not_null(ajv::JsonBuilder& array, const ajv::JsonBuilder& object)
{
    if (object.IsEmpty())
    {
        return false;
    }
    array[array.ValueCount()] = object;
    return true;
}

static ajv::JsonBuilder telemetry_add_metricevents(const TELEMETRY_DATA& telemetry_object)
{
    ajv::JsonBuilder json_array;

    if (push_if_not_null(json_array, telemetry_object.connectionJson))
    {
        // deviceJson should only be relevant when we have a connectionJson
        push_if_not_null(json_array, telemetry_object.deviceJson);

        // we can return here because when we have a connectionJson we know
        // all of the JSON besides deviceJson and connectionJson are not relevant
        return json_array;
    }

    push_if_not_null(json_array, telemetry_object.audioStartJson);
    push_if_not_null(json_array, telemetry_object.microphoneJson);
    push_if_not_null(json_array, telemetry_object.listeningTriggerJson);
    push_if_not_null(json_array, telemetry_object.ttsJson);
    if (!telemetry_object.phraseLatencyJson.IsEmpty())
    {
        auto recvObj = PropertybagInitializeWithKeyValue(event::name::PhraseLatency, telemetry_object.phraseLatencyJson);
        json_array[json_array.ValueCount()] = recvObj;
    }
    if (!telemetry_object.firstHypothesisLatencyJson.IsEmpty())
    {
        auto recvObj = PropertybagInitializeWithKeyValue(event::name::FirstHypothesisLatency, telemetry_object.firstHypothesisLatencyJson);
        json_array[json_array.ValueCount()] = recvObj;
    }
    if (!telemetry_object.hypothesisLatencyJson.IsEmpty())
    {
        auto recvObj = PropertybagInitializeWithKeyValue(event::name::HypothesisLatency, telemetry_object.hypothesisLatencyJson);
        json_array[json_array.ValueCount()] = recvObj;
    }

    return json_array;
}


static ajv::JsonBuilder* getJsonForEvent(TELEMETRY_DATA* telemetryObject, const std::string& eventName)
{
    if (eventName == event::name::AudioStart)
        return &telemetryObject->audioStartJson;
    if (eventName == event::name::Microphone)
        return &telemetryObject->microphoneJson;
    if (eventName == event::name::AudioPlayback)
        return &telemetryObject->ttsJson;

    SPX_TRACE_ERROR("Telemetry: invalid event name (%s)", eventName.c_str());
    return nullptr;
}

void CSpxTelemetry::RegisterNewRequestId(const std::string& requestId)
{
    if (requestId.empty())
    {
        SPX_TRACE_ERROR("Telemetry: empty request id");
        return;
    }

    std::lock_guard<std::mutex> lk{ m_lock };

    if (!GetTelemetryForRequestId(requestId))
    {
        auto telemetry_data = std::make_unique<TELEMETRY_DATA>();
        telemetry_data->requestId = requestId;
        m_telemetry_object_map.emplace(requestId, std::move(telemetry_data));
    }
    else
    {
        SPX_TRACE_ERROR("Telemetry: Attempting to register an already registered requestId: %s", requestId.c_str());
    }
}

void CSpxTelemetry::InbandConnectionTelemetry(const std::string& connectionId, const std::string& key, const std::string& value)
{
    std::lock_guard<std::mutex> lk{ m_lock };
    TELEMETRY_DATA* connection_data = m_current_telemetry_object.get();
    auto& connectionJson = connection_data->connectionJson;
    const std::string& eventName = event::name::Connection;
    if (event::keys::Start == key)
    {
        connection_data->bPayloadSet = populate_event_timestamp(connectionJson, eventName, connectionId, event::keys::Start);
    }
    else if (event::keys::DeviceId == key)
    {
        connection_data->bPayloadSet = populate_event_key_value(connection_data->deviceJson, event::name::Device, std::string{}, key, value);
    }
    else
    {
        connection_data->bPayloadSet &= value.empty() ?
            populate_event_timestamp(connectionJson, eventName, connectionId, key) :
            populate_event_key_value(connectionJson, eventName, connectionId, key, value);

        if (connection_data->bPayloadSet)
        {
            // Check if queue has reached maximum size to prevent memory growth
            if (m_inband_telemetry_queue.size() >= MaxInbandTelemetryQueueSize)
            {
                SPX_TRACE_WARNING("Telemetry: Inband telemetry queue is full (size: %zu), dropping oldest item.", m_inband_telemetry_queue.size());
                m_inband_telemetry_queue.pop(); // Remove oldest item
            }
            m_inband_telemetry_queue.push(std::move(m_current_telemetry_object));
        }

        // Assign new memory with zeros to current telemetry object
        m_current_telemetry_object = std::make_unique<TELEMETRY_DATA>();
    }
}

void CSpxTelemetry::InbandEventTimestampPopulate(const std::string& requestId, const std::string& eventName, const std::string& id, const std::string& key)
{
    std::lock_guard<std::mutex> lk{ m_lock };
    auto telemetry_data = GetTelemetryForRequestId(requestId);
    if (telemetry_data != nullptr)
    {
        auto* pBag = getJsonForEvent(telemetry_data, eventName);
        if (pBag != nullptr)
        {
            // Set the bPayloadSet flag
            telemetry_data->bPayloadSet |= populate_event_timestamp(*pBag, eventName, id, key);
        }
    }
    else
    {
        SPX_TRACE_WARNING("Telemetry: found no data for requestId: %s", requestId.c_str()); // may be already flushed
    }
}

static ajv::JsonBuilder telemetry_add_recvmsgs(const TELEMETRY_DATA& telemetry_object)
{
    ajv::JsonBuilder json_array;

    for (int i = 0; i < countOfMsgTypes; i++)
    {
        if (telemetry_object.receivedMsgs[i].ValueCount() != 0)
        {
            auto recvObj = PropertybagInitializeWithKeyValue(get_message_name(static_cast<IncomingMsgType>(i)), telemetry_object.receivedMsgs[i]);
            json_array[json_array.ValueCount()] = recvObj;
        }
    }
    return json_array;
}


int telemetry_serialize(ajv::JsonBuilder& root, const TELEMETRY_DATA& telemetry_object)
{
    // Root Object
    // Create and append all received events
    auto messages = telemetry_add_recvmsgs(telemetry_object);
    if (messages.ValueCount() != 0)
    {
        root[event::keys::array::ReceivedMessages] = messages;
    }

    auto metrics = telemetry_add_metricevents(telemetry_object);
    if (metrics.ValueCount() != 0)
    {    // append all metric events
        root[event::keys::array::Metrics] = metrics;
    }

    return 0;
}


CSpxTelemetry::CSpxTelemetry() : m_current_telemetry_object{ std::make_unique<TELEMETRY_DATA>() }
{
}

CSpxTelemetry::~CSpxTelemetry()
{
}

void CSpxTelemetry::Flush(const std::string& requestId, PTELEMETRY_WRITE callback)
{
    std::lock_guard<std::mutex> lk{ m_lock };

    // Check if events exist in queue. If yes, flush them out first.
    while (!m_inband_telemetry_queue.empty())
    {
        auto&& item = m_inband_telemetry_queue.front();
        // Telemetry messages need a requestId, so use the one we have if the object in queue doesn't have one
        if (item->requestId.empty())
        {
            item->requestId = requestId;
        }
        PrepareSend(*item, callback);
        m_inband_telemetry_queue.pop();
    }

    for (auto const& i : m_telemetry_object_map)
    {
        PrepareSend(*i.second, callback);
    }
    m_telemetry_object_map.clear();
}

void CSpxTelemetry::PrepareSend(const TELEMETRY_DATA& telemetryObject, PTELEMETRY_WRITE callback) const
{
    std::string requestId{};
    // Get request ID from telemetry object if any
    if (!telemetryObject.requestId.empty())
    {
        requestId = telemetryObject.requestId;
    }
    ajv::JsonBuilder root;
    if (telemetry_serialize(root, telemetryObject) == 0 && root.ValueCount() != 0)
    {
        SendSerializedTelemetry(root.AsJson(), requestId, callback);
    }
}

void CSpxTelemetry::SendSerializedTelemetry(std::string&& serialized, const std::string& requestId, PTELEMETRY_WRITE callback) const
{
    if (!serialized.empty() && callback)
    {
        SPX_TRACE_INFO("%s: Send telemetry (requestId:%s): %s", __FUNCTION__, requestId.c_str(), serialized.c_str());
        callback(std::move(serialized), requestId);
    }
}

void CSpxTelemetry::RecordReceivedMsg(const std::string& requestId, const std::string& messagePath)
{
    if (messagePath.empty())
    {
        SPX_TRACE_ERROR("Telemetry: received an empty message.");
        return;
    }

    char timeString[TIME_STRING_MAX_SIZE];
    if (-1 == GetISO8601Time(timeString, TIME_STRING_MAX_SIZE))
    {
        return;
    }

    IncomingMsgType msgType = message_from_name(messagePath);

    if (msgType == countOfMsgTypes)
    {
        SPX_TRACE_ERROR("Telemetry: received unexpected msg: (%s).", messagePath.c_str());
        return;
    }

    std::lock_guard<std::mutex> lk{ m_lock };
    auto telemetry_object = GetTelemetryForRequestId(requestId);
    if (telemetry_object != nullptr)
    {
        auto& telemetry_data = m_telemetry_object_map[requestId];
        auto& evArray = telemetry_data->receivedMsgs[static_cast<size_t>(msgType)];
        // If we reach the max number of messages, drop it.
        if (evArray.ValueCount() < MaxMessagesToRecord)
        {
            evArray[evArray.ValueCount()] = std::string(timeString);
        }
    }
    else
    {
        SPX_TRACE_ERROR("Telemetry: received unexpected requestId: (%s).", requestId.c_str());
    }
}

void CSpxTelemetry::RecordResultLatency(const std::string& requestId, uint64_t latencyInTicks, bool isPhraseLatency, bool isFirstHypothesisLatency)
{
    std::lock_guard<std::mutex> lk{ m_lock };
    auto telemetry_object = GetTelemetryForRequestId(requestId);
    if (telemetry_object != nullptr)
    {
        auto& telemetry_data = m_telemetry_object_map[requestId];
        SPX_DBG_ASSERT(telemetry_object == telemetry_data.get());

        if (isPhraseLatency)
        {
            auto& phraseLatencies = telemetry_data->phraseLatencyJson;
            SPX_IFTRUE(phraseLatencies.ValueCount() < MaxMessagesToRecord, phraseLatencies[phraseLatencies.ValueCount()] = (latencyInTicks));
        }
        else
        {
            if (isFirstHypothesisLatency)
            {
                auto& firstHypothesisLatencies = telemetry_data->firstHypothesisLatencyJson;
                SPX_IFTRUE(firstHypothesisLatencies.ValueCount() < MaxMessagesToRecord, firstHypothesisLatencies[firstHypothesisLatencies.ValueCount()] = latencyInTicks);
            }

            auto& hypothesisLatencies = telemetry_data->hypothesisLatencyJson;
            SPX_IFTRUE(hypothesisLatencies.ValueCount() < MaxMessagesToRecord, hypothesisLatencies[hypothesisLatencies.ValueCount()] = latencyInTicks);
        }
    }
    else
    {
        SPX_TRACE_ERROR("%s: Telemetry for %s: received unexpected requestId: (%s).", __FUNCTION__, isPhraseLatency ? "phrase" : "hypothesis", requestId.c_str());
    }
}

TELEMETRY_DATA* CSpxTelemetry::GetTelemetryForRequestId(const std::string& request_id) const
{
    const auto it = m_telemetry_object_map.find(request_id);
    if (it != m_telemetry_object_map.end())
    {
        return std::get<1>(*it).get();
    }
    return nullptr;
}

} } } }
