//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"
#include "mocks_manager.h"
#include "try_catch_helpers.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

    static std::string GetKey(uint64_t interfaceTypeId, const std::string& className = "")
    {
        const auto typeIdStr { std::to_string(interfaceTypeId) };
        std::string key;
        key.reserve(typeIdStr.size() + className.size() + 1);
        key += typeIdStr;
        key += ":";
        key += className;
        return key;
    }

    CSpxMocks& CSpxMocks::Instance()
    {
        static CSpxMocks instance;
        return instance;
    }

    void Speech::Impl::CSpxMocks::AddMock(uint64_t interfaceTypeId, const std::string& className, void* context, Generator func)
    {
        SPX_THROW_HR_IF(SPXERR_INVALID_ARG, func == nullptr);
        std::unique_lock<std::mutex> guard(m_lock);
        m_mocks[GetKey(interfaceTypeId, className)] = { className, context, func };
    }

    bool CSpxMocks::RemoveMock(uint64_t interfaceTypeId, const std::string& className)
    {
        std::unique_lock<std::mutex> guard(m_lock);
        return m_mocks.erase(GetKey(interfaceTypeId, className)) > 0;
    }

    bool Speech::Impl::CSpxMocks::HasMock(uint64_t interfaceTypeId, const std::string& className)
    {
        std::unique_lock<std::mutex> guard(m_lock);
        return m_mocks.find(GetKey(interfaceTypeId, className)) != m_mocks.end();
    }

    void* Speech::Impl::CSpxMocks::GetMock(uint64_t interfaceTypeId, const std::string& className)
    {
        std::unique_lock<std::mutex> guard(m_lock);

        Generator func = nullptr;

        // see if we have a specific override first
        auto key = GetKey(interfaceTypeId, className);
        auto entry = m_mocks.find(key);
        if (entry == m_mocks.end() || (func = entry->second.func) == nullptr)
        {
            // see if we have a global override for that interface type
            key = GetKey(interfaceTypeId);
            entry = m_mocks.find(key);
            if (entry == m_mocks.end() || (func = entry->second.func) == nullptr)
            {
                return nullptr;
            }
        }

        return func(entry->second.context);
    }

    void Speech::Impl::CSpxMocks::Clear()
    {
        std::unique_lock<std::mutex> guard(m_lock);
        m_mocks.clear();
    }

} } } } // Microsoft::CognitiveServices::Speech::Impl
