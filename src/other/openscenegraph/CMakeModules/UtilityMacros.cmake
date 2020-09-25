
INCLUDE(ListHandle)

#---------------------------------------------------
# Macro: FILTER_OUT FILTERS INPUTS OUTPUT
#
# Mimicks Gnu Make's $(filter-out) which removes elements 
# from a list that match the pattern.
# Arguments:
#  FILTERS - list of patterns that need to be removed
#  INPUTS  - list of inputs that will be worked on
#  OUTPUT  - the filtered list to be returned
# 
# Example: 
#  SET(MYLIST this that and the other)
#  SET(FILTS this that)
#
#  FILTER_OUT("${FILTS}" "${MYLIST}" OUT)
#  MESSAGE("OUTPUT = ${OUT}")
#
# The output - 
#   OUTPUT = and;the;other
#
#---------------------------------------------------
MACRO(FILTER_OUT FILTERS INPUTS OUTPUT)
    SET(FOUT "")
    FOREACH(INP ${INPUTS})
        SET(FILTERED 0)

        FOREACH(FILT ${FILTERS})
            IF(${FILTERED} EQUAL 0)
                IF("${FILT}" STREQUAL "${INP}")
                    SET(FILTERED 1)
                ENDIF()
            ENDIF()
        ENDFOREACH()

        IF(${FILTERED} EQUAL 0)
            SET(FOUT ${FOUT} ${INP})
        ENDIF()

    ENDFOREACH(INP ${INPUTS})
    SET(${OUTPUT} ${FOUT})
ENDMACRO()

#---------------------------------------------------
# Macro: GET_HEADERS_EXTENSIONLESS DIR GLOB_PATTERN OUTPUT
#
#---------------------------------------------------
MACRO(GET_HEADERS_EXTENSIONLESS DIR GLOB_PATTERN OUTPUT)
    FILE(GLOB TMP "${DIR}/${GLOB_PATTERN}" )
    #FOREACH(F ${TMP})
    #    MESSAGE(STATUS "header-->${F}<--")
    #ENDFOREACH(F ${TMP})
    FILTER_OUT("${DIR}/CVS" "${TMP}" TMP)
    FILTER_OUT("${DIR}/cvs" "${TMP}" ${OUTPUT})
    FILTER_OUT("${DIR}/.svn" "${TMP}" ${OUTPUT})
ENDMACRO()

#---------------------------------------------------
# Macro: ADD_DIRS_TO_ENV_VAR _VARNAME
#
#---------------------------------------------------
   
MACRO(ADD_DIRS_TO_ENV_VAR _VARNAME )
    FOREACH(_ADD_PATH ${ARGN}) 
        FILE(TO_NATIVE_PATH ${_ADD_PATH} _ADD_NATIVE)
        #SET(_CURR_ENV_PATH $ENV{PATH})
        #LIST(SET _CURR_ENV_PATH ${_ADD_PATH})
        #SET(ENV{PATH} ${_CURR_ENV_PATH})${_FILE}
        IF(WIN32)
            SET(ENV{${_VARNAME}} "$ENV{${_VARNAME}};${_ADD_NATIVE}")
        ELSE()
            SET(ENV{${_VARNAME}} "$ENV{${_VARNAME}}:${_ADD_NATIVE}")
        ENDIF()
        #MESSAGE(" env ${_VARNAME} --->$ENV{${_VARNAME}}<---")
    ENDFOREACH()
ENDMACRO()

#---------------------------------------------------
# Macro: CORRECT_PATH VAR PATH 
# 
# Corrects slashes in PATH to be cmake conformous ( / ) 
# and puts result in VAR 
#---------------------------------------------------

MACRO(CORRECT_PATH VAR PATH)
    SET(${VAR} ${PATH})
    IF(WIN32)    
        STRING(REGEX REPLACE "/" "\\\\" ${VAR} "${PATH}")
    ENDIF()    
ENDMACRO()

#---------------------------------------------------
# Macro: TARGET_LOCATIONS_SET_FILE FILE
# TODO: Ok, this seems a bit ridiculuous.
#---------------------------------------------------

MACRO(TARGET_LOCATIONS_SET_FILE FILE)
    SET(ACCUM_FILE_TARGETS ${FILE})
    FILE(WRITE ${ACCUM_FILE_TARGETS} "")
ENDMACRO()

#---------------------------------------------------
# Macro: TARGET_LOCATIONS_ACCUM TARGET_NAME
# 
#---------------------------------------------------

MACRO(TARGET_LOCATIONS_ACCUM TARGET_NAME)
    IF(ACCUM_FILE_TARGETS)
        IF(EXISTS ${ACCUM_FILE_TARGETS})
            GET_TARGET_PROPERTY(_FILE_LOCATION ${TARGET_NAME} LOCATION)
            FILE(APPEND ${ACCUM_FILE_TARGETS} "${_FILE_LOCATION};")
            #SET(_TARGETS_LIST ${_TARGETS_LIST} "${_FILE_LOCATION}" CACHE INTERNAL "lista dll")
            #MESSAGE("adding target -->${TARGET_NAME}<-- file -->${_FILE_LOCATION}<-- to list -->${_TARGETS_LIST}<--")
            #SET(ACCUM_FILE_TARGETS ${ACCUM_FILE_TARGETS} ${_FILE_LOCATION})
        ENDIF()
    ENDIF()
ENDMACRO()

#---------------------------------------------------
# Macro: TARGET_LOCATIONS_GET_LIST _VAR
# 
#---------------------------------------------------

MACRO(TARGET_LOCATIONS_GET_LIST _VAR)
    IF(ACCUM_FILE_TARGETS)
        IF(EXISTS ${ACCUM_FILE_TARGETS})
            FILE(READ ${ACCUM_FILE_TARGETS} ${_VAR})    
        ENDIF(EXISTS ${ACCUM_FILE_TARGETS})
    ENDIF()
ENDMACRO()

#---------------------------------------------------
# Macro: FIND_DEPENDENCY DEPNAME INCLUDEFILE LIBRARY SEARCHPATHLIST
# 
#---------------------------------------------------

MACRO(FIND_DEPENDENCY DEPNAME INCLUDEFILE LIBRARY SEARCHPATHLIST)
    MESSAGE(STATUS "searching ${DEPNAME} -->${INCLUDEFILE}<-->${LIBRARY}<-->${SEARCHPATHLIST}<--")

    SET(MY_PATH_INCLUDE )
    SET(MY_PATH_LIB )
    SET(MY_PATH_BIN )

    FOREACH( MYPATH ${SEARCHPATHLIST} )
        SET(MY_PATH_INCLUDE ${MY_PATH_INCLUDE} ${MYPATH}/include)
        SET(MY_PATH_LIB ${MY_PATH_LIB} ${MYPATH}/lib)
        SET(MY_PATH_BIN ${MY_PATH_BIN} ${MYPATH}/bin)
    ENDFOREACH()

    SET(MYLIBRARY "${LIBRARY}")
    SEPARATE_ARGUMENTS(MYLIBRARY)

    #MESSAGE( " include paths: -->${MY_PATH_INCLUDE}<--")

    #MESSAGE( " ${DEPNAME}_INCLUDE_DIR --> ${${DEPNAME}_INCLUDE_DIR}<--")

    FIND_PATH("${DEPNAME}_INCLUDE_DIR" ${INCLUDEFILE}
        ${MY_PATH_INCLUDE}
        )
    MARK_AS_ADVANCED("${DEPNAME}_INCLUDE_DIR")
    #MESSAGE( " ${DEPNAME}_INCLUDE_DIR --> ${${DEPNAME}_INCLUDE_DIR}<--")

    FIND_LIBRARY("${DEPNAME}_LIBRARY" 
        NAMES ${MYLIBRARY}
        PATHS ${MY_PATH_LIB}
        )
    IF(${DEPNAME}_LIBRARY)
        GET_FILENAME_COMPONENT(MYLIBNAME ${${DEPNAME}_LIBRARY} NAME_WE)
        GET_FILENAME_COMPONENT(MYBINPATH ${${DEPNAME}_LIBRARY} PATH)
        GET_FILENAME_COMPONENT(MYBINPATH ${MYBINPATH} PATH)
        SET(MYBINPATH "${MYBINPATH}/bin")
        IF(EXISTS ${MYBINPATH})
            SET(MYFOUND 0)
            FOREACH(MYPATH ${MY_ACCUM_BINARY_DEP})
                IF(MYPATH MATCHES ${MYBINPATH})
                    SET(MYFOUND 1)
                    #MESSAGE("found -->${MYPATH}<-->${MYBINPATH}<--")
                ENDIF()
            ENDFOREACH()
            IF(MYFOUND EQUAL 0)
                SET(MY_ACCUM_BINARY_DEP ${MY_ACCUM_BINARY_DEP} ${MYBINPATH})
            ENDIF()
        ENDIF()
        #MESSAGE("${DEPNAME}_BINDEP searching -->${MYLIBNAME}${CMAKE_SHARED_MODULE_SUFFIX}<--in-->${MY_PATH_BIN}<--")
        #    FIND_FILE("${DEPNAME}_BINDEP" 
        #        ${MYLIBNAME}${CMAKE_SHARED_MODULE_SUFFIX}
        #      PATHS ${MY_PATH_BIN}
        #    )
        #    FIND_LIBRARY("${DEPNAME}_BINDEP" 
        #        NAMES ${MYLIBRARY}
        #      PATHS ${MY_PATH_BIN}
        #    )
    ENDIF()

    MARK_AS_ADVANCED("${DEPNAME}_LIBRARY")
    #MESSAGE( " ${DEPNAME}_LIBRARY --> ${${DEPNAME}_LIBRARY}<--")
    IF(${DEPNAME}_INCLUDE_DIR)
        IF(${DEPNAME}_LIBRARY)
            SET( ${DEPNAME}_FOUND "YES" )
            SET( ${DEPNAME}_LIBRARIES ${${DEPNAME}_LIBRARY} )
        ENDIF()
    ENDIF()
ENDMACRO()

#---------------------------------------------------
# Macro: MACRO_MESSAGE MYTEXT
#
#---------------------------------------------------

#SET(MACRO_MESSAGE_DEBUG TRUE)
MACRO(MACRO_MESSAGE MYTEXT)
    IF(MACRO_MESSAGE_DEBUG)
        MESSAGE("in file -->${CMAKE_CURRENT_LIST_FILE}<-- line -->${CMAKE_CURRENT_LIST_LINE}<-- message  ${MYTEXT}")
    ELSE()
        MESSAGE(STATUS "in file -->${CMAKE_CURRENT_LIST_FILE}<-- line -->${CMAKE_CURRENT_LIST_LINE}<-- message  ${MYTEXT}")
    ENDIF()
ENDMACRO()

