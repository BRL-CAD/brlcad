# - Find termlib 
# Find a curses or other terminal library
#
#  TERMLIB_INCLUDE_DIRS  - where to find curses or terminal header.
#  TERMLIB_LIBRARY       - List of libraries when using curses or terminal library.
#  TERMLIB_FOUND         - True if an appropriate library found.
#
#=============================================================================

SET(TERMLIB_TEST_TEMPLATE "
includeline1
includeline2
int main () {
    char buffer[2048] = {0}\;
	 int result = tgetent(buffer, "vt100")\;
	 return 0\;
}
")


SET(TERMHEADER_SEARCH_LIST "termlib.h;ncurses.h;curses.h;termcap.h;terminfo.h;tinfo.h")
FIND_PATH(TERM_INCLUDE_DIRS term.h)
FOREACH(tname ${TERMHEADER_SEARCH_LIST})
	IF(NOT TERMLIB_INCLUDE_DIRS)
		FIND_PATH(TERMLIB_INCLUDE_DIRS ${tname})
		MESSAGE("TERMLIB_INCLUDE_DIRS: ${TERMLIB_INCLUDE_DIRS}")
		IF(TERMLIB_INCLUDE_DIRS)
			FIND_PATH(TERM_INCLUDE_DIRS term.h)
			#STRING(REGEX REPLACE "\n" "ENDLINE" TERMLIB_TEST_SRC "${TERMLIB_TEST_TEMPLATE}")
			STRING(REGEX REPLACE "includeline1" "#include <${tname}>" TERMLIB_TEST_SRC "${TERMLIB_TEST_TEMPLATE}")
		   #termlib.h is a special case - don't need term.h if termlib.h is present
			IF(NOT ${tname} MATCHES "termlib.h" AND TERM_INCLUDE_DIRS)
				STRING(REGEX REPLACE "includeline2" "#include <term.h>" TERMLIB_TEST_SRC "${TERMLIB_TEST_SRC}")
			ELSE(NOT ${tname} MATCHES "termlib.h" AND TERM_INCLUDE_DIRS)
				STRING(REGEX REPLACE "includeline2" "" TERMLIB_TEST_SRC "${TERMLIB_TEST_SRC}")
				SET(TERM_INCLUDE_DIRS "")
			ENDIF(NOT ${tname} MATCHES "termlib.h" AND TERM_INCLUDE_DIRS)
			#STRING(REGEX REPLACE "ENDLINE" "\n" TERMLIB_TEST_SRC "${TERMLIB_TEST_SRC}")
			FILE(WRITE "${CMAKE_BINARY_DIR}/CMakeTmp/termlib_src.c" ${TERMLIB_TEST_SRC})
			TRY_RUN(TERMLIB_EXITCODE TERMLIB_COMPILED
				${CMAKE_BINARY_DIR}
				${CMAKE_BINARY_DIR}/CMakeTmp/termlib_src.c
				CMAKE_FLAGS "-I${TERMLIB_INCLUDE_DIRS} -I${TERM_INCLUDE_DIRS}"
				OUTPUT_VARIABLE OUTPUT)
			#IF(TERMLIB_EXITCODE OR TERMLIB_COMPILED) 
			MESSAGE("TERMLIB_EXITCODE: ${TERMLIB_EXITCODE}")
			MESSAGE("TERMLIB_COMPILED: ${TERMLIB_COMPILED}")
			MESSAGE("TERMLIB_OUTPUT: ${OUTPUT}")
			#ENDIF(TERMLIB_EXITCODE OR TERMLIB_COMPILED) 
		ENDIF(TERMLIB_INCLUDE_DIRS)
	ENDIF(NOT TERMLIB_INCLUDE_DIRS)
ENDFOREACH(tname ${TERMHEADER_SEARCH_LIST})

MARK_AS_ADVANCED(TERMLIB_LIBRARY TERMLIB_INCLUDE_DIRS)

# handle the QUIETLY and REQUIRED arguments and set TERMLIB_FOUND to TRUE if 
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(TERMLIB DEFAULT_MSG TERMLIB_INCLUDE_DIR TERMLIB_LIBRARY)

