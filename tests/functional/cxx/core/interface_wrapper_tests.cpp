//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#include "stdafx.h"

#include <memory>

#include "interface_wrapper.h"

#include "mocks.h"
#include "test_utils.h"

SPXTEST_CASE_BEGIN("InterfaceWrapper tests", "[cxx][unit][interface_wrapper]")
{
    using Object = MergedMocks<MockBufferDataWriter, MockBufferData>;
    using Wrapper = Carbon::InterfaceWrapper<Carbon::ISpxBufferDataWriter, Carbon::ISpxBufferData, Carbon::ISpxBufferDataWriter>;

    SPXTEST_GIVEN("An uninitialzed wrapper")
    {
        Wrapper wrapper{};
        SPXTEST_WHEN("No calls are made")
        {
            THEN("API should properly inform that no object is contained")
            {
                SPXTEST_REQUIRE_FALSE(wrapper);
            }
        }
        SPXTEST_WHEN("Assignment operator is called with a compatible object")
        {
            auto o = std::make_shared<Object>();
            wrapper = o;
            THEN("API should properly inform that all interfaces are available")
            {
                SPXTEST_REQUIRE(wrapper);
            }
            THEN("Proper objects should be reachable through the accessor member")
            {
                bool writerCalled{ false };
                bool dataCalled{ false };
                o->WriteHandler = [&writerCalled](uint8_t*, uint32_t) { writerCalled = true; };
                o->GetOffsetHandler = [&dataCalled]() -> uint64_t { dataCalled = true; return 0; };
                auto& writer = wrapper.I<Carbon::ISpxBufferDataWriter>();
                auto& data = wrapper.I<Carbon::ISpxBufferData>();
                writer.Write(nullptr, 0);
                data.GetOffset();
                SPXTEST_REQUIRE(writerCalled);
                SPXTEST_REQUIRE(dataCalled);
            }
        }
    }

}
SPXTEST_CASE_END()
