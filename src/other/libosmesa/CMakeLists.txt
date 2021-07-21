# Minimum required version of CMake
cmake_minimum_required(VERSION 3.12)

# Set CMake project name
project(OSMESA)

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

include(CheckCXXCompilerFlag)
check_cxx_compiler_flag(-O3 O3_COMPILER_FLAG)

# Don't want to conflict with system OpenGL, so enable the
# mangling headers to produce mgl prefixing
add_definitions(-DUSE_MGL_NAMESPACE)

add_subdirectory(src)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
