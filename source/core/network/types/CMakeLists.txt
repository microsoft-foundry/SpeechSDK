#
# Copyright (c) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
#
cmake_minimum_required(VERSION 3.19)

project(network_types)

set(INC_DIR "${PROJECT_SOURCE_DIR}/include")

set(HEADERS
    "${INC_DIR}/network/message/control.h"
    "${INC_DIR}/network/message/visiting_json_reader_view.h"
    "${INC_DIR}/network/usp/message.h"
)

add_library(${PROJECT_NAME} INTERFACE ${HEADERS})

target_include_directories(${PROJECT_NAME}
    INTERFACE
        ${INC_DIR}
        ${CARBON_JSON_PARSER}
)

target_link_libraries(${PROJECT_NAME} INTERFACE
    types
    util)

set_target_properties(${PROJECT_NAME} PROPERTIES
    FOLDER core/network)
