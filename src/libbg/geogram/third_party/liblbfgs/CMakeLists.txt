#.rst
# CMake configuration of libLBFGS project
# ---------------------------------------
#
# This CMakeLists.txt defines some libLBFGS specific configuration variables
# using a custom "subproject_define" command defined in the Subproject.cmake module.
# The default values of these variables can be overridden either on the CMake
# command-line using the -D option of the cmake command or in a super-project
# which includes the libLBFGS source tree by setting the LBFGS_<varname>
# CMake variables before adding the libLBFGS source directory via CMake's
# add_subdirectory command. Only when the non-cached variable LBFGS_IS_SUBPROJECT
# has a value equivalent to FALSE, these configuration variables are added to
# the CMake cache so they can be edited in the CMake GUI. By default,
# LBFGS_IS_SUBPROJECT is set to TRUE when the CMAKE_SOURCE_DIR is not identical
# to the directory of this CMakeLists.txt  file, i.e., the top-level directory of
# the libLBFGS project source tree.
#
# Example CMakeLists.txt of user project which requires separate libLBFGS
# installation (possibly requires FindLBFGS.cmake module for distribution
# packages of libLBFGS that do not include a LBFGSConfig.cmake file)::
#
#    cmake_minimum_required(VERSION 2.8.12 FATAL_ERROR)
#
#    project(Foo)
#
#    find_package(LBFGS REQUIRED)
#
#    add_executable(foo src/foo.cc)
#    target_link_libraries(foo LBFGS::lib)
#
# Example CMakeLists.txt of super-project which contains libLBFGS source tree::
#
#    cmake_minimum_required(VERSION 2.8.12 FATAL_ERROR)
#
#    project(Foo)
#
#    set(LBFGS_USE_SSE ON)
#    set(LBFGS_lib_TARGET_NAME liblbfgs)
#    add_subdirectory(lbfgs)
#    set_target_properties(liblbfgs PROPERTIES OUTPUT_NAME foolbfgs)
#
#    add_executable(foo src/foo.cc)
#    target_link_libraries(foo liblbfgs)
#    # or use the usual namespaced ALIAS target:
#    #target_link_libraries(foo LBFGS::lib)
#
# Variables to configure the source files::
#
#    LBFGS_USE_DOUBLE  - Enable double precision floating point arithmetics. (default: ON)
#    LBFGS_USE_SSE     - Enable SSE/SSE2 optimiations. (default: OFF)
#    LBFGS_USE_IEEE754 - Enable optimization routines for IEEE754 floating point values. (default: ON)
#
# Variables to configure the build::
#
#    LBFGS_BUILD_SHARED_LIBS    - Enable build of shared libraries. (default: OFF)
#    LBFGS_BUILD_EXAMPLES       - Enable build of example programs. (default: OFF)
#    LBFGS_<target>_TARGET_NAME - Custom target name for target <target>, i.e., "lib" or "sample".
#                                 By default, the target name is prefixed by "lbfgs_" if this project
#                                 is configured as a subproject of another project.
#    LBFGS_NO_ALIASES           - Do not add ALIAS targets LBFGS::lib and LBFGS::sample. (default: OFF)
#
# Variables to configure the installation::
#
#    LBFGS_INSTALL_STATIC_LIBS - Whether to install static library files.
#                                Shared libraries are always installed.
#                                When a library is installed, its public header
#                                files are installed as well. The default is
#                                to not install static libraries when this
#                                project is a subproject of another project.
#    LBFGS_INSTALL_HEADERS     - Can be used to omit installation of public header files.
#    LBFGS_INSTALL_CONFIG      - Whether to install CMake configuration files.
#                                By default, the CMake configuration files are
#                                installed when the library itself is installed.
#    LBFGS_INSTALL_RUNTIME_DIR - Installation directory for runtime files. (default: bin)
#    LBFGS_INSTALL_INCLUDE_DIR - Installation directory for public header files. (default: include)
#    LBFGS_INSTALL_LIBRARY_DIR - Installation directory for library files. (default: lib)
#    LBFGS_INSTALL_CONFIG_DIR  - Installation directory for CMake configuration. (default: lib/cmake/liblbfgs)

# ==============================================================================
# libLBFGS: C library of limited-memory BFGS (L-BFGS)
#
# Copyright (c) 1990, Jorge Nocedal
# Copyright (c) 2007-2010, Naoaki Okazaki
#
# libLBFGS is distributed under the term of the MIT license.
# Please refer to COPYING file in the distribution.
# ==============================================================================

# ----------------------------------------------------------------------------
# CMake version and policies
cmake_minimum_required(VERSION 2.8.12 FATAL_ERROR)

if (POLICY CMP0042)
  cmake_policy(SET CMP0042 NEW)
endif ()

# ----------------------------------------------------------------------------
# package info
subproject(LBFGS VERSION 1.10.0 LANGUAGES C)

set(PACKAGE_NAME      "libLBFGS")
set(PACKAGE_STRING    "${PACKAGE_NAME} ${PROJECT_VERSION}")
set(PACKAGE_TARNAME   "liblbfgs-${PROJECT_VERSION}")
set(PACKAGE_BUGREPORT "https://github.com/chokkan/liblbfgs/issues")

# ----------------------------------------------------------------------------
# options
subproject_define(BOOL BUILD_SHARED_LIBS "Enable build of shared libraries"                OFF)
subproject_define(BOOL BUILD_EXAMPLES    "Enable build of sample programs"                 OFF)
subproject_define(BOOL USE_DOUBLE        "Use double precision floating point arithmetics" ON)
subproject_define(BOOL USE_SSE           "Use SSE/SSE2 instructions for optimization"      OFF)
subproject_define(BOOL USE_IEEE754       "Activate optimization routines for IEEE754 floating point values" ON)

subproject_set_property(USE_IEEE754 ADVANCED TRUE)

# ----------------------------------------------------------------------------
# checks for SSE/SSE2 instructions header files
if (USE_SSE)
  include(CheckIncludeFile)
  if (USE_DOUBLE)
    check_include_file(emmintrin.h HAVE_EMMINTRIN_H)
    if (NOT HAVE_EMMINTRIN_H)
      message(WARNING "SSE2 instructions header file emmintrin.h not found. Disabled SSE optimizations.")
      subproject_set_property(USE_SSE VALUE OFF)
    endif ()
  else ()
    check_include_file(xmmintrin.h HAVE_XMMINTRIN_H)
    if (NOT HAVE_XMMINTRIN_H)
      message(WARNING "SSE instructions header file xmmintrin.h not found. Disabled SSE optimizations.")
      subproject_set_property(USE_SSE VALUE OFF)
    endif ()
  endif ()
endif ()

# ----------------------------------------------------------------------------
# library
set(HEADERS
  "include/lbfgs.h"
)

set(SOURCES
  "lib/lbfgs.c"
  "lib/arithmetic_ansi.h"
  "lib/arithmetic_sse_float.h"
  "lib/arithmetic_sse_double.h"
)

subproject_add_library(_lib "lib" ${HEADERS} ${SOURCES})

set_target_properties(${_lib} PROPERTIES
  OUTPUT_NAME   lbfgs
  VERSION       ${PROJECT_VERSION}
  SOVERSION     ${PROJECT_SOVERSION}
  DEBUG_POSTFIX d
  POSITION_INDEPENDENT_CODE TRUE
)

target_include_directories(${_lib}
  PUBLIC  "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
  PRIVATE "${PROJECT_SOURCE_DIR}/lib"
)

if (NOT USE_DOUBLE)
  target_compile_definitions(${_lib} INTERFACE LBFGS_FLOAT=32)
endif ()
if (NOT USE_IEEE754)
  target_compile_definitions(${_lib} INTERFACE LBFGS_IEEE_FLOAT=0)
endif ()
if (USE_SSE)
  if (USE_DOUBLE)
    target_compile_definitions(${_lib} PRIVATE HAVE_EMMINTRIN_H=${HAVE_EMMINTRIN_H})
    if (MSVC)
      target_compile_definitions(${_lib} PRIVATE __SSE2__)
    endif ()
  else ()
    target_compile_definitions(${_lib} PRIVATE HAVE_XMMINTRIN_H=${HAVE_XMMINTRIN_H})
    if (MSVC)
      target_compile_definitions(${_lib} PRIVATE __SSE__)
    endif ()
  endif ()
endif ()

if (CMAKE_COMPILER_IS_GNUC)
  target_compile_options(${_lib} PRIVATE "$<$<CONFIG:Release>:-ffast-math>")
  if (USE_SSE)
    if (USE_DOUBLE)
      target_compile_options(${_lib} PRIVATE "-msse2")
    else ()
      target_compile_options(${_lib} PRIVATE "-msse")
    endif ()
  endif ()
endif ()

subproject_install_library(${_lib}
  RUNTIME_DESTINATION "bin"
  LIBRARY_DESTINATION "lib"
  INCLUDE_DESTINATION "include"
  PUBLIC_HEADER_FILES "${HEADERS}"
)

# ----------------------------------------------------------------------------
# examples
if (BUILD_EXAMPLES)
  subproject_add_executable(_sample "sample" sample/sample.c)
  target_link_libraries(${_sample} ${_lib})
endif ()

