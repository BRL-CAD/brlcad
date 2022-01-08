# Minimum required version of CMake
cmake_minimum_required(VERSION 3.12)

# Set CMake project name
project(OSMESA)

include(CheckCXXCompilerFlag)
check_cxx_compiler_flag(-O3 O3_COMPILER_FLAG)

# Tell CMake where to look for local CMake modules,
# if we have them
if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/CMake)
  list(APPEND CMAKE_MODULE_PATH "${OSMESA_SOURCE_DIR}/CMake")
  include(Path_Setup)
endif (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/CMake)

find_package(Threads REQUIRED)
if (CMAKE_USE_PTHREADS_INIT)
  add_definitions(-DHAVE_PTHREAD)
endif (CMAKE_USE_PTHREADS_INIT)

include(CheckLibraryExists)
check_library_exists(m cos "" HAVE_M_LIBRARY)
if (HAVE_M_LIBRARY)
  set(M_LIBRARY m)
endif (HAVE_M_LIBRARY)

include(CheckIncludeFiles)
check_include_files(endian.h HAVE_ENDIAN_H)
if (HAVE_ENDIAN_H)
  add_definitions(-DPTHREADS)
endif (HAVE_ENDIAN_H)

include(CheckFunctionExists)
# TODO - check for the following
add_definitions(-DHAVE_TIMESPEC_GET=1)

# The point is to be separate from system OpenGL,
# so define the mangling headers to produce mgl_
# prefixing
add_definitions(-DUSE_MGL_NAMESPACE)

# If we have a stripped down source tree, need to default
# to off for extras
if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/glu)
  set(GLU_OPT_DEFAULT ON)
else()
  set(GLU_OPT_DEFAULT OFF)
endif()
if (GLU_OPT_DEFAULT AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/examples)
  set(EX_OPT_DEFAULT ON)
else()
  set(EX_OPT_DEFAULT OFF)
endif()

# Optional component settings
option(OSMESA_BUNDLE_GLU "Incorporate GLU into library" ${GLU_OPT_DEFAULT})
option(OSMESA_BUILD_EXAMPLES "Build example programs" ${EX_OPT_DEFAULT})

add_subdirectory(src)

if (OSMESA_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif (OSMESA_BUILD_EXAMPLES)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
