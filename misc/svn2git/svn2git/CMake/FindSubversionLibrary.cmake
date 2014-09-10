# - Try to find subversion libraries
#
#  This macro uses the following variables as preference for searching the
#  headers and includes:
#  SUBVERSION_INSTALL_PATH - root directory where subversion is installed (usually /usr)
#  APR_CONFIG_PATH - path where apr-config or apr-1-config are located
#  APU_CONFIG_PATH - path where apu-config or apu-1-config are located
#
#  The variables set by this macro are:
#  SUBVERSION_FOUND - system has subversion libraries
#  SUBVERSION_INCLUDE_DIRS - the include directories to link to subversion
#  SUBVERSION_LIBRARIES - The libraries needed to link to subversion

IF(SUBVERSION_INCLUDE_DIRS AND SUBVERSION_LIBRARIES)
    # Already in cache, be silent
    SET(Subversion_FIND_QUIETLY TRUE)
ENDIF(SUBVERSION_INCLUDE_DIRS AND SUBVERSION_LIBRARIES)

IF (NOT WIN32)

    MACRO(FIND_SUB_LIB targetvar libname)
      FIND_LIBRARY(${targetvar} ${libname}
          HINTS
          ${SUBVERSION_INSTALL_PATH}/lib
      )
    ENDMACRO(FIND_SUB_LIB)

    FIND_PATH(SUBVERSION_INCLUDE_DIR svn_version.h
        HINTS
        ${SUBVERSION_INSTALL_PATH}/include
        PATH_SUFFIXES subversion-1
    )

    FIND_SUB_LIB(SUBVERSION_CLIENTLIB svn_client-1)
    FIND_SUB_LIB(SUBVERSION_REPOSITORYLIB svn_repos-1)
    FIND_SUB_LIB(SUBVERSION_WCLIB svn_wc-1)
    FIND_SUB_LIB(SUBVERSION_FSLIB svn_fs-1)
    FIND_SUB_LIB(SUBVERSION_SUBRLIB svn_subr-1)
    FIND_SUB_LIB(SUBVERSION_RALIB svn_ra-1)


    IF(APR_CONFIG_PATH)
        FIND_PROGRAM(APR_CONFIG NAMES apr-config apr-1-config
            PATHS
            ${APR_CONFIG_PATH}
            /usr/local/apr/bin
        )
    ELSE(APR_CONFIG_PATH)
        FIND_PROGRAM(APR_CONFIG NAMES apr-config apr-1-config
            PATHS
            /usr/local/apr/bin
        )
    ENDIF(APR_CONFIG_PATH)

    IF(APU_CONFIG_PATH)
        FIND_PROGRAM(APU_CONFIG NAMES apu-config apu-1-config
            PATHS
            ${APU_CONFIG_PATH}
            /usr/local/apr/bin
        )
    ELSE(APU_CONFIG_PATH)
        FIND_PROGRAM(APU_CONFIG NAMES apu-config apu-1-config
            PATHS
            /usr/local/apr/bin
        )
    ENDIF(APU_CONFIG_PATH)

    IF(NOT APR_CONFIG)
        MESSAGE(STATUS "no apr-config found, subversion support will be disabled")
        SET(SUBVERSION_FOUND false)
    ELSE(NOT APR_CONFIG)
        EXEC_PROGRAM(${APR_CONFIG} ARGS "--includedir" OUTPUT_VARIABLE APR_INCLUDE_DIR)
        STRING(REPLACE "-I" "" APR_INCLUDE_DIR ${APR_INCLUDE_DIR})
        EXEC_PROGRAM(${APR_CONFIG} ARGS "--link-ld" OUTPUT_VARIABLE _LINK_LD_ARGS)
        STRING( REPLACE " " ";" _LINK_LD_ARGS ${_LINK_LD_ARGS} )
        FOREACH( _ARG ${_LINK_LD_ARGS} )
            IF(${_ARG} MATCHES "^-L")
                STRING(REGEX REPLACE "^-L" "" _ARG ${_ARG})
                SET(_APR_LIB_PATHS ${_APR_LIB_PATHS} ${_ARG})
            ENDIF(${_ARG} MATCHES "^-L")
            IF(${_ARG} MATCHES "^-l")
               STRING(REGEX REPLACE "^-l" "" _ARG ${_ARG})
               FIND_LIBRARY(_APR_LIB_FROM_ARG NAMES ${_ARG}
                    PATHS
                    ${_APR_LIB_PATHS}
               )
               IF(_APR_LIB_FROM_ARG)
                  SET(APR_LIBRARY ${APR_LIBRARY} ${_APR_LIB_FROM_ARG})
               ENDIF(_APR_LIB_FROM_ARG)
            ENDIF(${_ARG} MATCHES "^-l")
         ENDFOREACH(_ARG)
    ENDIF(NOT APR_CONFIG)

    IF(NOT APU_CONFIG)
        MESSAGE(STATUS "no apu-config found, subversion support will be disabled")
        SET(SUBVERSION_FOUND false)
    ELSE(NOT APU_CONFIG)
        EXEC_PROGRAM(${APU_CONFIG} ARGS "--includedir" OUTPUT_VARIABLE APU_INCLUDE_DIR)
        STRING(REPLACE "-I" "" APU_INCLUDE_DIR ${APU_INCLUDE_DIR})
        EXEC_PROGRAM(${APU_CONFIG} ARGS "--link-ld" OUTPUT_VARIABLE _LINK_LD_ARGS)
        STRING( REPLACE " " ";" _LINK_LD_ARGS ${_LINK_LD_ARGS} )
        SET(_APU_LIB_PATHS "")
        FOREACH( _ARG ${_LINK_LD_ARGS} )
            IF(${_ARG} MATCHES "^-L")
                STRING(REGEX REPLACE "^-L" "" _ARG ${_ARG})
                SET(_APU_LIB_PATHS ${_APU_LIB_PATHS} ${_ARG})
            ENDIF(${_ARG} MATCHES "^-L")
            IF(${_ARG} MATCHES "^-l")
               STRING(REGEX REPLACE "^-l" "" _ARG ${_ARG})
               FIND_LIBRARY(_APU_LIB_FROM_ARG NAMES ${_ARG}
                    PATHS
                    ${_APU_LIB_PATHS}
               )
               IF(_APU_LIB_FROM_ARG)
                  SET(APU_LIBRARY ${APU_LIBRARY} ${_APU_LIB_FROM_ARG})
               ENDIF(_APU_LIB_FROM_ARG)
            ENDIF(${_ARG} MATCHES "^-l")
        ENDFOREACH(_ARG)
    ENDIF(NOT APU_CONFIG)
ELSE (NOT WIN32)
#search libraries for Windows
#this needs more work

# search for paths

    MACRO(FIND_SUB_INC targetvar libname pathadd)
      IF (SUBVERSION_INSTALL_PATH)
          FIND_PATH(${targetvar} ${libname}
              PATHS
              ${SUBVERSION_INSTALL_PATH}/include${pathadd}
              "$ENV{ProgramFiles}/Subversion/include"${pathadd}
          )
      ELSE(SUBVERSION_INSTALL_PATH)
          FIND_LIBRARY(${targetvar} ${libname}
              PATHS
              "$ENV{ProgramFiles}/Subversion/include"${pathadd}
          )
      ENDIF(SUBVERSION_INSTALL_PATH)
    ENDMACRO(FIND_SUB_INC)

    MACRO(FIND_SUB_LIB targetvar libname)
      IF (SUBVERSION_INSTALL_PATH)
          FIND_LIBRARY(${targetvar} ${libname}
              PATHS
              ${SUBVERSION_INSTALL_PATH}/lib
              "$ENV{ProgramFiles}/Subversion/lib
          )
      ELSE(SUBVERSION_INSTALL_PATH)
          FIND_LIBRARY(${targetvar} ${libname}
              PATHS
              "$ENV{ProgramFiles}/Subversion/lib
          )
      ENDIF(SUBVERSION_INSTALL_PATH)
    ENDMACRO(FIND_SUB_LIB)

    FIND_SUB_INC(SUBVERSION_INCLUDE_DIR svn_version.h "")

    FIND_SUB_INC(APR_INCLUDE_DIR apr.h /apr)

    FIND_SUB_INC(APU_INCLUDE_DIR apu.h /apr-util)


  # search for libraries
    FIND_SUB_LIB(APR_LIBRARY apr)

    FIND_SUB_LIB(APRICONV_LIB apriconv)

    FIND_SUB_LIB(APU_LIBRARY aprutil)

    #FIND_SUB_LIB(APU_XMLLIB xml)

    #FIND_SUB_LIB(NEON_LIB libneon)

    #FIND_SUB_LIB(NEON_ZLIBSTATLIB zlibstat )

    FIND_SUB_LIB(SUBVERSION_CLIENTLIB libsvn_client-1)

    FIND_SUB_LIB(SUBVERSION_DELTALIB libsvn_delta-1)

    FIND_SUB_LIB(SUBVERSION_DIFFLIB libsvn_diff-1)

    #FIND_SUB_LIB(SUBVERSION_FSBASELIB libsvn_fs_base-1)

    #FIND_SUB_LIB(SUBVERSION_FSFSLIB libsvn_fs_fs-1)

    FIND_SUB_LIB(SUBVERSION_FSLIB libsvn_fs-1)

    #FIND_SUB_LIB(SUBVERSION_RADAVLIB libsvn_ra_dav-1)

    #FIND_SUB_LIB(SUBVERSION_RALOCALLIB libsvn_ra_local-1)

    #FIND_SUB_LIB(SUBVERSION_RASVNLIB libsvn_ra_svn-1)

    FIND_SUB_LIB(SUBVERSION_RALIB libsvn_ra-1)

    FIND_SUB_LIB(SUBVERSION_REPOSITORYLIB libsvn_repos-1)

    FIND_SUB_LIB(SUBVERSION_SUBRLIB libsvn_subr-1)

    FIND_SUB_LIB(SUBVERSION_WCLIB libsvn_wc-1)

    # these are the win32-only libs, the others handled at the bottom.
    MARK_AS_ADVANCED(
        APRICONV_LIB
        #APU_XMLLIB
        #NEON_LIB
        #NEON_ZLIBSTATLIB
        SUBVERSION_DELTALIB
        SUBVERSION_DIFFLIB
        SUBVERSION_FSBASELIB
        SUBVERSION_FSFSLIB
        SUBVERSION_RADAVLIB
        SUBVERSION_RALOCALLIB
        SUBVERSION_RASVNLIB
    )

  # check found libraries

    IF (NOT APRICONV_LIB)
        MESSAGE(STATUS "No apriconv lib found!")
    ELSE (NOT APRICONV_LIB)
        IF(NOT Subversion_FIND_QUIETLY)
            MESSAGE(STATUS "Found apriconv lib: ${APRICONV_LIB}")
        ENDIF(NOT Subversion_FIND_QUIETLY)
        SET(SUBVERSION_LIBRARIES ${SUBVERSION_LIBRARIES} ${APRICONV_LIB})
    ENDIF(NOT APRICONV_LIB)

    #IF (NOT APU_XMLLIB)
    #    MESSAGE(STATUS "No xml lib found!")
    #ELSE (NOT APU_XMLLIB)
    #    IF(NOT Subversion_FIND_QUIETLY)
    #        MESSAGE(STATUS "Found xml lib: ${APU_XMLLIB}")
    #    ENDIF(NOT Subversion_FIND_QUIETLY)
    #    SET(SUBVERSION_LIBRARIES ${SUBVERSION_LIBRARIES} ${APU_XMLLIB})
    #ENDIF(NOT APU_XMLLIB)

    #IF (NOT NEON_LIB)
    #    MESSAGE(STATUS "No neon lib found!")
    #ELSE (NOT NEON_LIB)
    #    IF(NOT Subversion_FIND_QUIETLY)
    #        MESSAGE(STATUS "Found neon lib: ${NEON_LIB}")
    #    ENDIF(NOT Subversion_FIND_QUIETLY)
    #    SET(SUBVERSION_LIBRARIES ${SUBVERSION_LIBRARIES} ${NEON_LIB})
    #ENDIF(NOT NEON_LIB)

    #IF (NOT NEON_ZLIBSTATLIB)
    #    MESSAGE(STATUS "No zlibstat lib found!")
    #ELSE (NOT APRICONV_LIB)
    #    IF(NOT Subversion_FIND_QUIETLY)
    #        MESSAGE(STATUS "Found zlibstat lib: ${NEON_ZLIBSTATLIB}")
    #    ENDIF(NOT Subversion_FIND_QUIETLY)
    #    SET(SUBVERSION_LIBRARIES ${SUBVERSION_LIBRARIES} ${NEON_ZLIBSTATLIB})
    #ENDIF(NOT NEON_ZLIBSTATLIB)

    IF (NOT SUBVERSION_DELTALIB)
        MESSAGE(STATUS "No subversion delta lib found!")
    ELSE (NOT SUBVERSION_DELTALIB)
        IF(NOT Subversion_FIND_QUIETLY)
            MESSAGE(STATUS "Found subversion delta lib: ${SUBVERSION_DELTALIB}")
        ENDIF(NOT Subversion_FIND_QUIETLY)
        SET(SUBVERSION_LIBRARIES ${SUBVERSION_LIBRARIES} ${SUBVERSION_DELTALIB})
    ENDIF(NOT SUBVERSION_DELTALIB)

    IF (NOT SUBVERSION_DIFFLIB)
        MESSAGE(STATUS "No subversion diff lib found!")
    ELSE (NOT SUBVERSION_DIFFLIB)
        IF(NOT Subversion_FIND_QUIETLY)
            MESSAGE(STATUS "Found subversion diff lib: ${SUBVERSION_DIFFLIB}")
        ENDIF(NOT Subversion_FIND_QUIETLY)
        SET(SUBVERSION_LIBRARIES ${SUBVERSION_LIBRARIES} ${SUBVERSION_DIFFLIB})
    ENDIF(NOT SUBVERSION_DIFFLIB)

    #IF (NOT SUBVERSION_FSBASELIB)
    #    MESSAGE(STATUS "No subversion fs base lib found!")
    #ELSE (NOT SUBVERSION_FSBASELIB)
    #    IF(NOT Subversion_FIND_QUIETLY)
    #        MESSAGE(STATUS "Found subversion fs base lib: ${SUBVERSION_FSBASELIB}")
    #    ENDIF(NOT Subversion_FIND_QUIETLY)
    #    SET(SUBVERSION_LIBRARIES ${SUBVERSION_LIBRARIES} ${SUBVERSION_FSBASELIB})
    #ENDIF(NOT SUBVERSION_FSBASELIB)

    #IF (NOT SUBVERSION_FSFSLIB)
    #    MESSAGE(STATUS "No subversion fs fs lib found!")
    #ELSE (NOT SUBVERSION_FSFSLIB)
    #    IF(NOT Subversion_FIND_QUIETLY)
    #        MESSAGE(STATUS "Found subversion fs fs lib: ${SUBVERSION_FSFSLIB}")
    #    ENDIF(NOT Subversion_FIND_QUIETLY)
    #    SET(SUBVERSION_LIBRARIES ${SUBVERSION_LIBRARIES} ${SUBVERSION_FSFSLIB})
    #ENDIF(NOT SUBVERSION_FSFSLIB)

    #IF (NOT SUBVERSION_RADAVLIB)
    #    MESSAGE(STATUS "No subversion ra dav lib found!")
    #ELSE (NOT SUBVERSION_RADAVLIB)
    #    IF(NOT Subversion_FIND_QUIETLY)
    #        MESSAGE(STATUS "Found subversion lib: ${SUBVERSION_RADAVLIB}")
    #    ENDIF(NOT Subversion_FIND_QUIETLY)
    #    SET(SUBVERSION_LIBRARIES ${SUBVERSION_LIBRARIES} ${SUBVERSION_RADAVLIB})
    #ENDIF(NOT SUBVERSION_RADAVLIB)

    #IF (NOT SUBVERSION_RALOCALLIB)
    #    MESSAGE(STATUS "No subversion ra local lib found!")
    #ELSE (NOT SUBVERSION_RALOCALLIB)
    #    IF(NOT Subversion_FIND_QUIETLY)
    #        MESSAGE(STATUS "Found subversion ra local lib: ${SUBVERSION_RALOCALLIB}")
    #    ENDIF(NOT Subversion_FIND_QUIETLY)
    #    SET(SUBVERSION_LIBRARIES ${SUBVERSION_LIBRARIES} ${SUBVERSION_RALOCALLIB})
    #ENDIF(NOT SUBVERSION_RALOCALLIB)

    #IF (NOT SUBVERSION_RASVNLIB)
    #    MESSAGE(STATUS "No subversion ra svn lib found!")
    #ELSE (NOT SUBVERSION_RASVNLIB)
    #    IF(NOT Subversion_FIND_QUIETLY)
    #        MESSAGE(STATUS "Found subversion ra svn lib: ${SUBVERSION_RASVNLIB}")
    #    ENDIF(NOT Subversion_FIND_QUIETLY)
    #    SET(SUBVERSION_LIBRARIES ${SUBVERSION_LIBRARIES} ${SUBVERSION_RASVNLIB})
    #ENDIF(NOT SUBVERSION_RASVNLIB)

ENDIF (NOT WIN32)

##############################
# Setup the result variables #
##############################

IF(NOT SUBVERSION_INCLUDE_DIR)
  MESSAGE(STATUS "No subversion includes found, subversion support will be disabled")
  SET(SUBVERSION_FOUND false)
ELSE(NOT SUBVERSION_INCLUDE_DIR)
  IF(NOT Subversion_FIND_QUIETLY)
    MESSAGE(STATUS "Found subversion include: ${SUBVERSION_INCLUDE_DIR}")
  ENDIF(NOT Subversion_FIND_QUIETLY)
  SET(SUBVERSION_FOUND true)
  SET(SUBVERSION_INCLUDE_DIRS ${SUBVERSION_INCLUDE_DIR})
ENDIF(NOT SUBVERSION_INCLUDE_DIR)

IF(NOT APR_INCLUDE_DIR)
  MESSAGE(STATUS "No apr includes found, subversion support will be disabled")
  SET(APR_FOUND false)
ELSE(NOT APR_INCLUDE_DIR)
  IF(NOT Subversion_FIND_QUIETLY)
    MESSAGE(STATUS "Found apr include: ${APR_INCLUDE_DIR}")
  ENDIF(NOT Subversion_FIND_QUIETLY)
  SET(SUBVERSION_INCLUDE_DIRS ${SUBVERSION_INCLUDE_DIRS} ${APR_INCLUDE_DIR})
ENDIF(NOT APR_INCLUDE_DIR)

IF(NOT APU_INCLUDE_DIR)
  MESSAGE(STATUS "No apu includes found, subversion support will be disabled")
  SET(APU_FOUND false)
ELSE(NOT APU_INCLUDE_DIR)
  IF(NOT Subversion_FIND_QUIETLY)
    MESSAGE(STATUS "Found apu include: ${APU_INCLUDE_DIR}")
  ENDIF(NOT Subversion_FIND_QUIETLY)
  SET(SUBVERSION_INCLUDE_DIRS ${SUBVERSION_INCLUDE_DIRS} ${APU_INCLUDE_DIR})
ENDIF(NOT APU_INCLUDE_DIR)

IF (NOT SUBVERSION_CLIENTLIB)
  MESSAGE(STATUS "No subversion client libs found, subversion support will be disabled")
  SET(SUBVERSION_FOUND false)
ELSE (NOT SUBVERSION_CLIENTLIB)
  IF(NOT Subversion_FIND_QUIETLY)
    MESSAGE(STATUS "Found subversion client lib: ${SUBVERSION_CLIENTLIB}")
  ENDIF(NOT Subversion_FIND_QUIETLY)
  SET(SUBVERSION_LIBRARIES ${SUBVERSION_LIBRARIES} ${SUBVERSION_CLIENTLIB})
ENDIF(NOT SUBVERSION_CLIENTLIB)

IF (NOT SUBVERSION_REPOSITORYLIB)
  MESSAGE(STATUS "No subversion repository lib found, subversion support will be disabled")
  SET(SUBVERSION_FOUND false)
ELSE (NOT SUBVERSION_REPOSITORYLIB)
  IF(NOT Subversion_FIND_QUIETLY)
    MESSAGE(STATUS "Found subversion repository lib: ${SUBVERSION_REPOSITORYLIB}")
  ENDIF(NOT Subversion_FIND_QUIETLY)
  SET(SUBVERSION_LIBRARIES ${SUBVERSION_LIBRARIES} ${SUBVERSION_REPOSITORYLIB})
ENDIF(NOT SUBVERSION_REPOSITORYLIB)

IF (NOT SUBVERSION_FSLIB)
  MESSAGE(STATUS "No subversion fs lib found, subversion support will be disabled")
  SET(SUBVERSION_FOUND false)
ELSE (NOT SUBVERSION_FSLIB)
  IF(NOT Subversion_FIND_QUIETLY)
    MESSAGE(STATUS "Found subversion fs lib: ${SUBVERSION_FSLIB}")
  ENDIF(NOT Subversion_FIND_QUIETLY)
  SET(SUBVERSION_LIBRARIES ${SUBVERSION_LIBRARIES} ${SUBVERSION_FSLIB})
ENDIF(NOT SUBVERSION_FSLIB)

IF (NOT SUBVERSION_SUBRLIB)
  MESSAGE(STATUS "No subversion subr lib found, subversion support will be disabled")
  SET(SUBVERSION_FOUND false)
ELSE (NOT SUBVERSION_SUBRLIB)
  IF(NOT Subversion_FIND_QUIETLY)
    MESSAGE(STATUS "Found subversion subr lib: ${SUBVERSION_SUBRLIB}")
  ENDIF(NOT Subversion_FIND_QUIETLY)
  SET(SUBVERSION_LIBRARIES ${SUBVERSION_LIBRARIES} ${SUBVERSION_SUBRLIB})
ENDIF(NOT SUBVERSION_SUBRLIB)

IF (NOT SUBVERSION_WCLIB)
  MESSAGE(STATUS "No subversion wc lib found, subversion support will be disabled")
  SET(SUBVERSION_FOUND false)
ELSE (NOT SUBVERSION_WCLIB)
  IF(NOT Subversion_FIND_QUIETLY)
    MESSAGE(STATUS "Found subversion wc lib: ${SUBVERSION_WCLIB}")
  ENDIF(NOT Subversion_FIND_QUIETLY)
  SET(SUBVERSION_LIBRARIES ${SUBVERSION_LIBRARIES} ${SUBVERSION_WCLIB})
ENDIF(NOT SUBVERSION_WCLIB)

IF (NOT SUBVERSION_RALIB)
  MESSAGE(STATUS "No subversion ra lib found, subversion support will be disabled")
  SET(SUBVERSION_FOUND false)
ELSE (NOT SUBVERSION_RALIB)
  IF(NOT Subversion_FIND_QUIETLY)
    MESSAGE(STATUS "Found subversion ra lib: ${SUBVERSION_RALIB}")
  ENDIF(NOT Subversion_FIND_QUIETLY)
  SET(SUBVERSION_LIBRARIES ${SUBVERSION_LIBRARIES} ${SUBVERSION_RALIB})
ENDIF(NOT SUBVERSION_RALIB)

IF (NOT APR_LIBRARY)
  MESSAGE(STATUS "No apr lib found, subversion support will be disabled")
  SET(SUBVERSION_FOUND false)
ELSE (NOT APR_LIBRARY)
  IF(NOT Subversion_FIND_QUIETLY)
    MESSAGE(STATUS "Found apr lib: ${APR_LIBRARY}")
  ENDIF(NOT Subversion_FIND_QUIETLY)
  SET(SUBVERSION_LIBRARIES ${SUBVERSION_LIBRARIES} ${APR_LIBRARY})
ENDIF(NOT APR_LIBRARY)

IF (NOT APU_LIBRARY)
  MESSAGE(STATUS "No apu lib found, subversion support will be disabled")
  SET(SUBVERSION_FOUND false)
ELSE (NOT APU_LIBRARY)
  IF(NOT Subversion_FIND_QUIETLY)
    MESSAGE(STATUS "Found apu lib: ${APU_LIBRARY}")
  ENDIF(NOT Subversion_FIND_QUIETLY)
  SET(SUBVERSION_LIBRARIES ${SUBVERSION_LIBRARIES} ${APU_LIBRARY})
ENDIF(NOT APU_LIBRARY)

SET(SUBVERSION_LIBRARIES ${SUBVERSION_LIBRARIES} CACHE STRING "List of all subversion and related libraries")
SET(SUBVERSION_INCLUDE_DIRS ${SUBVERSION_INCLUDE_DIRS} CACHE STRING "List of all subversion and related libraries include directories")
#SET(SUBVERSION_INCLUDE_DIR ${SUBVERSION_INCLUDE_DIR} CACHE PATH "Path of subversion include directory")

MARK_AS_ADVANCED(
  SUBVERSION_RALIB
  SUBVERSION_WCLIB
  SUBVERSION_SUBRLIB
  SUBVERSION_FSLIB
  SUBVERSION_CLIENTLIB
  SUBVERSION_REPOSITORYLIB
  SUBVERSION_INCLUDE_DIR
  SUBVERSION_INCLUDE_DIRS
  SUBVERSION_LIBRARIES
  APR_INCLUDE_DIR
  APR_LIBRARY
  APU_INCLUDE_DIR
  APU_LIBRARY
)


#kate: space-indent on; indent-width 2; tab-width: 2; replace-tabs on; auto-insert-doxygen on

