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

macro(TERMLIB_CHECK_LIBRARY lname func headers)
  if(NOT TERMLIB_LIBRARY OR "${TERMLIB_LIBRARY}" MATCHES "NOTFOUND")
    CHECK_LIBRARY_EXISTS(${lname} ${func} "" HAVE_TERMLIB_${lname})
    if(HAVE_TERMLIB_${lname})
      # Got lib, now sort through headers
      foreach(hdr ${headers})
	string(TOUPPER "${hdr}" HDR)
	string(REGEX REPLACE "[^A-Z0-9]" "_" HDR "${HDR}")
	CHECK_INCLUDE_FILES(${hdr} HAVE_${HDR})
	if(NOT TERMLIB_INCLUDE_DIR OR "${TERMLIB_INCLUDE_DIR}" MATCHES "NOTFOUND")
	  find_path(TERMLIB_INCLUDE_DIR "${hdr}")
	  if(NOT "${TERMLIB_INCLUDE_DIR}" MATCHES "NOTFOUND")
	    set(LIBTERM_RESULT)
	    set(TERMLIB_LIBRARY "${lname}")
	    configure_file("${BRLCAD_CMAKE_DIR}/test_srcs/termlib.c.in" "${CMAKE_BINARY_DIR}/CMakeTmp/termlib.c")
	    set(CMAKE_REQUIRED_LIBRARIES_BAK ${CMAKE_REQUIRED_LIBRARIES})
	    set(CMAKE_REQUIRED_LIBRARIES ${TERMLIB_LIBRARY})
	    if(NOT DEFINED LIBTERM_RESULT)
	      CHECK_C_SOURCE_RUNS("${CMAKE_BINARY_DIR}/CMakeTmp/termlib.c" LIBTERM_RESULT)
	    endif(NOT DEFINED LIBTERM_RESULT)
	    if(NOT LIBTERM_RESULT)
	      set(TERMLIB_LIBRARY "NOTFOUND" CACHE STRING "TERMLIB" FORCE)
	    else(NOT LIBTERM_RESULT)
	      set(TERMLIB_LIBRARY ${TERMLIB_LIBRARY} CACHE STRING "TERMLIB" FORCE)
	    endif(NOT LIBTERM_RESULT)
	    set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES_BAK})
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
