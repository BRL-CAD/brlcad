# Minimum required version of CMake
cmake_minimum_required(VERSION 3.1.3)

# set CMake project name
project(GCT)

# The location in which to install executables.
if(NOT BIN_DIR)
  set(BIN_DIR bin)
endif(NOT BIN_DIR)

# The location in which to install libraries.
if(NOT LIB_DIR)
  if(NOT WIN32)
    set(LIB_DIR lib)
  else(NOT WIN32)
    set(LIB_DIR bin)
  endif(NOT WIN32)
endif(NOT LIB_DIR)

find_package(Threads)

add_subdirectory(Auxiliary)
add_subdirectory(BRLCAD)
add_subdirectory(Sampler)
add_subdirectory(MeshDecimation)
add_subdirectory(psmain)
add_subdirectory(dcmain)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
