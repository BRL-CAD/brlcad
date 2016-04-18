# CMake by default provides four different configurations for multi-
# configuration build tools.  We want only two - Debug and Release.
if(CMAKE_CONFIGURATION_TYPES AND NOT "${CMAKE_CONFIGURATION_TYPES}" STREQUAL "Debug;Release")
  set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Allowed configuration types" FORCE)
endif(CMAKE_CONFIGURATION_TYPES AND NOT "${CMAKE_CONFIGURATION_TYPES}" STREQUAL "Debug;Release")

# Normalize the build type capitalization
if(CMAKE_BUILD_TYPE)
  string(TOUPPER "${CMAKE_BUILD_TYPE}" BUILD_TYPE_UPPER)
  if ("${BUILD_TYPE_UPPER}" STREQUAL "RELEASE")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build Type" FORCE)
  endif ("${BUILD_TYPE_UPPER}" STREQUAL "RELEASE")
  if ("${BUILD_TYPE_UPPER}" STREQUAL "DEBUG")
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build Type" FORCE)
  endif ("${BUILD_TYPE_UPPER}" STREQUAL "DEBUG")
endif(CMAKE_BUILD_TYPE)

# Stash the build type so we can set up a drop-down menu in the gui
if(NOT BRLCAD_IS_SUBBUILD)
  set(CMAKE_BUILD_TYPE "${CMAKE_BUILD_TYPE}" CACHE STRING "high level build configuration")
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "" Debug Release)
endif(NOT BRLCAD_IS_SUBBUILD)
if(CMAKE_CONFIGURATION_TYPES)
  mark_as_advanced(CMAKE_BUILD_TYPE)
else(CMAKE_CONFIGURATION_TYPES)
  mark_as_advanced(CMAKE_BUILD_TYPE)
endif(CMAKE_CONFIGURATION_TYPES)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

