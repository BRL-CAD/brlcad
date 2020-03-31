#---------------------------------------------------------------------
# CMake by default provides four different configurations for multi-
# configuration build tools.  We want only two - Debug and Release.
if(NOT ENABLE_ALL_CONFIG_TYPES)
  if(CMAKE_CONFIGURATION_TYPES AND NOT "${CMAKE_CONFIGURATION_TYPES}" STREQUAL "Debug;Release")
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Allowed BRL-CAD configuration types" FORCE)
  endif(CMAKE_CONFIGURATION_TYPES AND NOT "${CMAKE_CONFIGURATION_TYPES}" STREQUAL "Debug;Release")
endif(NOT ENABLE_ALL_CONFIG_TYPES)

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
  set(CMAKE_BUILD_TYPE "${CMAKE_BUILD_TYPE}" CACHE STRING "BRL-CAD high level build configuration")
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "" Debug Release)
endif(NOT BRLCAD_IS_SUBBUILD)
if(CMAKE_CONFIGURATION_TYPES)
  mark_as_advanced(CMAKE_BUILD_TYPE)
else(CMAKE_CONFIGURATION_TYPES)
  mark_as_advanced(CMAKE_BUILD_TYPE)
endif(CMAKE_CONFIGURATION_TYPES)

#------------------------------------------------------------------------------
# For multi-configuration builds, we frequently need to know the run-time build
# directory.  Confusingly, we need to do different things for install commands
# and custom command definitions.  To manage this, we define
# CMAKE_CURRENT_BUILD_DIR_SCRIPT and CMAKE_CURRENT_BUILD_DIR_INSTALL once at
# the top level, then use them when we need the configuration-dependent path.
#
# Note that neither of these will work when we need to generate a .cmake file
# that does runtime detection of the current build configuration.  CMake
# scripts run using "cmake -P script.cmake" style invocation are independent of
# the "main" build system and will not know how to resolve either of the below
# variables correctly.  In that case, the script itself must check the current
# file in CMakeTmp/CURRENT_PATH (TODO - need to better organize and document
# this mechanism, given how critical it is proving... possible convention will
# be to have the string CURRENT_BUILD_DIR in any path that needs the relevant
# logic in a script, and a standard substitution routine...) and set any
# necessary path variables accordingly.  (TODO - make a standard function to do
# that the right way that scripts can load and use - right now that logic is
# scattered all over the code and if we wanted to change where those files went
# it would be a lot of work.)

# TODO - in principle, this mechanism should be replaced with generator
# expressions
if(CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_CURRENT_BUILD_DIR_SCRIPT "${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}")
  set(CMAKE_CURRENT_BUILD_DIR_INSTALL "${CMAKE_BINARY_DIR}/\${BUILD_TYPE}")
else(CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_CURRENT_BUILD_DIR_SCRIPT "${CMAKE_BINARY_DIR}")
  set(CMAKE_CURRENT_BUILD_DIR_INSTALL "${CMAKE_BINARY_DIR}")
endif(CMAKE_CONFIGURATION_TYPES)

# Mark CMAKE_CONFIGURATION_TYPES as advanced - users shouldn't be adjusting this
# directly.
mark_as_advanced(CMAKE_CONFIGURATION_TYPES)


