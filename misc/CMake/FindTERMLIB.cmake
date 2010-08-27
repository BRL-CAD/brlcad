# - Find termlib 
# Find a curses or other terminal library
#
#  TERMLIB_INCLUDE_DIR  - where to find curses or terminal header.
#  TERMLIB_LIBRARY       - List of libraries when using curses or terminal library.
#  TERMLIB_FOUND         - True if an appropriate library found.
#
#=============================================================================

INCLUDE(CheckLibraryExists)

SET(TERMHEADER_SEARCH_LIST "termlib;ncurses;curses;termcap;terminfo;tinfo")
FOREACH(tname ${TERMHEADER_SEARCH_LIST})
	IF(NOT TERMLIB_INCLUDE_DIR OR NOT TERMLIB_LIBRARY)
		FIND_PATH(TERMLIB_INCLUDE_DIR ${tname}.h)
		FIND_LIBRARY(TERMLIB_LIBRARY ${tname})
		IF(TERMLIB_INCLUDE_DIR AND TERMLIB_LIBRARY)
			CHECK_LIBRARY_EXISTS(${tname} tputs "" TERMLIB_LIBRARY_EXISTS)
			IF(NOT TERMLIB_LIBRARY_EXISTS)
				SET(TERMLIB_LIBRARY "NOTFOUND")
			ENDIF(NOT TERMLIB_LIBRARY_EXISTS)
	      MESSAGE("name:  ${tname} TERMLIB_INCLUDE_DIR: ${TERMLIB_INCLUDE_DIR} TERMLIB_LIBRARY: ${TERMLIB_LIBRARY}")
		ENDIF(TERMLIB_INCLUDE_DIR AND TERMLIB_LIBRARY)
	ENDIF(NOT TERMLIB_INCLUDE_DIR OR NOT TERMLIB_LIBRARY)
ENDFOREACH(tname ${TERMHEADER_SEARCH_LIST})

MARK_AS_ADVANCED(TERMLIB_LIBRARY TERMLIB_INCLUDE_DIR)

# handle the QUIETLY and REQUIRED arguments and set TERMLIB_FOUND to TRUE if 
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(TERMLIB DEFAULT_MSG TERMLIB_INCLUDE_DIR TERMLIB_LIBRARY)

