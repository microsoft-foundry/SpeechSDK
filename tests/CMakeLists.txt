#
# Copyright (c) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
#
project(tests)

set(SRC_DIR "${PROJECT_SOURCE_DIR}")
set(INPUT_DIR "${SRC_DIR}/input/")

file(GLOB TEST_INPUT ${INPUT_DIR}/**/*)

list(LENGTH TEST_INPUT NUM_TEST_INPUTS)
if(NUM_TEST_INPUTS EQUAL 0 AND NOT BUILD_CORE_ONLY)
    execute_process(COMMAND git submodule update --init)
endif()

add_subdirectory(common)

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
  # Disable link-time optimization (applied globally) for tests
  string(APPEND CMAKE_SHARED_LINKER_FLAGS " -fno-lto")
  string(APPEND CMAKE_EXE_LINKER_FLAGS " -fno-lto")
endif()

# Product code is built without RTTI but test code that doesn't require linkage against internal libraries should
# (and must!) work with RTTI enabled.
string(REPLACE "/GR-" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
string(REPLACE "-fno-rtti" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})

include(Catch2.cmake)

if(BUILD_TESTS)
  if(NOT BUILD_CORE_ONLY)
    add_subdirectory("${SRC_DIR}/unit-tests")
  endif()
  add_subdirectory("${SRC_DIR}/functional/cxx/core")
  if(NOT BUILD_CORE_ONLY)
    add_subdirectory("${SRC_DIR}/functional/cxx/api")
  endif()

  if(BUILD_CSHARP)
    # A "platform" language flag gets set somewhere by CMake, and it's incorrect for arm/arm64. Remove it
    # prior to project generation for these C# apps lest we get warnings about architecture mismatches.
    string(REGEX REPLACE
      "/platform:[a-zA-Z0-9_]*" "" CMAKE_CSharp_FLAGS "${CMAKE_CSharp_FLAGS}")
    if(WIN32)
      add_subdirectory("${SRC_DIR}/functional/csharp/common")
    endif()
    add_subdirectory("${SRC_DIR}/functional/csharp/speech/end2end")
    add_subdirectory("${SRC_DIR}/functional/csharp/speech/console")
    add_subdirectory("${SRC_DIR}/functional/csharp/speech/parallel_runner")
  endif()
  if(BUILD_JAVA)
    add_subdirectory(${JAVA_SPEECH_TESTS})
  endif()
endif()

