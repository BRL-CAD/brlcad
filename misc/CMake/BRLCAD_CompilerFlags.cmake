#      B R L C A D _ C O M P I L E R F L A G S . C M A K E
# BRL-CAD
#
# Copyright (c) 2011-2012 United States Government as represented by
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
#
###

# -fast provokes a stack corruption in the shadow computations because
# of strict aliasing getting enabled.  we _require_
# -fno-strict-aliasing until someone changes how lists are managed.
# -fast-math results in non-IEEE floating point math among a handful
# of other optimizations that cause substantial error in ray tracing
# and tessellation (and probably more).

if(${BRLCAD_OPTIMIZED_BUILD} MATCHES "ON")
  if(CMAKE_BUILD_TYPE)
    set(opt_conf_list "ALL")
  endif(CMAKE_BUILD_TYPE)
  if(CMAKE_CONFIGURATION_TYPES)
    set(opt_conf_list "Release;RelWithDebInfo")
  endif(CMAKE_CONFIGURATION_TYPES)
endif(${BRLCAD_OPTIMIZED_BUILD} MATCHES "ON")
  BRLCAD_CHECK_C_FLAG(O3 "${opt_conf_list}")
  BRLCAD_CHECK_C_FLAG(fstrength-reduce "${opt_conf_list}")
  BRLCAD_CHECK_C_FLAG(fexpensive-optimizations "${opt_conf_list}")
  BRLCAD_CHECK_C_FLAG(finline-functions "${opt_conf_list}")
  BRLCAD_CHECK_C_FLAG("finline-limit=10000" "${opt_conf_list}")
  BRLCAD_CHECK_CXX_FLAG(O3 "${opt_conf_list}")
  BRLCAD_CHECK_CXX_FLAG(fstrength-reduce "${opt_conf_list}")
  BRLCAD_CHECK_CXX_FLAG(fexpensive-optimizations "${opt_conf_list}")
  BRLCAD_CHECK_CXX_FLAG(finline-functions "${opt_conf_list}")
  BRLCAD_CHECK_CXX_FLAG("finline-limit=10000" "${opt_conf_list}")
if(${BRLCAD_OPTIMIZED_BUILD} MATCHES "ON")
  if(NOT BRLCAD_ENABLE_PROFILING AND NOT BRLCAD_DEBUG_BUILD)
    BRLCAD_CHECK_C_FLAG(fomit-frame-pointer "Release")
    BRLCAD_CHECK_CXX_FLAG(fomit-frame-pointer "Release")
  else(NOT BRLCAD_ENABLE_PROFILING AND NOT BRLCAD_DEBUG_BUILD)
    BRLCAD_CHECK_C_FLAG(fno-omit-frame-pointer)
    BRLCAD_CHECK_CXX_FLAG(fno-omit-frame-pointer)
  endif(NOT BRLCAD_ENABLE_PROFILING AND NOT BRLCAD_DEBUG_BUILD)
endif(${BRLCAD_OPTIMIZED_BUILD} MATCHES "ON")
#need to strip out non-debug-compat flags after the fact based on build type, or do something else
#that will restore them if build type changes

# verbose warning flags
if(BRLCAD_ENABLE_COMPILER_WARNINGS OR BRLCAD_ENABLE_STRICT)
  # also of interest:
  # -Wunreachable-code -Wmissing-declarations -Wmissing-prototypes -Wstrict-prototypes -ansi
  # -Wformat=2 (after bu_fopen_uniq() is obsolete)
  BRLCAD_CHECK_C_FLAG(pedantic)
  BRLCAD_CHECK_CXX_FLAG(pedantic)
  # The Wall warnings are too verbose with Visual C++
  if(NOT MSVC)
    BRLCAD_CHECK_C_FLAG(Wall)
    BRLCAD_CHECK_CXX_FLAG(Wall)
  else(NOT MSVC)
    BRLCAD_CHECK_C_FLAG(W4)
    BRLCAD_CHECK_CXX_FLAG(W4)
  endif(NOT MSVC)
  BRLCAD_CHECK_C_FLAG(Wextra)
  BRLCAD_CHECK_C_FLAG(Wundef)
  BRLCAD_CHECK_C_FLAG(Wfloat-equal)
  BRLCAD_CHECK_C_FLAG(Wshadow)
  BRLCAD_CHECK_C_FLAG(Winline)
  BRLCAD_CHECK_CXX_FLAG(Wextra)
  BRLCAD_CHECK_CXX_FLAG(Wundef)
  BRLCAD_CHECK_CXX_FLAG(Wfloat-equal)
  BRLCAD_CHECK_CXX_FLAG(Wshadow)
  BRLCAD_CHECK_CXX_FLAG(Winline)
  # Need this for tcl.h
  BRLCAD_CHECK_C_FLAG(Wno-long-long) 
  BRLCAD_CHECK_CXX_FLAG(Wno-long-long) 
endif(BRLCAD_ENABLE_COMPILER_WARNINGS OR BRLCAD_ENABLE_STRICT)

if(BRLCAD_ENABLE_STRICT)
  BRLCAD_CHECK_C_FLAG(Werror)
endif(BRLCAD_ENABLE_STRICT)

# End detection of flags intended for BRL-CAD use.  Make sure all variables have
# their appropriate values written to the cache - otherwise, DiffCache will see
# differences and update the COUNT file.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}" CACHE STRING "C Compiler flags used by all targets" FORCE)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}" CACHE STRING "C++ Compiler flags used by all targets" FORCE)
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}" CACHE STRING "Linker flags used by all shared library targets" FORCE)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}" CACHE STRING "Linker flags used by all exe targets" FORCE)
if(CMAKE_BUILD_TYPE)
  string(TOUPPER "${CMAKE_BUILD_TYPE}" BUILD_TYPE_UPPER)
  set(CMAKE_C_FLAGS_${BUILD_TYPE_UPPER} "${CMAKE_C_FLAGS_${BUILD_TYPE_UPPER}}" CACHE STRING "C Compiler flags used for ${CMAKE_BUILD_TYPE} builds" FORCE)
  set(CMAKE_CXX_FLAGS_${BUILD_TYPE_UPPER} "${CMAKE_CXX_FLAGS_${BUILD_TYPE_UPPER}}" CACHE STRING "C++ Compiler flags used for ${CMAKE_BUILD_TYPE} builds" FORCE)
  set(CMAKE_SHARED_LINKER_FLAGS_${BUILD_TYPE_UPPER} "${CMAKE_SHARED_LINKER_FLAGS_${BUILD_TYPE_UPPER}}" CACHE STRING "Linker flags used for ${CMAKE_BUILD_TYPE} builds" FORCE)
  set(CMAKE_EXE_LINKER_FLAGS_${BUILD_TYPE_UPPER} "${CMAKE_EXE_LINKER_FLAGS_${BUILD_TYPE_UPPER}}" CACHE STRING "Exe linker flags used for ${CMAKE_BUILD_TYPE} builds" FORCE)
endif(CMAKE_BUILD_TYPE)

foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
  string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
  set(CMAKE_C_FLAGS_${CFG_TYPE_UPPER} "${CMAKE_C_FLAGS_${CFG_TYPE_UPPER}}" CACHE STRING "C Compiler flags used for ${CFG_TYPE} builds" FORCE)
  set(CMAKE_CXX_FLAGS_${CFG_TYPE_UPPER} "${CMAKE_CXX_FLAGS_${CFG_TYPE_UPPER}}" CACHE STRING "C++ Compiler flags used for ${CFG_TYPE} builds" FORCE)
  set(CMAKE_SHARED_LINKER_FLAGS_${CFG_TYPE_UPPER} "${CMAKE_SHARED_LINKER_FLAGS_${CFG_TYPE_UPPER}}" CACHE STRING "Linker flags used for ${CFG_TYPE} builds" FORCE)
  set(CMAKE_EXE_LINKER_FLAGS_${CFG_TYPE_UPPER} "${CMAKE_EXE_LINKER_FLAGS_${CFG_TYPE_UPPER}}" CACHE STRING "Exe linker flags used for ${CFG_TYPE} builds" FORCE)
endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
