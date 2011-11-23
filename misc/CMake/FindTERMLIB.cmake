#               F I N D T E R M L I B . C M A K E
# BRL-CAD
#
# Copyright (c) 2011 United States Government as represented by
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
INCLUDE(ResolveCompilerPaths)
MACRO(TERMLIB_CHECK_LIBRARY targetname lname func)
	IF(NOT ${targetname}_LIBRARY)
		CHECK_LIBRARY_EXISTS(${lname} ${func} "" HAVE_${targetname}_${lname})
		IF(HAVE_${targetname}_${lname})
			FIND_PATH(TERMLIB_INCLUDE_DIR ${lname}.h)
			RESOLVE_LIBRARIES (${targetname}_LIBRARY "-l${lname}")
			SET(${targetname}_LINKOPT "-l${lname}" CACHE STRING "${targetname} link option")
			MARK_AS_ADVANCED(${targetname}_LINKOPT)
		ENDIF(HAVE_${targetname}_${lname})
	ENDIF(NOT ${targetname}_LIBRARY)
ENDMACRO(TERMLIB_CHECK_LIBRARY lname func)

TERMLIB_CHECK_LIBRARY(TERMLIB termlib tputs)
TERMLIB_CHECK_LIBRARY(TERMLIB ncurses tputs)
TERMLIB_CHECK_LIBRARY(TERMLIB curses tputs)
TERMLIB_CHECK_LIBRARY(TERMLIB termcap tputs)
TERMLIB_CHECK_LIBRARY(TERMLIB terminfo tputs)
TERMLIB_CHECK_LIBRARY(TERMLIB tinfo tputs)
TERMLIB_CHECK_LIBRARY(TERMLIB ccurses tputs)
include(CheckCFileRuns)
configure_file(${BRLCAD_CMAKE_DIR}/test_srcs/termlib.c.in ${CMAKE_BINARY_DIR}/CMakeTmp/termlib.c)
SET(CMAKE_REQUIRED_LIBRARIES_BAK ${CMAKE_REQUIRED_LIBRARIES})
SET(CMAKE_REQUIRED_LIBRARIES ${TERMLIB_LIBRARY})
CHECK_C_FILE_RUNS(${CMAKE_BINARY_DIR}/CMakeTmp/termlib.c LIBTERM_RESULT)
IF(NOT LIBTERM_RESULT)
	SET(TERMLIB_LIBRARY "NOTFOUND" CACHE STRING "TERMLIB" FORCE)
ELSE(NOT LIBTERM_RESULT)
	SET(TERMLIB_LIBRARY ${TERMLIB_LIBRARY} CACHE STRING "TERMLIB" FORCE)
ENDIF(NOT LIBTERM_RESULT)
MARK_AS_ADVANCED(TERMLIB_LIBRARY)
SET(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES_BAK})
# handle the QUIETLY and REQUIRED arguments and set TERMLIB_FOUND to TRUE if 
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(TERMLIB DEFAULT_MSG TERMLIB_LIBRARY TERMLIB_INCLUDE_DIR)


# Local Variables:
# tab-width: 8
# mode: sh
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
