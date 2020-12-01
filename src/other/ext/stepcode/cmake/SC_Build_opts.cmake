# BIN and LIB directories
if(NOT DEFINED BIN_DIR)
  set(BIN_DIR bin)
endif(NOT DEFINED BIN_DIR)

if(NOT DEFINED LIB_DIR)
  set(LIB_DIR lib)
endif(NOT DEFINED LIB_DIR)

# testing and compilation options, build output dirs, install dirs, etc
# included by root CMakeLists

if(NOT DEFINED INCLUDE_INSTALL_DIR)
  set(INCLUDE_INSTALL_DIR include)
endif(NOT DEFINED INCLUDE_INSTALL_DIR)

if(NOT DEFINED LIB_INSTALL_DIR)
  set(LIB_INSTALL_DIR lib)
endif(NOT DEFINED LIB_INSTALL_DIR)

if(NOT DEFINED BIN_INSTALL_DIR)
  set(BIN_INSTALL_DIR bin)
endif(NOT DEFINED BIN_INSTALL_DIR)

if(NOT DEFINED SC_BUILD_TYPE)
  set(SC_BUILD_TYPE "Debug" CACHE STRING "Build type") # By default set debug build
endif(NOT DEFINED SC_BUILD_TYPE)
if(NOT SC_IS_SUBBUILD)
  set(CMAKE_BUILD_TYPE ${SC_BUILD_TYPE} CACHE INTERNAL "Build type, immutable" FORCE)
else(NOT SC_IS_SUBBUILD)
  set(CMAKE_BUILD_TYPE ${SC_BUILD_TYPE})
endif(NOT SC_IS_SUBBUILD)

# Define helper macro OPTION_WITH_DEFAULT
macro(OPTION_WITH_DEFAULT OPTION_NAME OPTION_STRING OPTION_DEFAULT)
  if(NOT DEFINED ${OPTION_NAME})
    set(${OPTION_NAME} ${OPTION_DEFAULT})
  endif(NOT DEFINED ${OPTION_NAME})
  option(${OPTION_NAME} "${OPTION_STRING}" ${${OPTION_NAME}})
endmacro(OPTION_WITH_DEFAULT OPTION_NAME OPTION_STRING OPTION_DEFAULT)

# build shared libs by default
OPTION_WITH_DEFAULT(SC_BUILD_SHARED_LIBS "Build shared libs" ON)

# don't build static libs by default
OPTION_WITH_DEFAULT(SC_BUILD_STATIC_LIBS "Build static libs" OFF)

OPTION_WITH_DEFAULT(SC_PYTHON_GENERATOR "Compile exp2python" ON)
OPTION_WITH_DEFAULT(SC_CPP_GENERATOR "Compile exp2cxx" ON)

OPTION_WITH_DEFAULT(SC_MEMMGR_ENABLE_CHECKS "Enable sc_memmgr's memory leak detection" OFF)
OPTION_WITH_DEFAULT(SC_TRACE_FPRINTF "Enable extra comments in generated code so the code's source in exp2cxx may be located" OFF)

# Should we use C++11?
OPTION_WITH_DEFAULT(SC_ENABLE_CXX11 "Build with C++ 11 features" ON)

# Get version from git
OPTION_WITH_DEFAULT(SC_GIT_VERSION "Build using version from git" ON)

option(SC_BUILD_EXPRESS_ONLY "Only build express parser." OFF)
mark_as_advanced(SC_BUILD_EXPRESS_ONLY)

option(DEBUGGING_GENERATED_SOURCES "disable md5 verification of generated sources" OFF)
mark_as_advanced(DEBUGGING_GENERATED_SOURCES)

#---------------------------------------------------------------------
# Coverage option
OPTION_WITH_DEFAULT(SC_ENABLE_COVERAGE "Enable code coverage test" OFF)
if(SC_ENABLE_COVERAGE)
  set(SC_ENABLE_TESTING ON CACHE BOOL "Testing enabled by coverage option" FORCE)
  # build static libs, better coverage report
  set(SC_BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libs" FORCE)
  set(SC_BUILD_STATIC_LIBS ON CACHE BOOL "Build static libs" FORCE)
  set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -fprofile-arcs -ftest-coverage" CACHE STRING "Extra compile flags required by code coverage" FORCE)
  set(CMAKE_C_FLAGS_DEBUG "-O0 -g -fprofile-arcs -ftest-coverage" CACHE STRING "Extra compile flags required by code coverage" FORCE)
  set(CMAKE_MODULE_LINKER_FLAGS_DEBUG "-fprofile-arcs -ftest-coverage" CACHE STRING "Extra linker flags required by code coverage" FORCE)
  set(SC_BUILD_TYPE "Debug" CACHE STRING "Build type required by testing framework" FORCE)
  set(SC_PYTHON_GENERATOR OFF) #won't build with static libs
endif(SC_ENABLE_COVERAGE)

#---------------------------------------------------------------------
# Testing option
OPTION_WITH_DEFAULT(SC_ENABLE_TESTING "Enable unittesting framework" OFF)
if(SC_ENABLE_TESTING)
  if(NOT ${SC_BUILD_EXPRESS_ONLY} AND NOT DEFINED SC_BUILD_SCHEMAS)
    set(SC_BUILD_SCHEMAS "ALL") #test all schemas, unless otherwise specified
  endif()
  include(CTest)
endif(SC_ENABLE_TESTING)

#---------------------------------------------------------------------
# Executable install option
OPTION_WITH_DEFAULT(SC_SKIP_EXEC_INSTALL "Skip installing executables" OFF)
if(SC_SKIP_EXEC_INSTALL)
  set(SC_EXEC_NOINSTALL "NO_INSTALL")
endif(SC_SKIP_EXEC_INSTALL)

#---------------------------------------------------------------------
# The following logic is what allows binaries to run successfully in
# the build directory AND install directory.  Thanks to plplot for
# identifying the necessity of setting CMAKE_INSTALL_NAME_DIR on OSX.
# Documentation of these options is available at
# http://www.cmake.org/Wiki/CMake_RPATH_handling

# use, i.e. don't skip the full RPATH for the build tree
set(CMAKE_SKIP_BUILD_RPATH  FALSE)

# when building, don't use the install RPATH already
# (but later on when installing)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)

# the RPATH/INSTALL_NAME_DIR to be used when installing
if (NOT APPLE)
  set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib:\$ORIGIN/../lib")
endif(NOT APPLE)
# On OSX, we need to set INSTALL_NAME_DIR instead of RPATH
# http://www.cmake.org/cmake/help/cmake-2-8-docs.html#variable:CMAKE_INSTALL_NAME_DIR
set(CMAKE_INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}/lib")

# add the automatically determined parts of the RPATH which point to
# directories outside the build tree to the install RPATH
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# When this is a subbuild, assume that the parent project controls all of the following
#======================================================================================
if(NOT SC_IS_SUBBUILD)

  # Output directories. In a separate file so it can be used by the schema scanner CMake as well.
  include(${SC_CMAKE_DIR}/SC_Outdirs.cmake)

  #-----------------------------------------------------------------------------
  # Configure install locations. Only do this if CMAKE_INSTALL_PREFIX hasn't
  # been set already, to try and allow parent builds (if any) some control.
  #
  # Need a good Debug location for Windows.
  if(NOT WIN32)
    if(${CMAKE_BUILD_TYPE} MATCHES "Debug")
      set(SC_INSTALL_PREFIX "${SC_SOURCE_DIR}/../sc-install")
    else()
      set(SC_INSTALL_PREFIX "/usr/local")
    endif()
  endif(NOT WIN32)
  set(SC_INSTALL_PREFIX ${SC_INSTALL_PREFIX} CACHE
    PATH "Install prefix prepended to target to create install location")
  set(CMAKE_INSTALL_PREFIX ${SC_INSTALL_PREFIX} CACHE INTERNAL "Prefix prepended to install directories if target destination is not absolute, immutable" FORCE)

  #-----------------------------------------------------------------------------
  # SC Packaging
  # $make package
  set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "STEPcode")
  set(CPACK_SET_DESTDIR "ON")
  set(CPACK_PACKAGE_VERSION_MAJOR ${SC_VERSION_MAJOR})
  set(CPACK_PACKAGE_VERSION_MINOR ${SC_VERSION_MINOR})
  set(CPACK_PACKAGE_NAME SC)
  set(CPACK_PACKAGE_CONTACT "SC Developers <scl-dev@googlegroups.com>")
  include(CPack)

  #-----------------------------------------------------------------------------
  # Uninstall target
  # From http://www.cmake.org/Wiki/CMake_FAQ#Can_I_do_.22make_uninstall.22_with_CMake.3F
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)
  add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)

endif(NOT SC_IS_SUBBUILD)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

