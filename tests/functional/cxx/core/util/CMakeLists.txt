#
# Copyright (c) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
#
project(core_tests_util)

set(SRC_DIR "${PROJECT_SOURCE_DIR}")

set(SOURCES
    "${SRC_DIR}/either_tests.cpp"
    "${SRC_DIR}/result_tests.cpp"
    "${SRC_DIR}/file_utils.cpp"
    "${SRC_DIR}/platform_tests.cpp"
)

set_source_groups()

add_library(${PROJECT_NAME} OBJECT ${SOURCES})

target_include_directories(${PROJECT_NAME}
    PRIVATE
        "${CARBON_C_API}"
        "${CARBON_CXX_API}"
        "${CARBON_JSON_PARSER}"
        "${TEST_INCLUDE}"
)

target_link_libraries(${PROJECT_NAME} PUBLIC test_utils common util Catch2::Catch2)

set_target_properties (${PROJECT_NAME} PROPERTIES
    FOLDER tests/unit/areas
)

if (MSVC)
  set (_NO_RTTI_FLAG "/GR-")
else()
  set (_NO_RTTI_FLAG "-fno-rtti")
endif()
