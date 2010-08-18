# - Find Tcl/Tk includes and libraries.

# There are quite a number of potential compilcations when it comes to
# Tcl/Tk, particularly in cases where multiple versions of Tcl/Tk are
# present on a system and the case of OSX, which my have Tk built for
# either X11 or Aqua.  On Windows there may be Cygwin installs of
# Tcl/Tk as well.  The approach of this cmake file, as opposed to
# the standard one in CMake, is to provide controlling variables which
# will allow a parent CMakeLists.txt file to specify what they are
# looking for.  For the moment, this file is intended to handle everything -
# stub libraries, tclsh, and the libraries themselves.

SET(TCLTK_POSSIBLE_MAJOR_VERSIONS 8)
SET(TCLTK_POSSIBLE_MINOR_VERSIONS 6 5 4 3 2 1 0)

OPTION(TCLTK_USE_TK "Look for Tk installation, not just Tcl." ON)
OPTION(TCLTK_NEED_HEADERS "Don't report a found Tcl/Tk unless headers are present." ON)

SET(tkwin_script "
set filename \"${CMAKE_BINARY_DIR}/CMakeTmp/TK_WINDOWINGSYSTEM\"
set fileId [open $filename \"w\"]
set windowingsystem [tk windowingsystem]
puts $fileId $windowingsystem
close $fileId
exit
")

SET(tkwin_scriptfile "${CMAKE_BINARY_DIR}/CMakeTmp/tk_windowingsystem.tcl")

FILE(WRITE ${tkwin_scriptfile} ${tkwin_script})

# There are four variables that will be used as "feeders"
# for locating Tcl/Tk installations - TCL_PREFIX, TCL_INCLUDE_DIR,
# TCL_LIBRARY_DIR, and TCL_BIN_DIR.  In search paths, TCL_PREFIX
# generated paths should come after the more specific variables.

IF(TCL_PREFIX)
   SET(TCL_PREFIX_BIN "${TCL_PREFIX}/bin")
   SET(TCL_PREFIX_LIB "${TCL_PREFIX}/lib")
   SET(TCL_PREFIX_INC "${TCL_PREFIX}/include")
ENDIF(TCL_PREFIX)

MACRO(WIN32TCLTKPATHS vararray extension)
  IF(WIN32)
    GET_FILENAME_COMPONENT(
      ActiveTcl_CurrentVersion 
      "[HKEY_LOCAL_MACHINE\\SOFTWARE\\ActiveState\\ActiveTcl;CurrentVersion]" 
      NAME)
    SET(${vararray} ${${vararray}}
      "[HKEY_LOCAL_MACHINE\\SOFTWARE\\ActiveState\\ActiveTcl\\${ActiveTcl_CurrentVersion}]/${extension}")
      FOREACH(MAJORNUM ${TCLTK_POSSIBLE_MAJOR_VERSIONS})
        FOREACH(MINORNUM ${TCLTK_POSSIBLE_MINOR_VERSIONS})
         SET(${vararray} ${${vararray}} "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Scriptics\\Tcl\\${MAJORNUM}.${MINORNUM};Root]/${extension}")
        ENDFOREACH()
      ENDFOREACH()
    ENDIF(WIN32)
ENDMACRO(WIN32TCLTKPATHS)

IF(WIN32)
   WIN32TCLTKPATHS(SYSTEM_LOCALS bin)
endif(WIN32)

SET(SEARCHPATHS ${TCL_BIN_DIR} ${TCL_PREFIX_BIN} ${SYSTEM_LOCALS})

FOREACH(MAJORNUM ${TCLTK_POSSIBLE_MAJOR_VERSIONS})
   FOREACH(MINORNUM ${TCLTK_POSSIBLE_MINOR_VERSIONS})
      SET(TCL_TCLSH${MAJORNUM}${MINORNUM} TCL_TCLSH${MAJORNUM}${MINORNUM}-NOTFOUND)
      FOREACH(SPATH ${SEARCHPATHS})
         FIND_PROGRAM(TCL_TCLSH${MAJORNUM}${MINORNUM} NAMES tclsh${MAJORNUM}.${MINORNUM} tclsh${MAJORNUM}${MINORNUM} PATHS ${SPATH} NO_SYSTEM_PATH)
         IF(NOT TCL_TCLSH${MAJORNUM}${MINORNUM} MATCHES "NOTFOUND$")
            SET(TCL_TCLSH${MAJORNUM}${MINORNUM}_LIST ${TCL_TCLSH${MAJORNUM}${MINORNUM}_LIST} ${TCL_TCLSH${MAJORNUM}${MINORNUM}})
         endif()
         SET(TCL_TCLSH${MAJORNUM}${MINORNUM} TCL_TCLSH${MAJORNUM}${MINORNUM}-NOTFOUND)
      ENDFOREACH()
      # Tried all the specific paths - now see what the system PATH has
      FIND_PROGRAM(TCL_TCLSH${MAJORNUM}${MINORNUM} NAMES tclsh${MAJORNUM}.${MINORNUM} tclsh${MAJORNUM}${MINORNUM})
      IF(NOT TCL_TCLSH${MAJORNUM}${MINORNUM} MATCHES "NOTFOUND$")
         # Make sure we don't have it already
         IF (NOT TCL_TCLSH${MAJORNUM}${MINORNUM}_LIST MATCHES "${TCL_TCLSH${MAJORNUM}${MINORNUM}}")
            # On repeat configures, CMAKE_INSTALL_PREFIX apparently gets added to the FIND_PROGRAM search path.  This is a problem
            # if re-installing over a previous compile that includes a tclsh of its own, so strip these out - never want
            # to use a tclsh from a previous build to do the current build!
            IF(NOT TCL_TCLSH${MAJORNUM}${MINORNUM} MATCHES "^${CMAKE_INSTALL_PREFIX}")
               SET(TCL_TCLSH${MAJORNUM}${MINORNUM}_LIST ${TCL_TCLSH${MAJORNUM}${MINORNUM}_LIST} ${TCL_TCLSH${MAJORNUM}${MINORNUM}})
            endif()
         endif()
      endif()
      SET(TCL_FOUND_MAJOR_VERSIONS ${TCL_FOUND_MAJOR_VERSIONS} ${MAJORNUM})
      SET(TCL_FOUND_MINOR_VERSIONS ${TCL_FOUND_MINOR_VERSIONS} ${MINORNUM})
      LIST(GET TCL_TCLSH${MAJORNUM}${MINORNUM}_LIST 0 FIRST_ITEM)
      IF(FIRST_ITEM)
         SET(TCL_TCLSH${MAJORNUM}${MINORNUM} ${FIRST_ITEM} CACHE STRING "tclsh var" FORCE)
         IF(NOT TCL_MAX_FOUND_MINOR_VERSION)
	   SET(TCL_MAX_FOUND_MINOR_VERSION ${MINORNUM})
         ENDIF()
      endif()
   MESSAGE("list${MAJORNUM}${MINORNUM}: ${TCL_TCLSH${MAJORNUM}${MINORNUM}_LIST}")
   ENDFOREACH()
ENDFOREACH()

LIST(REMOVE_DUPLICATES TCL_FOUND_MAJOR_VERSIONS)
LIST(REMOVE_DUPLICATES TCL_FOUND_MINOR_VERSIONS)

GET_FILENAME_COMPONENT(TCL_TCLSH_PATH_PARENT "${TCL_BIN_DIR}" PATH)
SET(TCLTK_POSSIBLE_PATHS ${TCLTK_POSSIBLE_PATHS} ${TCL_TCLSH_PATH_PARENT})

GET_FILENAME_COMPONENT(TCL_TCLSH_PATH_PARENT "${TCL_LIBRARY_DIR}" PATH)
SET(TCLTK_POSSIBLE_PATHS ${TCLTK_POSSIBLE_PATHS} ${TCL_TCLSH_PATH_PARENT})

GET_FILENAME_COMPONENT(TCL_TCLSH_PATH_PARENT "${TCL_INCLUDE_DIR}" PATH)
SET(TCLTK_POSSIBLE_PATHS ${TCLTK_POSSIBLE_PATHS} ${TCL_TCLSH_PATH_PARENT})

SET(TCLTK_POSSIBLE_PATHS ${TCLTK_POSSIBLE_PATHS} ${TCL_PREFIX})

FOREACH(MAJORNUM ${TCL_FOUND_MAJOR_VERSIONS})
   FOREACH(MINORNUM ${TCL_FOUND_MINOR_VERSIONS})
       SET(LISTITEM 0)
       LIST(LENGTH TCL_TCLSH${MAJORNUM}${MINORNUM}_LIST LISTLENGTH)
       IF(LISTLENGTH GREATER 0)
          WHILE (LISTLENGTH GREATER LISTITEM)
             LIST(GET TCL_TCLSH${MAJORNUM}${MINORNUM}_LIST ${LISTITEM} TCL_TCLSH)
             GET_FILENAME_COMPONENT(TCL_TCLSH_PATH "${TCL_TCLSH}" PATH)
             GET_FILENAME_COMPONENT(TCL_TCLSH_PATH_PARENT "${TCL_TCLSH_PATH}" PATH)
             IF (TCLTK_USE_TK)
		 # FIXME: There is a risk here of picking up a wish in a system path that we don't want to consider, but if
		 # the NO_SYSTEM_PATH option is added valid entires in our list will not find their corresponding wish
		 # even if it is there - looks like we're going to have to special case tclsh/wish found in system paths
                 FIND_PROGRAM(TCLTK_WISH${MAJORNUM}${MINORNUM} NAMES wish${MAJORNUM}.${MINORNUM} wish${MAJORNUM}${MINORNUM} PATHS ${TCL_TCLSH_PATH_PARENT}/bin})
                 IF(NOT TCLTK_WISH${MAJORNUM}${MINORNUM} MATCHES "NOTFOUND$")
                    SET(TCLTK_POSSIBLE_PATHS ${TCLTK_POSSIBLE_PATHS} ${TCL_TCLSH_PATH_PARENT})
		 endif()
             ELSE(TCLTK_USE_TK)
               SET(TCLTK_POSSIBLE_PATHS ${TCLTK_POSSIBLE_PATHS} ${TCL_TCLSH_PATH_PARENT})
             ENDIF(TCLTK_USE_TK)
	     math(EXPR LISTITEM "${LISTITEM} + 1")
	  endwhile()
       endif()
   ENDFOREACH()
ENDFOREACH()

LIST(REMOVE_DUPLICATES TCLTK_POSSIBLE_PATHS)

MESSAGE("possible paths: ${TCLTK_POSSIBLE_PATHS}")

GET_FILENAME_COMPONENT(TCL_TCLSH_PATH "${TCL_TCLSH}" PATH)
GET_FILENAME_COMPONENT(TCL_TCLSH_PATH_PARENT "${TCL_TCLSH_PATH}" PATH)
STRING(REGEX REPLACE 
  "^.*tclsh([0-9]\\.*[0-9]).*$" "\\1" TCL_TCLSH_VERSION "${TCL_TCLSH}")

GET_FILENAME_COMPONENT(TK_WISH_PATH "${TK_WISH}" PATH)
GET_FILENAME_COMPONENT(TK_WISH_PATH_PARENT "${TK_WISH_PATH}" PATH)
STRING(REGEX REPLACE 
  "^.*wish([0-9]\\.*[0-9]).*$" "\\1" TK_WISH_VERSION "${TK_WISH}")

GET_FILENAME_COMPONENT(TCL_INCLUDE_PATH_PARENT "${TCL_INCLUDE_PATH}" PATH)
GET_FILENAME_COMPONENT(TK_INCLUDE_PATH_PARENT "${TK_INCLUDE_PATH}" PATH)

GET_FILENAME_COMPONENT(TCL_LIBRARY_PATH "${TCL_LIBRARY}" PATH)
GET_FILENAME_COMPONENT(TCL_LIBRARY_PATH_PARENT "${TCL_LIBRARY_PATH}" PATH)
STRING(REGEX REPLACE 
  "^.*tcl([0-9]\\.*[0-9]).*$" "\\1" TCL_LIBRARY_VERSION "${TCL_LIBRARY}")

GET_FILENAME_COMPONENT(TK_LIBRARY_PATH "${TK_LIBRARY}" PATH)
GET_FILENAME_COMPONENT(TK_LIBRARY_PATH_PARENT "${TK_LIBRARY_PATH}" PATH)
STRING(REGEX REPLACE 
  "^.*tk([0-9]\\.*[0-9]).*$" "\\1" TK_LIBRARY_VERSION "${TK_LIBRARY}")

SET(TCLTK_POSSIBLE_LIB_PATHS
  "${TCL_INCLUDE_PATH_PARENT}/lib"
  "${TK_INCLUDE_PATH_PARENT}/lib"
  "${TCL_LIBRARY_PATH}"
  "${TK_LIBRARY_PATH}"
  "${TCL_TCLSH_PATH_PARENT}/lib"
  "${TK_WISH_PATH_PARENT}/lib"
  /usr/lib 
  /usr/local/lib
  )
FIND_LIBRARY(TCL_LIBRARY
  NAMES 
  tcl   
  tcl${TK_LIBRARY_VERSION} tcl${TCL_TCLSH_VERSION} tcl${TK_WISH_VERSION}
  tcl86 tcl8.6 
  tcl85 tcl8.5 
  tcl84 tcl8.4 
  tcl83 tcl8.3 
  tcl82 tcl8.2 
  tcl80 tcl8.0
  PATHS ${TCLTK_POSSIBLE_LIB_PATHS}
  )

FIND_LIBRARY(TK_LIBRARY 
  NAMES 
  tk
  tk${TCL_LIBRARY_VERSION} tk${TCL_TCLSH_VERSION} tk${TK_WISH_VERSION}
  tk86 tk8.6
  tk85 tk8.5 
  tk84 tk8.4 
  tk83 tk8.3 
  tk82 tk8.2 
  tk80 tk8.0
  PATHS ${TCLTK_POSSIBLE_LIB_PATHS}
  )

SET(TCLTK_POSSIBLE_INCLUDE_PATHS
  "${TCL_LIBRARY_PATH_PARENT}/include"
  "${TK_LIBRARY_PATH_PARENT}/include"
  "${TCL_INCLUDE_PATH}"
  "${TK_INCLUDE_PATH}"
  ${TCL_FRAMEWORK_INCLUDES} 
  ${TK_FRAMEWORK_INCLUDES} 
  "${TCL_TCLSH_PATH_PARENT}/include"
  "${TK_WISH_PATH_PARENT}/include"
  /usr/include
  /usr/local/include
  /usr/include/tcl${TK_LIBRARY_VERSION}
  /usr/include/tcl${TCL_LIBRARY_VERSION}
  /usr/local/include/tcl${TK_LIBRARY_VERSION}
  /usr/local/include/tcl${TCL_LIBRARY_VERSION}
  /usr/include/tcl8.6
  /usr/include/tcl8.5
  /usr/include/tcl8.4
  /usr/include/tcl8.3
  /usr/include/tcl8.2
  /usr/include/tcl8.0
  /usr/local/include/tcl8.6
  /usr/local/include/tcl8.5
  /usr/local/include/tcl8.4
  /usr/local/include/tcl8.3
  /usr/local/include/tcl8.2
  /usr/local/include/tcl8.0
  /usr/include/tk${TK_LIBRARY_VERSION}
  /usr/include/tk${TCL_LIBRARY_VERSION}
  /usr/local/include/tk${TK_LIBRARY_VERSION}
  /usr/local/include/tk${TCL_LIBRARY_VERSION}
  /usr/include/tk8.6
  /usr/include/tk8.5
  /usr/include/tk8.4
  /usr/include/tk8.3
  /usr/include/tk8.2
  /usr/include/tk8.0
  /usr/local/include/tk8.6
  /usr/local/include/tk8.5
  /usr/local/include/tk8.4
  /usr/local/include/tk8.3
  /usr/local/include/tk8.2
  /usr/local/include/tk8.0
  )

IF(WIN32)
  SET(TCLTK_POSSIBLE_INCLUDE_PATHS ${TCLTK_POSSIBLE_INCLUDE_PATHS}
    "[HKEY_LOCAL_MACHINE\\SOFTWARE\\ActiveState\\ActiveTcl\\${ActiveTcl_CurrentVersion}]/include"
    "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Scriptics\\Tcl\\8.6;Root]/include"
    "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Scriptics\\Tcl\\8.5;Root]/include"
    "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Scriptics\\Tcl\\8.4;Root]/include"
    "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Scriptics\\Tcl\\8.3;Root]/include"
    "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Scriptics\\Tcl\\8.2;Root]/include"
    "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Scriptics\\Tcl\\8.0;Root]/include"
    "$ENV{ProgramFiles}/Tcl/include"
    "C:/Program Files/Tcl/include"
    "C:/Tcl/include"
    )
ENDIF(WIN32)

FIND_PATH(TCL_INCLUDE_PATH 
  NAMES tcl.h
  HINTS ${TCLTK_POSSIBLE_INCLUDE_PATHS}
  )

FIND_PATH(TK_INCLUDE_PATH 
  NAMES tk.h
  HINTS ${TCLTK_POSSIBLE_INCLUDE_PATHS}
  )

# handle the QUIETLY and REQUIRED arguments and set TCL_FOUND to TRUE if 
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)

FIND_PACKAGE_HANDLE_STANDARD_ARGS(TCL DEFAULT_MSG TCL_LIBRARY TCL_INCLUDE_PATH)
SET(TCLTK_FIND_REQUIRED ${TCL_FIND_REQUIRED})
SET(TCLTK_FIND_QUIETLY  ${TCL_FIND_QUIETLY})
FIND_PACKAGE_HANDLE_STANDARD_ARGS(TCLTK DEFAULT_MSG TCL_LIBRARY TCL_INCLUDE_PATH TK_LIBRARY TK_INCLUDE_PATH)
SET(TK_FIND_REQUIRED ${TCL_FIND_REQUIRED})
SET(TK_FIND_QUIETLY  ${TCL_FIND_QUIETLY})
FIND_PACKAGE_HANDLE_STANDARD_ARGS(TK DEFAULT_MSG TK_LIBRARY TK_INCLUDE_PATH)

MARK_AS_ADVANCED(
  TCL_INCLUDE_PATH
  TK_INCLUDE_PATH
  TCL_LIBRARY
  TK_LIBRARY
  )
