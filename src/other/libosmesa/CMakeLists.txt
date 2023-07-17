# Minimum required version of CMake
cmake_minimum_required(VERSION 3.18)

# Set CMake project name
project(OSMESA)

include(CheckCXXCompilerFlag)
check_cxx_compiler_flag(-O2 O2_COMPILER_FLAG)

find_package(Threads REQUIRED)
if (CMAKE_USE_PTHREADS_INIT)
  add_definitions(-DPTHREADS)
endif (CMAKE_USE_PTHREADS_INIT)

include(CheckLibraryExists)
check_library_exists(m cos "" HAVE_M_LIBRARY)
if (HAVE_M_LIBRARY)
  set(M_LIBRARY m)
endif (HAVE_M_LIBRARY)

# The point is to be separate from system OpenGL,
# so define the mangling headers to produce mgl_
# prefixing
add_definitions(-DUSE_MGL_NAMESPACE)

# The primary libosmesa library
add_subdirectory(src)

# For testing purposes, the build defines a number
# of (relatively) self-contained OpenGL examples.
# These are very much optional - in case a distribution
# does not wish to include them, allow for the examples
# directory to have been removed
if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/examples)
  option(OSMESA_BUILD_EXAMPLES "Build example programs" ON)
  if (OSMESA_BUILD_EXAMPLES)
    add_subdirectory(examples)
  endif (OSMESA_BUILD_EXAMPLES)
endif (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/examples)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
