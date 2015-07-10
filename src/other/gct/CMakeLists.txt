# Minimum required version of CMake
CMAKE_MINIMUM_REQUIRED(VERSION 2.8.3)
if(COMMAND CMAKE_POLICY)
  CMAKE_POLICY(SET CMP0003 NEW)
endif(COMMAND CMAKE_POLICY)

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
add_subdirectory(BallPivoting)
add_subdirectory(MeshDecimation)
add_subdirectory(psmain)
add_subdirectory(bpmain)
add_subdirectory(dcmain)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
