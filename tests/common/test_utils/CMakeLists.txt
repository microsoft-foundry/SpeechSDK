#
# Copyright (c) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
#
project(test_utils)

set(SRC_DIR "${PROJECT_SOURCE_DIR}")
set(INC_DIR "${PROJECT_SOURCE_DIR}/include")

set(HEADERS_PRIVATE
    "${SRC_DIR}/handle_wrapper.h"
)

set(HEADERS
    "${INC_DIR}/catch_test_listeners.h"
    "${INC_DIR}/i_pipe.h"
    "${INC_DIR}/i_process.h"
    "${INC_DIR}/mocks.h"
    "${INC_DIR}/site_with_thread_service.h"
    "${INC_DIR}/test_PAL.h"
    "${INC_DIR}/test_server.h"
    "${INC_DIR}/test_utils.h"
    "${INC_DIR}/unit_test_utils.h"
)

set(SOURCES
    "${SRC_DIR}/test_server.cpp"
)

# Determine the PAL files to use.
# Right now the test server code only works on Windows and Linux to ensure no orphaned
# child process (UWP is Job functionality, POSIX compliant systems other than Linux
# missing PRTCL)
set(PAL_FILES)
if (WIN32 AND NOT CARBONSDK_OS STREQUAL "WindowsUwp")
    list(APPEND PAL_FILES
        "${SRC_DIR}/windows/job.cpp"
        "${SRC_DIR}/windows/job.h"
        "${SRC_DIR}/windows/pipe.cpp"
        "${SRC_DIR}/windows/process.cpp"
        "${SRC_DIR}/windows/test_PAL.cpp"
    )
elseif (CARBONSDK_OS STREQUAL "Linux" OR CARBONSDK_OS STREQUAL "OSX")
    list(APPEND PAL_FILES
        "${SRC_DIR}/posix/pipe.cpp"
        "${SRC_DIR}/posix/process.cpp"
        "${SRC_DIR}/posix/test_PAL.cpp"
    )
else ()
    list(APPEND PAL_FILES
        "${SRC_DIR}/default/pipe.cpp"
        "${SRC_DIR}/default/process.cpp"
        "${SRC_DIR}/default/test_PAL.cpp"
    )
endif()

set_source_groups()
source_group("Source Files/PAL" FILES ${PAL_FILES})

add_library(${PROJECT_NAME} STATIC ${SOURCES} ${HEADERS} ${HEADERS_PRIVATE} ${PAL_FILES})

target_include_directories(${PROJECT_NAME}
    PUBLIC
        "${INC_DIR}"
        "${CARBON_C_API}"
        "${CARBON_CXX_API}"
    PRIVATE
        "${SRC_DIR}"
        "${CARBON_JSON_PARSER}"
)

target_link_libraries(${PROJECT_NAME} PUBLIC
    interfaces
    common
    networking
    Catch2::Catch2
)

set_target_properties (${PROJECT_NAME} PROPERTIES
    FOLDER tests/common
)
