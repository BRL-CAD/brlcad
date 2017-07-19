#               F I N D T E R M L I B . C M A K E
# BRL-CAD
#
# Copyright (c) 2011-2016 United States Government as represented by
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
# - Find termlib
# Find a curses or other terminal library
#
#  TERMLIB_LIBRARY       - terminal library.
#  TERMLIB_INCLUDE_DIR   - terminal header directory.
#  TERMLIB_FOUND         - library found.
#
#=============================================================================
include(CheckLibraryExists)
include(CheckIncludeFiles)
include(CheckCSourceRuns)

set(termlib_src "
#ifdef HAVE_TERMLIB_H
#  include <termlib.h>
#else
#  ifdef HAVE_NCURSES_H
#    include <ncurses.h>
#  else
#    ifdef HAVE_CURSES_H
#      include <curses.h>
#    else
#      ifdef HAVE_TERMCAP_H
#        include <termcap.h>
#      else
#        ifdef HAVE_TERMINFO_H
#          include <terminfo.h>
#        else
#          ifdef HAVE_TINFO_H
#            include <tinfo.h>
#          endif
#        endif
#      endif
#    endif
#  endif
#  ifdef HAVE_TERM_H
#    include <term.h>
#  endif
#endif
int main (void) {
   char buffer[2048] = {0};
   (void)tgetent(buffer, \"vt100\");
   return 0;
}
")

macro(TERMLIB_CHECK_LIBRARY lname func headers)
  if(NOT TERMLIB_LIBRARY OR "${TERMLIB_LIBRARY}" MATCHES "NOTFOUND")
    CHECK_LIBRARY_EXISTS(${lname} ${func} "" HAVE_TERMLIB_${lname})
    if(HAVE_TERMLIB_${lname})
      # Got lib, now sort through headers
      foreach(hdr ${headers})
	if(NOT TERMLIB_INCLUDE_DIR OR "${TERMLIB_INCLUDE_DIR}" MATCHES "NOTFOUND")
	  message("${lname}: ${hdr}")
	  string(TOUPPER "${hdr}" HDR)
	  string(REGEX REPLACE "[^A-Z0-9]" "_" HDR "${HDR}")
	  CHECK_INCLUDE_FILES(${hdr} HAVE_${HDR})
	  find_path(TERMLIB_INCLUDE_DIR "${hdr}")
	  if(NOT "${TERMLIB_INCLUDE_DIR}" MATCHES "NOTFOUND")
	    set(LIBTERM_RESULT 1)
	    set(TERMLIB_LIBRARY "${lname}")
	    file(WRITE "${CMAKE_BINARY_DIR}/CMakeTmp/termlib.c" "${termlib_src}")
	    if(LIBTERM_RESULT)
              try_run(LIBTERM_RESULT LIBTERM_COMPILE "${CMAKE_BINARY_DIR}/CMakeTmp"
		"${CMAKE_BINARY_DIR}/CMakeTmp/termlib.c"
		COMPILE_DEFINITIONS "-DHAVE_${HDR}"
		LINK_LIBRARIES "${lname}"
		COMPILE_OUTPUT_VARIABLE CTERM_OUT
		RUN_OUTPUT_VARIABLE RTERM_OUT)
	      message("CTERM: ${CTERM_OUT}")
	      message("RTERM: ${RTERM_OUT}")
	      message("LIBTERM_RESULT: ${LIBTERM_RESULT}")
	    endif(LIBTERM_RESULT)
	    #file(REMOVE "${CMAKE_BINARY_DIR}/CMakeTmp/termlib.c")
	    if(LIBTERM_RESULT)
	      set(TERMLIB_LIBRARY "NOTFOUND" CACHE STRING "TERMLIB" FORCE)
	      set(TERMLIB_INCLUDE_DIR "NOTFOUND"  CACHE STRING "TERMLIB" FORCE)
	    else(LIBTERM_RESULT)
	      set(TERMLIB_LIBRARY ${lname} CACHE STRING "TERMLIB" FORCE)
	      set(TERMLIB_INCLUDE_DIR "${TERMLIB_INCLUDE_DIR}"  CACHE STRING "TERMLIB" FORCE)
	    endif(LIBTERM_RESULT)
	  endif(NOT "${TERMLIB_INCLUDE_DIR}" MATCHES "NOTFOUND")
	endif(NOT TERMLIB_INCLUDE_DIR OR "${TERMLIB_INCLUDE_DIR}" MATCHES "NOTFOUND")
      endforeach(hdr ${headers})
    endif(HAVE_TERMLIB_${lname})
  endif(NOT TERMLIB_LIBRARY OR "${TERMLIB_LIBRARY}" MATCHES "NOTFOUND")
endmacro(TERMLIB_CHECK_LIBRARY lname func)

TERMLIB_CHECK_LIBRARY(termlib tputs "termlib.h")
TERMLIB_CHECK_LIBRARY(ncurses tputs "ncurses.h;curses.h;termcap.h;term.h")
TERMLIB_CHECK_LIBRARY(curses tputs "curses.h;termcap.h;term.h")
TERMLIB_CHECK_LIBRARY(termcap tputs "termcap.h;term.h")
TERMLIB_CHECK_LIBRARY(terminfo tputs "terminfo.h;term.h")
TERMLIB_CHECK_LIBRARY(tinfo tputs "tinfo.h;term.h")
TERMLIB_CHECK_LIBRARY(ccurses tputs "curses.h;termcap.h;term.h")

# handle the QUIETLY and REQUIRED arguments and set TERMLIB_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(TERMLIB DEFAULT_MSG TERMLIB_LIBRARY TERMLIB_INCLUDE_DIR)

# Hide details
mark_as_advanced(TERMLIB_LIBRARY)
mark_as_advanced(TERMLIB_INCLUDE_DIR)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
