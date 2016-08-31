# Copyright (c) 2010-2016 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following
# disclaimer in the documentation and/or other materials provided
# with the distribution.
#
# 3. The name of the author may not be used to endorse or promote
# products derived from this software without specific prior written
# permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
# OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# *******************************************************************
# ***             Screened Poisson CMakeLists.txt                 ***
# *******************************************************************

# Minimum required version of CMake
cmake_minimum_required(VERSION 2.8.4)
if(COMMAND CMAKE_POLICY)
  # libraries linked via full path don't introduce linker search paths
  CMAKE_POLICY(SET CMP0003 NEW)

  # list command doesn't ignore empty elements
  CMAKE_POLICY(SET CMP0007 NEW)

  # prefer files in CMAKE_MODULE_PATH over the cmake module dir
  CMAKE_POLICY(SET CMP0017 OLD)

  # allow relative paths in INCLUDE_DIRECTORIES
  if ("${CMAKE_VERSION}" VERSION_EQUAL 2.8.12 OR "${CMAKE_VERSION}" VERSION_GREATER 2.8.12)
    CMAKE_POLICY(SET CMP0021 OLD)
  endif ("${CMAKE_VERSION}" VERSION_EQUAL 2.8.12 OR "${CMAKE_VERSION}" VERSION_GREATER 2.8.12)

  if ("${CMAKE_VERSION}" VERSION_GREATER 2.99)
    CMAKE_POLICY(SET CMP0026 OLD)
    CMAKE_POLICY(SET CMP0042 OLD)
  endif ("${CMAKE_VERSION}" VERSION_GREATER 2.99)

  if ("${CMAKE_VERSION}" VERSION_GREATER 3.0.9)
    CMAKE_POLICY(SET CMP0046 NEW)
    CMAKE_POLICY(SET CMP0053 OLD)
    CMAKE_POLICY(SET CMP0054 NEW)
  endif ("${CMAKE_VERSION}" VERSION_GREATER 3.0.9)
endif(COMMAND CMAKE_POLICY)

# set CMake project name
# (Screened Poisson Reconstruction - SPR)
project(SPR)

if(SPR_ENABLE_OPENMP)
  find_package(OpenMP)
  set(CMAKE_C_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_C_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
endif(SPR_ENABLE_OPENMP)

set(libSPR_srcs
  Src/Factor.cpp
  Src/Geometry.cpp
  Src/MarchingCubes.cpp
  Src/SPR.cpp
  )

if(MSVC)
  add_definitions("-DSPR_DLL_EXPORTS")
endif(MSVC)

IF(BUILD_SHARED_LIBS)
  add_library(libSPR SHARED ${libSPR_srcs})
  install(TARGETS libSPR
    RUNTIME DESTINATION ${BIN_DIR}
    LIBRARY DESTINATION ${LIB_DIR}
    ARCHIVE DESTINATION ${LIB_DIR})
ENDIF(BUILD_SHARED_LIBS)

IF(BUILD_STATIC_LIBS)
  add_library(libSPR-static STATIC ${libSPR_srcs})
  IF(NOT WIN32)
    SET_TARGET_PROPERTIES(libSPR-static PROPERTIES OUTPUT_NAME "libSPR")
  ENDIF(NOT WIN32)
  install(TARGETS libSPR-static
    RUNTIME DESTINATION ${BIN_DIR}
    LIBRARY DESTINATION ${LIB_DIR}
    ARCHIVE DESTINATION ${LIB_DIR})
ENDIF(BUILD_STATIC_LIBS)

if(NOT DEFINED SPR_ENABLE_EXAMPLES)
  set(SPR_ENABLE_EXAMPLES ON)
endif(NOT DEFINED SPR_ENABLE_EXAMPLES)

if(SPR_ENABLE_EXAMPLES)

  add_executable(PoissonRecon
    Src/CmdLineParser.cpp Src/PlyFile.cpp Src/PoissonRecon.cpp
    Src/Factor.cpp  Src/Geometry.cpp  Src/MarchingCubes.cpp
    )

#  add_executable(PoissonRecon
#    Src/CmdLineParser.cpp Src/PlyFile.cpp Src/PoissonRecon.cpp
#    Src/Factor.cpp  Src/Geometry.cpp  Src/MarchingCubes.cpp Src/SPR.cpp
#    )

  add_executable(SurfaceTrimmer
    Src/CmdLineParser.cpp Src/PlyFile.cpp Src/SurfaceTrimmer.cpp
    Src/Factor.cpp  Src/Geometry.cpp  Src/MarchingCubes.cpp
    )

endif(SPR_ENABLE_EXAMPLES)

#CFLAGS_DEBUG = -DDEBUG -g3

#CFLAGS_RELEASE = -O3 -DRELEASE -funroll-loops -ffast-math

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

