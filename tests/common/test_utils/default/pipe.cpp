//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include <i_pipe.h>

namespace Azure {
namespace AI {
namespace Test {
namespace Tools {

    std::unique_ptr<IPipe> CreateServerPipe(const std::string& name, IPipe::Direction dir)
    {
        (void)name;
        (void)dir;

        return std::unique_ptr<IPipe>{};
    }

}}}}
