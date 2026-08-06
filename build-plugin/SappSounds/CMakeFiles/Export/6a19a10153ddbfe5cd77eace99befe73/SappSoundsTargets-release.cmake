#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "Sapp::SappSounds" for configuration "Release"
set_property(TARGET Sapp::SappSounds APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(Sapp::SappSounds PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libSappSounds.a"
  )

list(APPEND _cmake_import_check_targets Sapp::SappSounds )
list(APPEND _cmake_import_check_files_for_Sapp::SappSounds "${_IMPORT_PREFIX}/lib/libSappSounds.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
