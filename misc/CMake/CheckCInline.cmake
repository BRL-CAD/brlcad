#              C H E C K C I N L I N E . C M A K E
# BRL-CAD
#
# Copyright (c) 2012 United States Government as represented by
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
#
# This file provides a CHECK_C_INLINE macro that detects what keyword,
# if any, is used for inlining C functions.  This check is similar to
# autoconf's AC_C_INLINE macro.  The macro tests the inline keyword
# (c99), then __inline__ (c89), and then __inline.  When it finds one
# that works, it will ADD_DEFINITIONS(-Dinline=${KEYWORD}) and if none
# work, it will ADD_DEFINITIONS(-Dinline=).
#
# This implementation is based on a snippet from Jack Kelly on the
# cmake email list in Sep 2007, firther inspired by autoconf's c.m4.
#
# TODO: Don't just set global definitions, let the caller pass a
# variable.
#
###

include(CheckCSourceCompiles)

macro(CHECK_C_INLINE)

  if(NOT MSVC)

    foreach(KEYWORD "inline" "__inline__" "__inline")
      if(NOT DEFINED C_INLINE)

	string(TOUPPER "HAVE_${KEYWORD}" HAVE_INLINE)

	check_c_source_compiles(
	  "typedef int foo_t; static inline foo_t static_foo() {return 0;}
        foo_t foo() {return 0;} int main(int argc, char *argv[]) {return 0;}"
          ${HAVE_INLINE})

	if(${HAVE_INLINE})
	  set(C_INLINE TRUE)
	  if(NOT ${KEYWORD} MATCHES "inline")
	    # set inline to something else
	    add_definitions("-Dinline=${KEYWORD}")
	  endif(NOT ${KEYWORD} MATCHES "inline")
	endif(${HAVE_INLINE})

      endif(NOT DEFINED C_INLINE)
    endforeach(KEYWORD)

    if(NOT DEFINED C_INLINE)
      # unset inline if none were found
      add_definitions("-Dinline=")
    endif(NOT DEFINED C_INLINE)

  endif(NOT MSVC)

endmacro(CHECK_C_INLINE)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
