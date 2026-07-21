//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include <i_process.h>

namespace Azure {
namespace AI {
namespace Test {
namespace Tools {

    std::unique_ptr<IProcess> StartProcess(const ProcessStartInfo& info)
    {
        (void)info;
        
        return std::unique_ptr<IProcess>{};
    }

    std::string GetCurrentProcessId()
    {
        return std::string{};
    }

}}}}
