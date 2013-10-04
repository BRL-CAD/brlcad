#      B R L C A D _ C O M P I L E R F L A G S . C M A K E
# BRL-CAD
#
# Copyright (c) 2011-2013 United States Government as represented by
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
CHECK_C_FLAG(O3 GROUPS OPTIMIZE_C_FLAGS)
CHECK_C_FLAG(fstrength-reduce GROUPS OPTIMIZE_C_FLAGS)
CHECK_C_FLAG(fexpensive-optimizations GROUPS OPTIMIZE_C_FLAGS)
CHECK_C_FLAG(finline-functions GROUPS OPTIMIZE_C_FLAGS)
CHECK_C_FLAG("finline-limit=10000 --param inline-unit-growth=300 --param large-function-growth=300" GROUPS OPTIMIZE_C_FLAGS)
CHECK_CXX_FLAG(O3 GROUPS OPTIMIZE_CXX_FLAGS)
CHECK_CXX_FLAG(fstrength-reduce GROUPS OPTIMIZE_CXX_FLAGS)
CHECK_CXX_FLAG(fexpensive-optimizations GROUPS OPTIMIZE_CXX_FLAGS)
CHECK_CXX_FLAG(finline-functions GROUPS OPTIMIZE_CXX_FLAGS)
CHECK_CXX_FLAG("finline-limit=10000 --param inline-unit-growth=300 --param large-function-growth=300" GROUPS OPTIMIZE_CXX_FLAGS)
if(${BRLCAD_OPTIMIZED_BUILD} MATCHES "ON")
  if(NOT BRLCAD_ENABLE_PROFILING AND NOT BRLCAD_FLAGS_DEBUG)
    CHECK_C_FLAG(fomit-frame-pointer GROUPS OPTIMIZE_C_FLAGS)
    CHECK_CXX_FLAG(fomit-frame-pointer GROUPS OPTIMIZE_CXX_FLAGS)
  else(NOT BRLCAD_ENABLE_PROFILING AND NOT BRLCAD_FLAGS_DEBUG)
    CHECK_C_FLAG(fno-omit-frame-pointer GROUPS OPTIMIZE_C_FLAGS)
    CHECK_CXX_FLAG(fno-omit-frame-pointer GROUPS OPTIMIZE_CXX_FLAGS)
  endif(NOT BRLCAD_ENABLE_PROFILING AND NOT BRLCAD_FLAGS_DEBUG)
endif(${BRLCAD_OPTIMIZED_BUILD} MATCHES "ON")

if(${BRLCAD_FLAGS_OPTIMIZATION} MATCHES "AUTO")
  if(CMAKE_CONFIGURATION_TYPES)
    set(opt_conf_list "Release")
  else(CMAKE_CONFIGURATION_TYPES)
    if(${BRLCAD_OPTIMIZED_BUILD} MATCHES "ON")
      set(opt_conf_list "ALL")
    endif(${BRLCAD_OPTIMIZED_BUILD} MATCHES "ON")
  endif(CMAKE_CONFIGURATION_TYPES)
else(${BRLCAD_FLAGS_OPTIMIZATION} MATCHES "AUTO")
  if(${BRLCAD_OPTIMIZED_BUILD} MATCHES "ON")
    set(opt_conf_list "ALL")
  endif(${BRLCAD_OPTIMIZED_BUILD} MATCHES "ON")
endif(${BRLCAD_FLAGS_OPTIMIZATION} MATCHES "AUTO")
if(opt_conf_list)
  ADD_NEW_FLAG(C OPTIMIZE_C_FLAGS "${opt_conf_list}")
  ADD_NEW_FLAG(CXX OPTIMIZE_CXX_FLAGS "${opt_conf_list}")
endif(opt_conf_list)

# enable stack protection for unoptimized debug builds.  this is
# intended to help make it easier to identify problematic code but
# only when compiling unoptimized (because the extra barrier checks
# can affect the memory footprint and runtime performance.
if(${BRLCAD_OPTIMIZED_BUILD} MATCHES "OFF" AND BRLCAD_FLAGS_DEBUG)
  CHECK_C_FLAG(fstack-protector-all)
  CHECK_CXX_FLAG(fstack-protector-all)
  # checking both in case compiling c/c++ with different compilers
  CHECK_C_FLAG(qstackprotect)
  CHECK_CXX_FLAG(qstackprotect)
endif(${BRLCAD_OPTIMIZED_BUILD} MATCHES "OFF" AND BRLCAD_FLAGS_DEBUG)

# verbose warning flags.  we intentionally try to turn on as many as
# possible.  adding more is encouraged (as long as all issues are
# fixed first).
if(BRLCAD_ENABLE_COMPILER_WARNINGS OR BRLCAD_ENABLE_STRICT)
  # also of interest:
  # -Wunreachable-code -Wmissing-declarations -Wmissing-prototypes -Wstrict-prototypes -ansi
  # -Wformat=2 (after bu_fopen_uniq() is obsolete) -Wdocumentation (for Doxygen comments)
  CHECK_C_FLAG(pedantic)
  CHECK_CXX_FLAG(pedantic)

  # FIXME: The Wall warnings are too verbose with Visual C++ (for
  # now).  we have a lot to clean up.
  if(NOT MSVC)
    CHECK_C_FLAG(Wall)
    CHECK_CXX_FLAG(Wall)
  else(NOT MSVC)
    CHECK_C_FLAG(W4)
    CHECK_CXX_FLAG(W4)
  endif(NOT MSVC)

  CHECK_C_FLAG(Wextra)
  CHECK_CXX_FLAG(Wextra)

  CHECK_C_FLAG(Wundef)
  CHECK_CXX_FLAG(Wundef)

  CHECK_C_FLAG(Wfloat-equal)
  CHECK_CXX_FLAG(Wfloat-equal)

  CHECK_C_FLAG(Wshadow)
  CHECK_CXX_FLAG(Wshadow)

# report where we throw away const
#  CHECK_C_FLAG(Wcast-qual)
#  CHECK_CXX_FLAG(Wcast-qual)

# check for redundant declarations
#  CHECK_C_FLAG(Wredundant-decls)
#  CHECK_CXX_FLAG(Wredundant-decls)

  # want C inline warnings, but versions of g++ (circa 4.7) spew
  # unquellable bogus warnings on default constructors that we don't
  # have access to (e.g., in opennurbs and boost), so turn them off
  CHECK_C_FLAG(Winline)
  CHECK_CXX_FLAG(Wno-inline)

  # Need this for tcl.h
  CHECK_C_FLAG(Wno-long-long)
  CHECK_CXX_FLAG(Wno-long-long)

  # Need this for X11 headers using variadic macros
  CHECK_C_FLAG(Wno-variadic-macros)
  CHECK_CXX_FLAG(Wno-variadic-macros)
endif(BRLCAD_ENABLE_COMPILER_WARNINGS OR BRLCAD_ENABLE_STRICT)

if(BRLCAD_ENABLE_STRICT)
  CHECK_C_FLAG(Werror)
  CHECK_CXX_FLAG(Werror)
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
