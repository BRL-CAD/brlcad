#-----------------------------------------------------------------------------
MACRO(THIRD_PARTY_OPTION upper lower)
  SET(${upper}_LIBRARY "${lower}-NOTFOUND")
  SET(${upper}_INCLUDE_DIR "${lower}-NOTFOUND")
  OPTION(${CMAKE_PROJECT_NAME}_BUILD_LOCAL_${upper} "Build the local ${lower} library." OFF)
 IF(NOT ${CMAKE_PROJECT_NAME}_BUILD_LOCAL_${upper})
    IF(EXISTS ${CMAKE_ROOT}/Modules/Find${upper}.cmake)
      INCLUDE(${CMAKE_ROOT}/Modules/Find${upper}.cmake)
    ELSE(EXISTS ${CMAKE_ROOT}/Modules/Find${upper}.cmake)
      INCLUDE(${${CMAKE_PROJECT_NAME}_CMAKE_DIR}/Find${upper}.cmake)
    ENDIF(EXISTS ${CMAKE_ROOT}/Modules/Find${upper}.cmake)
    MARK_AS_ADVANCED(${upper}_INCLUDE_DIR ${upper}_LIBRARY)
    IF(${upper}_FOUND)
      SET(${CMAKE_PROJECT_NAME}_${upper}_LIBRARIES ${${upper}_LIBRARIES})
      IF("${upper}" MATCHES "^PNG$")
        SET(PNG_INCLUDE_DIR ${PNG_PNG_INCLUDE_DIR})
        MARK_AS_ADVANCED(PNG_PNG_INCLUDE_DIR)
      ENDIF("${upper}" MATCHES "^PNG$")
    ELSE(${upper}_FOUND)
      SET(${CMAKE_PROJECT_NAME}_BUILD_LOCAL_${upper} ON)
      SET(${CMAKE_PROJECT_NAME}_${upper}_LIBRARIES ${lower})
    ENDIF(${upper}_FOUND)
  ELSE(NOT ${CMAKE_PROJECT_NAME}_BUILD_LOCAL_${upper})
    SET(${CMAKE_PROJECT_NAME}_${upper}_LIBRARIES ${lower})
  ENDIF(NOT ${CMAKE_PROJECT_NAME}_BUILD_LOCAL_${upper})
  MARK_AS_ADVANCED(${CMAKE_PROJECT_NAME}_BUILD_LOCAL_${upper})
ENDMACRO(THIRD_PARTY_OPTION)

#-----------------------------------------------------------------------------
MACRO(THIRD_PARTY_INCLUDE upper lower)
  IF(NOT ${CMAKE_PROJECT_NAME}_BUILD_LOCAL_${upper})
    IF(${upper}_INCLUDE_DIR)
      SET(${CMAKE_PROJECT_NAME}_INCLUDE_DIRS_SYSTEM ${${CMAKE_PROJECT_NAME}_INCLUDE_DIRS_SYSTEM} ${${upper}_INCLUDE_DIR})
    ENDIF(${upper}_INCLUDE_DIR)
  ELSE(NOT ${CMAKE_PROJECT_NAME}_BUILD_LOCAL_${upper})
    SET(${CMAKE_PROJECT_NAME}_INCLUDE_DIRS_SOURCE_TREE ${${CMAKE_PROJECT_NAME}_INCLUDE_DIRS_SOURCE_TREE}
      ${${CMAKE_PROJECT_NAME}_BINARY_DIR}/Utilities/${lower}
      ${${CMAKE_PROJECT_NAME}_SOURCE_DIR}/Utilities/${lower}
    )
  ENDIF(NOT ${CMAKE_PROJECT_NAME}_BUILD_LOCAL_${upper})
ENDMACRO(THIRD_PARTY_INCLUDE)

MACRO(THIRD_PARTY_INCLUDE2 upper)
  IF(NOT ${CMAKE_PROJECT_NAME}_BUILD_LOCAL_${upper})
    IF(${upper}_INCLUDE_DIR)
      SET(${CMAKE_PROJECT_NAME}_INCLUDE_DIRS_SYSTEM ${${CMAKE_PROJECT_NAME}_INCLUDE_DIRS_SYSTEM} ${${upper}_INCLUDE_DIR})
    ENDIF(${upper}_INCLUDE_DIR)
  ENDIF(NOT ${CMAKE_PROJECT_NAME}_BUILD_LOCAL_${upper})
ENDMACRO(THIRD_PARTY_INCLUDE2)

#-----------------------------------------------------------------------------
MACRO(THIRD_PARTY_SUBDIR upper lower)
  IF(${CMAKE_PROJECT_NAME}_BUILD_LOCAL_${upper})
    ADD_SUBDIRECTORY(${lower})
  ENDIF(${CMAKE_PROJECT_NAME}_BUILD_LOCAL_${upper})
ENDMACRO(THIRD_PARTY_SUBDIR)

#-----------------------------------------------------------------------------
MACRO(THIRD_PARTY_WARNING_SUPPRESS upper lang)
  IF(NOT ${upper}_WARNINGS_ALLOW)
    # MSVC uses /w to suppress warnings.  It also complains if another
    # warning level is given, so remove it.
    IF(MSVC)
      SET(${upper}_WARNINGS_BLOCKED 1)
      STRING(REGEX REPLACE "(^| )([/-])W[0-9]( |$)" " "
        CMAKE_${lang}_FLAGS "${CMAKE_${lang}_FLAGS}")
      SET(CMAKE_${lang}_FLAGS "${CMAKE_${lang}_FLAGS} /w")
    ENDIF(MSVC)

    # Borland uses -w- to suppress warnings.
    IF(BORLAND)
      SET(${upper}_WARNINGS_BLOCKED 1)
      SET(CMAKE_${lang}_FLAGS "${CMAKE_${lang}_FLAGS} -w-")
    ENDIF(BORLAND)

    # Most compilers use -w to suppress warnings.
    IF(NOT ${upper}_WARNINGS_BLOCKED)
      SET(CMAKE_${lang}_FLAGS "${CMAKE_${lang}_FLAGS} -w")
    ENDIF(NOT ${upper}_WARNINGS_BLOCKED)
  ENDIF(NOT ${upper}_WARNINGS_ALLOW)
ENDMACRO(THIRD_PARTY_WARNING_SUPPRESS)
