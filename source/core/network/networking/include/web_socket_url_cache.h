//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once
#include <string>  
#include <unordered_map>  
#include <chrono>  
#include <mutex>
#include "../stdafx.h"
#include "spxdebug.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

class EndpointCache {
public:
    using KeyType = std::string;
    using ValueType = std::string;
    using TimePoint = std::chrono::steady_clock::time_point;

    // Singleton access method  
    static EndpointCache& getInstance() {
        static EndpointCache instance;
        return instance;
    }

    // Deleted methods to prevent copying  
    EndpointCache(const EndpointCache&) = delete;
    EndpointCache& operator=(const EndpointCache&) = delete;

    // Add or update a cache entry  
    void put(const KeyType& key, const ValueType& value, std::chrono::seconds validityDuration) {
        std::lock_guard<std::mutex> lock(mutex);
        auto expirationTime = std::chrono::steady_clock::now() + validityDuration;
        SPX_TRACE_INFO("Caching %s as %s for %" PRId64 " seconds", key.c_str(), value.c_str(),  static_cast<int64_t>(validityDuration.count()));
        cache[key] = { value, expirationTime };
    }

    // Retrieve a cache entry  
    bool get(const KeyType& key, ValueType& value) {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = cache.find(key);
        if (it != cache.end()) {
            if (std::chrono::steady_clock::now() < it->second.expirationTime) {
                value = it->second.value;
                SPX_TRACE_INFO("Cache hit for %s with value %s", it->first.c_str(), it->second.value.c_str());
                return true;
            }
            else {
                SPX_TRACE_INFO("Removing expired cache entry %s", it->first.c_str());
                // Remove expired entry  
                cache.erase(it);
            }
        }
        return false;
    }

    // Remove a cache entry  
    void remove(const KeyType& key) {
        std::lock_guard<std::mutex> lock(mutex);
        cache.erase(key);
    }

    // Clear all cache entries  
    void clear() {
        std::lock_guard<std::mutex> lock(mutex);
        cache.clear();
    }

private:
    EndpointCache() = default;

    struct CacheEntry {
        ValueType value;
        TimePoint expirationTime;
    };

    std::unordered_map<KeyType, CacheEntry> cache;
    std::mutex mutex;
};
}}}}
