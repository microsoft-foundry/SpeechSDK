#
# Copyright (c) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
#

cmake_minimum_required(VERSION 3.19)

project(network_interfaces)

set(INC_DIR "${PROJECT_SOURCE_DIR}/include")

set(HEADERS
    "${INC_DIR}/interfaces/network/web_socket_telemetry.h"
)

add_library(${PROJECT_NAME} INTERFACE ${HEADERS})

target_include_directories(${PROJECT_NAME}
    INTERFACE
        ${INC_DIR}
)

target_link_libraries(${PROJECT_NAME} INTERFACE network_types)

set_target_properties(${PROJECT_NAME} PROPERTIES
    FOLDER core/network)