//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#pragma once

#include <map>
#include <string>
#include <mutex>
#include <functional>
#include "spxcore_common.h"
#include "interfaces/base.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

    class CSpxMocks
    {
    public:
        using Generator = std::function<void*(void*)>;

        static CSpxMocks& Instance();
        void AddMock(uint64_t interfaceTypeId, const std::string& className, void* context, Generator func);
        bool RemoveMock(uint64_t interfaceTypeId, const std::string& className);
        bool HasMock(uint64_t interfaceTypeId, const std::string& className);
        void* GetMock(uint64_t interfaceTypeId, const std::string& className);
        void Clear();

    private:
        struct Entry
        {
            std::string name;
            void* context;
            Generator func;
        };

        CSpxMocks() = default;

        std::mutex m_lock;
        std::map<std::string, Entry> m_mocks;
    };

} } } } // Microsoft::CognitiveServices::Speech::Impl
