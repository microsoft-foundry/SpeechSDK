//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// rest_tts_engine_adapter.h: Implementation declarations for CSpxRestTtsEngineAdapter C++ class
//

#pragma once
#include <memory>
#include <queue>
#include "asyncop.h"
#include "spxcore_common.h"
#include "ispxinterfaces.h"
#include "interface_helpers.h"
#include "property_bag_impl.h"
#include "interfaces/ispx_http_response.h"
#include <object_with_site_init_impl.h>
#include "cloud_tts_engine_adapter.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {


typedef struct RestTtsResponse_Tag
{
    std::vector<uint8_t> body;
    std::mutex mutex;
} RestTtsResponse;


typedef struct RestTtsRequest_Tag
{
    std::string requestId;
    std::string endpoint;
    std::string postContent;
    bool isSsml;
    std::string subscriptionKey;
    std::string accessToken;
    std::string outputFormatString;
    ISpxTtsEngineAdapter* adapter;
    std::shared_ptr<ISpxTtsEngineAdapterSite> site;
    RestTtsResponse response;
    std::string userAgent;
} RestTtsRequest;


class CSpxRestTtsEngineAdapter :
    public CSpxCloudTtsEngineAdapter
{
public:

    CSpxRestTtsEngineAdapter();
    virtual ~CSpxRestTtsEngineAdapter();

    SPX_INTERFACE_MAP_BEGIN()
        SPX_INTERFACE_MAP_ENTRY(ISpxObjectWithSite)
        SPX_INTERFACE_MAP_ENTRY(ISpxObjectInit)
        SPX_INTERFACE_MAP_ENTRY(ISpxTtsEngineAdapter)
    SPX_INTERFACE_MAP_END()

    // --- ISpxObjectInit ---
    void Init() override;
    void Term() override;

    // --- ISpxTtsEngineAdapter ---
    std::shared_ptr<ISpxSynthesisResult> Speak(const std::string& text, bool isSsml, const std::string& requestId, bool retry) override;
    std::shared_ptr<ISpxSynthesisResult> Speak(std::shared_ptr<ISpxSynthesisRequestReader> request, bool retry) override;
    void StopSpeaking(const std::shared_ptr<ISpxErrorInformation> &reason = nullptr) override;
    void Connect() override;
    void Disconnect(bool async = false) override;

private:

    using SitePtr = std::shared_ptr<ISpxTtsEngineAdapterSite>;

    CSpxRestTtsEngineAdapter(const CSpxRestTtsEngineAdapter&) = delete;
    CSpxRestTtsEngineAdapter(const CSpxRestTtsEngineAdapter&&) = delete;

    CSpxRestTtsEngineAdapter& operator=(const CSpxRestTtsEngineAdapter&) = delete;

    static std::unique_ptr<ISpxHttpResponse> PostTtsRequest(RestTtsRequest& request, std::shared_ptr<ISpxSynthesisResultInit> result_init, ISpxNamedProperties::Ptr properties);
    static void OnChunkReceived(RestTtsRequest* request, const uint8_t* buffer, size_t size);


private:

    std::string m_endpoint;

};


} } } } // Microsoft::CognitiveServices::Speech::Impl
