#
# Copyright (c) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
#
project(util)

set(INC_DIR "${PROJECT_SOURCE_DIR}/include")

set(HEADERS
    "${INC_DIR}/util/buffer.h"
    "${INC_DIR}/util/either.h"
    "${INC_DIR}/util/event.h"
    "${INC_DIR}/util/json_serializable.h"
    "${INC_DIR}/util/maybe.h"
    "${INC_DIR}/util/promise_helpers.h"
    "${INC_DIR}/util/scope_guard.h"
    "${INC_DIR}/util/state_machine.h"
    "${INC_DIR}/util/result.h"
    "${INC_DIR}/util/traits.h"
)

add_library(${PROJECT_NAME} INTERFACE ${HEADERS})

target_include_directories(${PROJECT_NAME}
    INTERFACE
        ${INC_DIR}
)

set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER core/shared)
