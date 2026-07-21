#
# Copyright (c) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
#
project(factory)

set(SRC_DIR "${PROJECT_SOURCE_DIR}")
set(INC_DIR "${PROJECT_SOURCE_DIR}/include")

set(HEADERS
    "${SRC_DIR}/stdafx.h"
    "${INC_DIR}/create_module_object.h"
    "${INC_DIR}/resource_manager.h"
    "${SRC_DIR}/resource_manager_transport_factory_impl.h"
    )

set(SOURCES
    "${SRC_DIR}/create_module_object.cpp"
    "${SRC_DIR}/resource_manager.cpp"
    "${SRC_DIR}/factory_site_helpers.cpp"
    "${SRC_DIR}/stdafx.cpp"
    )

set_source_groups()

add_library(factory STATIC ${HEADERS} ${HEADERS_PRIVATE} ${SOURCES})

target_include_directories(factory
    PRIVATE
        ${CARBON_C_API}
        ${CARBON_CXX_API}
        ${CARBON_JSON_PARSER}
    PUBLIC
        ${INC_DIR}
)

add_security_compile_flags(${PROJECT_NAME})

set_target_properties (factory PROPERTIES
    FOLDER core
)

target_link_libraries(factory
                      PRIVATE interfaces
                      PRIVATE common
                      )
