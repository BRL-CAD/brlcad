#            B R L C A D _ A P I _ F L A G . C M A K E
# BRL-CAD
#
# Copyright (c) 2022 United States Government as represented by
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

include(CheckCXXSourceRuns)
include(CMakePushCheckState)


###
# make sure a given API standards-compliance flag seems to work.
#
# Flag to be tested is AFLAG, resulting variable to append it to from
# the parent scope is AFLAGS.
#
# Example: BRLCAD_API_FLAG("-D_POSIX_SOURCES" API_FLAGS)
###
function(BRLCAD_API_FLAG AFLAG AFLAGS)
  set(CHECK_API_FLAG_SRC "
#include <iostream>
int main(int ac, char *av[])
{
  if (ac > 0 && av)
    std::cout << \"hello\";
  return 0;
}
")
    string(TOUPPER "${AFLAG}" UAFLAG)
    string(REPLACE "-" "_" AFLAGVAR "${UAFLAG}")
    string(REPLACE "=" "_" AFLAGVAR "${AFLAGVAR}")
    cmake_push_check_state()
    set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} ${C_STANDARD_FLAGS} ${AFLAG}")
    check_cxx_source_runs("${CHECK_API_FLAG_SRC}" WORKING_${AFLAGVAR})
    cmake_pop_check_state()
    if(WORKING_${AFLAGVAR})
      set(${AFLAGS} "${${AFLAGS}} ${AFLAG}" PARENT_SCOPE)
    endif(WORKING_${AFLAGVAR})
endfunction(BRLCAD_API_FLAG)


# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

