#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "ispc::libispc_shared" for configuration "Release"
set_property(TARGET ispc::libispc_shared APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(ispc::libispc_shared PROPERTIES
  IMPORTED_IMPLIB_RELEASE "${_IMPORT_PREFIX}/lib/ispc.lib"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/bin/ispc.dll"
  )

list(APPEND _IMPORT_CHECK_TARGETS ispc::libispc_shared )
list(APPEND _IMPORT_CHECK_FILES_FOR_ispc::libispc_shared "${_IMPORT_PREFIX}/lib/ispc.lib" "${_IMPORT_PREFIX}/bin/ispc.dll" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
