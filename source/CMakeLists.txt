#
# Copyright (c) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
#
if(NOT BUILD_CORE_ONLY)
  add_subdirectory(extensions)
  add_subdirectory(templates)
endif()
add_subdirectory(core)
add_subdirectory(public)