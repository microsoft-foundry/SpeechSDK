#
# Copyright (c) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
#
project(pal_dynamic_azure_c_shared)

set(SRC_DIR "${PROJECT_SOURCE_DIR}")
set(INC_DIR "${PROJECT_SOURCE_DIR}/include")
set(HELPER_DIR "${PROJECT_SOURCE_DIR}/../pal_azure_core/helpers")

set(HEADERS
)

set(HEADERS_PRIVATE
    "${SRC_DIR}/http_platform_impl.h"
    "${SRC_DIR}/stdafx.h"
)

set(SOURCES
    "${HELPER_DIR}/az_buffer_helper.cpp"
    "${HELPER_DIR}/az_lock_helper.cpp"
    "${HELPER_DIR}/az_strings_helper.cpp"
    "${HELPER_DIR}/az_threadapi_helper.cpp"
    "${SRC_DIR}/http_platform_impl.cpp"
    "${SRC_DIR}/pal_create_module_object.cpp"
)

set_source_groups()

add_library(${PROJECT_NAME} STATIC ${HEADERS} ${HEADERS_PRIVATE} ${SOURCES})

target_compile_definitions(${PROJECT_NAME} PUBLIC "NETWORKING_NONE")

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CARBON_C_API}
        ${CARBON_CXX_API}
    PUBLIC
        ${INC_DIR}
)

add_security_compile_flags(${PROJECT_NAME})

set_target_properties(${PROJECT_NAME} PROPERTIES
    FOLDER core/shared
    COMPILE_DEFINITIONS "PAL_CONFIG_EXPORTAPIS"
)

# Because of the special way we create and link iOS test projects, we need to manually specify all necessary
# static libraries to link against
list(APPEND IOS_ADDITIONAL_STATIC_LIBS
    $<TARGET_FILE:${PROJECT_NAME}>
    # you can add any additional full paths to static libraries here as well
)

set(ADDITIONAL_LIBS)
if(NOT WIN32)
  find_package(Threads REQUIRED)
  list(APPEND ADDITIONAL_LIBS Threads::Threads)
endif()

target_link_libraries(${PROJECT_NAME}
    PRIVATE interfaces
    PRIVATE common
    PRIVATE logging
    PUBLIC network_pal
    PUBLIC ${ADDITIONAL_LIBS}
)

# CMake copies by value into child scopes. Explicitly update the parent scope to ensure other projects
# see the additional libs
if(NOT BUILD_CORE_ONLY)
  message(STATUS "IOS_ADDITIONAL_STATIC_LIBS=${IOS_ADDITIONAL_STATIC_LIBS}")
endif()
set(IOS_ADDITIONAL_STATIC_LIBS ${IOS_ADDITIONAL_STATIC_LIBS} PARENT_SCOPE)
