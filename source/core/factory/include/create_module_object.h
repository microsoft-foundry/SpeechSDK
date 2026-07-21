//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// create_module_object.h: Implementation declarations for *CreateModuleObject* methods
//

#pragma once

#include "ispxinterfaces.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

SPX_EXTERN_C void* IntraAssemblyCreateModuleObject(const char* className, uint64_t interfaceTypeId);
SPX_EXTERN_C void AddExtensionModules(std::list<std::shared_ptr<ISpxObjectFactory>>& moduleFactories);

} } } } // Microsoft::CognitiveServices::Speech::Impl
