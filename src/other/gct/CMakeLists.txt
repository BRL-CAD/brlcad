# Minimum required version of CMake
cmake_minimum_required(VERSION 3.12)

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

CHECK_INCLUDE_FILE(pthread.h PROBE_PTHREAD_H)
if (NOT PROBE_PTHREAD_H)
  cmake_push_check_state(RESET)
  # pthread.h on FreeBSD 10 and some older Linucies use non-c90 types
  set(CMAKE_REQUIRED_DEFINITIONS "-Dclockid_t=clock_t")
  set(CMAKE_REQUIRED_FLAGS "-pthread")
  CHECK_INCLUDE_FILE(pthread.h PROBE_PTHREAD_H_CLOCKID_T)
  if (PROBE_PTHREAD_H_CLOCKID_T)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Dclockid_t=clock_t -pthread")
  endif (PROBE_PTHREAD_H_CLOCKID_T)
endif (NOT PROBE_PTHREAD_H)

set(THREADS_PREFER_PTHREAD_FLAG TRUE)
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
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
