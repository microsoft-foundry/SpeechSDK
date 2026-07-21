#
# Copyright (c) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
#

##
## Catch2 inclusion
##  We populate this from a known-good release tag using CMake's FetchContent module. This allows use
##  to grab and use the code without needing to manage a submodule.
##
##  Alternatively, if CATCH2_OVERRIDE_SRC is defined, that path will be used.
##

if (DEFINED CATCH2_OVERRIDE_SRC)
  message("-- Using override source for Catch2 dependency: ${CATCH2_OVERRIDE_SRC}")
  if (NOT (EXISTS ${CATCH2_OVERRIDE_SRC} AND EXISTS ${CATCH2_OVERRIDE_SRC}/src))
    message(FATAL_ERROR 
      "* CATCH2_OVERRIDE_SRC was specified but the location was not found.")
  endif()

  add_subdirectory(
    "${CATCH2_OVERRIDE_SRC}"
    "${CMAKE_CURRENT_BINARY_DIR}/Catch2-build")
  include_directories("${CATCH2_OVERRIDE_SRC}/src")
else()
  include(FetchContent)

  # Catch2 v3 reads CATCH_CONFIG_CONSOLE_WIDTH at configure time and bakes the
  # value into the generated catch_user_config.hpp. Setting it here (with
  # FORCE, to override Catch2's own cached default) avoids the equivalent
  # target_compile_definitions on consumer targets, which would produce a
  # macro-redefinition warning at compile time. Adjust the value below if
  # the default Catch2 console width is too narrow for typical assertion
  # output.
  # set(CATCH_CONFIG_CONSOLE_WIDTH 300 CACHE STRING "Catch2 console output width" FORCE)

  FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.6.0
    GIT_SHALLOW    ON
  )
  FetchContent_GetProperties(Catch2)
  if(NOT catch2_POPULATED)
    string(TIMESTAMP currentTime "%M:%S")
    message("-- ${currentTime} Starting FetchContent_Populate(Catch2)...")
    cmake_policy(PUSH)
    if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.30.0")
      cmake_policy(SET CMP0169 OLD)  # suppress warning about FetchContent_Populate
    endif()
    FetchContent_Populate(Catch2)
    cmake_policy(POP)

    # Apply Catch2 carry patches. Check first so that
    # reconfigure on a populated tree is a no-op.
    find_package(Git)
    if(Git_FOUND AND NOT EXISTS "${catch2_SOURCE_DIR}/src/catch2/reporters/catch_reporter_vstest.cpp")
      message("-- Applying Catch2 carry patches...")
      file(GLOB CATCH2_PATCHES "${CMAKE_CURRENT_LIST_DIR}/catch2-patches/*.patch")
      list(SORT CATCH2_PATCHES)
      foreach(p ${CATCH2_PATCHES})
        # Check if the patch can be applied cleanly
        execute_process(
          COMMAND git apply --check "${p}"
          WORKING_DIRECTORY "${catch2_SOURCE_DIR}"
          RESULT_VARIABLE check_rc
        )
        if(NOT check_rc EQUAL 0)
          message(FATAL_ERROR "Cannot apply Catch2 carry patch: ${p}")
        endif()
        # Apply the patch
        execute_process(
          COMMAND git apply "${p}"
          WORKING_DIRECTORY "${catch2_SOURCE_DIR}"
          RESULT_VARIABLE apply_rc
        )
        if(NOT apply_rc EQUAL 0)
          message(FATAL_ERROR "Failed to apply Catch2 carry patch: ${p}")
        endif()
      endforeach()
    endif()

    string(TIMESTAMP currentTime "%M:%S")
    message(
      "-- ${currentTime} End FetchContent_Populate(Catch2)
      -- src : ${catch2_SOURCE_DIR}
      -- bin : ${catch2_BINARY_DIR}")
    add_subdirectory(${catch2_SOURCE_DIR} ${catch2_BINARY_DIR})
  endif()
endif()

# Used to make the Catch2 harness properly emit traces to test run logs
target_compile_definitions(
  Catch2 PUBLIC
  CATCH_CONFIG_EXPERIMENTAL_REDIRECT
  CATCH_CONFIG_NEW_CAPTURE
)

# As an external dependency, we really don't want to see warnings related to Catch2's compilation.
get_target_property(CATCH2_COMPILE_OPTIONS Catch2 COMPILE_OPTIONS)

if (NOT ("${CATCH2_COMPILE_OPTIONS}" STREQUAL "CATCH2_COMPILE_OPTIONS-NOTFOUND"))
  set(NEW_CATCH2_COMPILE_OPTIONS "${CATCH2_COMPILE_OPTIONS}")
endif()

  list(REMOVE_ITEM NEW_CATCH2_COMPILE_OPTIONS /W4)
  list(REMOVE_ITEM NEW_CATCH2_COMPILE_OPTIONS /we4242)
  list(REMOVE_ITEM NEW_CATCH2_COMPILE_OPTIONS /we4254)
  list(REMOVE_ITEM NEW_CATCH2_COMPILE_OPTIONS /WX)
  if (MSVC)
    list(APPEND NEW_CATCH2_COMPILE_OPTIONS /EHsc)
  endif()

set_target_properties(Catch2 PROPERTIES
  FOLDER tests/dependencies
  COMPILE_OPTIONS "${NEW_CATCH2_COMPILE_OPTIONS}")
set_target_properties(Catch2WithMain PROPERTIES
  FOLDER tests/dependencies
  COMPILE_OPTIONS "${NEW_CATCH2_COMPILE_OPTIONS}")

# Ubuntu 22.04 complains about missing () in in the Catch2 source code. Disable these for now 
if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options("-Wno-error=parentheses")
endif()
