#                 F I N D S T L . C M A K E
# BRL-CAD
#
# Copyright (c) 2011-2014 United States Government as represented by
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
# In some situations, it is necessary to explicitly link the C++ STL
# library.  Traditional find_library searches won't work in all cases,
# so an alternative approach is required.  By default stdc++ is searched
# for, but this can be changed by setting the STDCXX_LIBRARIES variable.
# If already set, this routine will test the contents of STDCXX_LIBRARIES.
# STDCXX_LIBRARIES will be set to the empty string if the test fails.
#
#  STDCXX_LIBRARIES - Contains linker args for using STL library.
#
#=============================================================================

set(stl_test_src "
int
main(int ac, char *av[])
{
  if (av)
    return 0;
  else
    return ac-ac;
}
")

set(CMAKE_REQUIRED_LIBRARIES_BAK "${CMAKE_REQUIRED_LIBRARIES}")

# first try with no library
set(CMAKE_REQUIRED_LIBRARIES "")
CHECK_C_SOURCE_RUNS("${stl_test_src}" STL_LIB_TEST)

if("${STL_LIB_TEST}" EQUAL 1)
  # succeeded - no STL needed
  set(STDCXX_LIBRARIES "" CACHE STRING "STL not required" FORCE)
else("${STL_LIB_TEST}" EQUAL 1)
  # failed - try library
  if(NOT STDCXX_LIBRARIES)
    set(STDCXX_LIBRARIES "-lstdc++")
  endif(NOT STDCXX_LIBRARIES)

  set(CMAKE_REQUIRED_LIBRARIES "${STDCXX_LIBRARIES}")
  CHECK_C_SOURCE_RUNS("${stl_test_src}" STL_LIB_TEST)

  if("${STL_LIB_TEST}" EQUAL 1)
    set(STDCXX_LIBRARIES "${STDCXX_LIBRARIES}" CACHE STRING "STL found" FORCE)
  else()
    set(STDCXX_LIBRARIES "" CACHE STRING "STL not found" FORCE)
  endif()

  set(CMAKE_REQUIRED_LIBRARIES "${CMAKE_REQUIRED_LIBRARIES_BAK}")

  # handle the QUIETLY and REQUIRED arguments
  include(FindPackageHandleStandardArgs)
  FIND_PACKAGE_HANDLE_STANDARD_ARGS(STL DEFAULT_MSG STDCXX_LIBRARIES)
endif("${STL_LIB_TEST}" EQUAL 1)
mark_as_advanced(STDCXX_LIBRARIES)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
