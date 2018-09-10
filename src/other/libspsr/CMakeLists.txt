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
cmake_minimum_required(VERSION 3.1.3)

# set CMake project name
# (Screened Poisson Surface Reconstruction - SPSR)
project(SPSR)

if(SPSR_ENABLE_OPENMP)
  find_package(OpenMP)
  set(CMAKE_C_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_C_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
endif(SPSR_ENABLE_OPENMP)

set(libSPSR_srcs
  Src/Factor.cpp
  Src/Geometry.cpp
  Src/MarchingCubes.cpp
  Src/SPSR.cpp
  )

if(MSVC)
  add_definitions("-DSPSR_DLL_EXPORTS")
endif(MSVC)

IF(BUILD_SHARED_LIBS)
  add_library(libSPSR SHARED ${libSPSR_srcs})
  install(TARGETS libSPSR
    RUNTIME DESTINATION ${BIN_DIR}
    LIBRARY DESTINATION ${LIB_DIR}
    ARCHIVE DESTINATION ${LIB_DIR})
ENDIF(BUILD_SHARED_LIBS)

IF(BUILD_STATIC_LIBS)
  add_library(libSPSR-static STATIC ${libSPSR_srcs})
  IF(NOT WIN32)
    SET_TARGET_PROPERTIES(libSPSR-static PROPERTIES OUTPUT_NAME "libSPSR")
  ENDIF(NOT WIN32)
  install(TARGETS libSPSR-static
    RUNTIME DESTINATION ${BIN_DIR}
    LIBRARY DESTINATION ${LIB_DIR}
    ARCHIVE DESTINATION ${LIB_DIR})
ENDIF(BUILD_STATIC_LIBS)

if(NOT DEFINED SPSR_ENABLE_EXAMPLES)
  set(SPSR_ENABLE_EXAMPLES OFF)
endif(NOT DEFINED SPSR_ENABLE_EXAMPLES)

if(SPSR_ENABLE_EXAMPLES)

  add_executable(PoissonRecon
    Src/CmdLineParser.cpp Src/PlyFile.cpp Src/PoissonRecon.cpp
    Src/Factor.cpp  Src/Geometry.cpp  Src/MarchingCubes.cpp
    )

  add_executable(SurfaceTrimmer
    Src/CmdLineParser.cpp Src/PlyFile.cpp Src/SurfaceTrimmer.cpp
    Src/Factor.cpp  Src/Geometry.cpp  Src/MarchingCubes.cpp
    )

endif(SPSR_ENABLE_EXAMPLES)

#CFLAGS_DEBUG = -DDEBUG -g3

#CFLAGS_RELEASE = -O3 -DRELEASE -funroll-loops -ffast-math

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

