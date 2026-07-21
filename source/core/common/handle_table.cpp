//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
//

#include "stdafx.h"
#include "handle_table.h"
#include "platform.h"
#include <memory>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

using counterMap = std::map<uint64_t, CSpxHandleCounter*>;

std::mutex CSpxSharedPtrHandleTableManager::s_mutex;

// On linux, this static member is destroyed before LibUnload (marked as __attribute__((destructor))).
// Using a deleter instead to clean everything up before shutting down.
CSpxSharedPtrHandleTableManager::deleted_unique_ptr<counterMap> CSpxSharedPtrHandleTableManager::s_counters(new counterMap(), [] (counterMap* map) {

#ifdef ASAN_BUILD 
// Only clear the table out for a ASAN enabled build. 
// This prevents issues where an entry in the handle table references objects in its destructor that have already been destroyed, like static objects.
    for (const auto& item : *map) {
        SPX_TRACE_VERBOSE("Deleting handle table counter for type id=%" PRIu64 ", name=%s\n", item.second->Id(), item.second->Name());
        delete item.second;
    }
    map->clear();
#endif

    delete map;
});

} } } } // Microsoft::CognitiveServices::Speech::Impl

