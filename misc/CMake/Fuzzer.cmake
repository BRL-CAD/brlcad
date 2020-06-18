#                    F U Z Z E R . C M A K E
# BRL-CAD
#
# Copyright (c) 2020 United States Government as represented by
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

include(CheckCXXCompilerFlag)


# check whether fuzzing support is available.  sets HAVE_FUZZER.
function(brlcad_check_fuzzer)
  set(CMAKE_REQUIRED_FLAGS -fsanitize=fuzzer)
  check_cxx_compiler_flag(-fsanitize=fuzzer HAVE_FUZZER)
  set(HAVE_FUZZER ${HAVE_FUZZER} PARENT_SCOPE)
endfunction(brlcad_check_fuzzer)


# add fuzzing flags to the compile and link flags of a given target.
function(brlcad_add_fuzzer target)
  brlcad_check_fuzzer()
  if (NOT ${HAVE_FUZZER})
    message(SEND_ERROR "Attempting to build fuzz targets with an unsupported compiler (expecting clang)")
  endif (NOT ${HAVE_FUZZER})

  target_compile_options(${target} PRIVATE -fsanitize=fuzzer)
  target_link_options(${target} PRIVATE -fsanitize=fuzzer)
endfunction(brlcad_add_fuzzer)


# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
