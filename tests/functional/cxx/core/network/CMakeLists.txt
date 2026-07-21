#
# Copyright (c) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
#
project(core_tests_network)

set(SRC_DIR "${PROJECT_SOURCE_DIR}")

set(PRIVATE_HEADERS
  "${SRC_DIR}/mock_usp_service.h"
)

set(SOURCES
    "${SRC_DIR}/network_utils_tests.cpp"
    "${SRC_DIR}/websocket.cpp"
)

set_source_groups()

add_library(${PROJECT_NAME} OBJECT ${SOURCES} ${PRIVATE_HEADERS})

target_include_directories(${PROJECT_NAME}
    PRIVATE
        "${CARBON_C_API}"
        "${CARBON_CXX_API}"
        "${CARBON_JSON_PARSER}"
        "${TEST_INCLUDE}"
)

target_link_libraries(${PROJECT_NAME} PUBLIC test_utils network_interfaces network_util Catch2::Catch2)

set_target_properties (${PROJECT_NAME} PROPERTIES
    FOLDER tests/unit/areas
)

if (MSVC)
  set (_NO_RTTI_FLAG "/GR-")
else()
  set (_NO_RTTI_FLAG "-fno-rtti")
endif()

if (NOT DEFINED MACOSX)
  set_source_files_properties(
    network_utils_tests.cpp
    PROPERTIES COMPILE_FLAGS ${_NO_RTTI_FLAG})
endif()
