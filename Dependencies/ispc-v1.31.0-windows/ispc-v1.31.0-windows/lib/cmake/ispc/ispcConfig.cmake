#
#  Copyright (c) 2025, Intel Corporation
#
#  SPDX-License-Identifier: BSD-3-Clause

# ispcConfig.cmake.in


####### Expanded from @PACKAGE_INIT@ by configure_package_config_file() #######
####### Any changes to this file will be overwritten by the next CMake run ####
####### The input file was ispcConfig.cmake.in                            ########

get_filename_component(PACKAGE_PREFIX_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../" ABSOLUTE)

macro(set_and_check _var _file)
  set(${_var} "${_file}")
  if(NOT EXISTS "${_file}")
    message(FATAL_ERROR "File or directory ${_file} referenced by variable ${_var} does not exist !")
  endif()
endmacro()

macro(check_required_components _NAME)
  foreach(comp ${${_NAME}_FIND_COMPONENTS})
    if(NOT ${_NAME}_${comp}_FOUND)
      if(${_NAME}_FIND_REQUIRED_${comp})
        set(${_NAME}_FOUND FALSE)
      endif()
    endif()
  endforeach()
endmacro()

####################################################################################

# Only include exports if library targets exist
if(TRUE AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/ispc_Exports.cmake")
    include("${CMAKE_CURRENT_LIST_DIR}/ispc_Exports.cmake")
endif()

check_required_components("ispc")

# Set up paths
set(ISPC_INCLUDE_DIRS "${PACKAGE_PREFIX_DIR}/include")
set(ISPC_EXECUTABLE "${PACKAGE_PREFIX_DIR}/bin/ispc")

# Set variables for backward compatibility
set(ISPC_LIBRARIES "${PACKAGE_PREFIX_DIR}/lib")

# Check if targets were built
set(ISPC_SHARED_LIBRARY_AVAILABLE TRUE)

if(ISPC_SHARED_LIBRARY_AVAILABLE)
    if(WIN32)
        set(ISPC_LIBRARY "${ISPC_LIBRARIES}/ispc.dll")
    else()
        set(ISPC_LIBRARY "${ISPC_LIBRARIES}/libispc.so")
    endif()
endif()

# Create convenience aliases for common naming patterns
if(ISPC_SHARED_LIBRARY_AVAILABLE AND TARGET ispc::libispc_shared)
    add_library(ispc::lib ALIAS ispc::libispc_shared)
endif()

# Set standard variables
set(ISPC_FOUND TRUE)

# Provide summary
include(FindPackageMessage)
if(ISPC_SHARED_LIBRARY_AVAILABLE)
    find_package_message(ispc "Found ISPC: ${PACKAGE_PREFIX_DIR}" "[${PACKAGE_PREFIX_DIR}]")
else()
    find_package_message(ispc "Found ISPC: ${PACKAGE_PREFIX_DIR} (executable only)" "[${PACKAGE_PREFIX_DIR}]")
endif()
