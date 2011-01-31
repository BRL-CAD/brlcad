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
SET(CMAKE_REQUIRED_LIBRARIES ${TERMLIB_LIBRARY})
CHECK_C_FILE_RUNS(${CMAKE_BINARY_DIR}/CMakeTmp/termlib.c LIBTERM_RESULT)
IF(NOT LIBTERM_RESULT)
	SET(TERMLIB_LIBRARY "NOTFOUND" CACHE STRING "TERMLIB" FORCE)
ELSE(NOT LIBTERM_RESULT)
	SET(TERMLIB_LIBRARY ${TERMLIB_LIBRARY} CACHE STRING "TERMLIB" FORCE)
ENDIF(NOT LIBTERM_RESULT)
MARK_AS_ADVANCED(TERMLIB_LIBRARY)

# handle the QUIETLY and REQUIRED arguments and set TERMLIB_FOUND to TRUE if 
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(TERMLIB DEFAULT_MSG TERMLIB_LIBRARY TERMLIB_INCLUDE_DIR)

