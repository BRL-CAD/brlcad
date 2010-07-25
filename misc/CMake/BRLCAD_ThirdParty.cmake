#-----------------------------------------------------------------------------
MACRO(BRLCAD_THIRD_PARTY_OPTION upper lower)
  OPTION(BRLCAD_USE_SYSTEM_${upper} "Use the system's ${lower} library." OFF)
  MARK_AS_ADVANCED(BRLCAD_USE_SYSTEM_${upper})
  IF(BRLCAD_USE_SYSTEM_${upper})
    IF(EXISTS ${CMAKE_ROOT}/Modules/Find${upper}.cmake)
      INCLUDE(${CMAKE_ROOT}/Modules/Find${upper}.cmake)
    ELSE(EXISTS ${CMAKE_ROOT}/Modules/Find${upper}.cmake)
      INCLUDE(${BRLCAD_CMAKE_DIR}/Find${upper}.cmake)
    ENDIF(EXISTS ${CMAKE_ROOT}/Modules/Find${upper}.cmake)
    MARK_AS_ADVANCED(${upper}_INCLUDE_DIR ${upper}_LIBRARY)
    IF(${upper}_FOUND)
      SET(BRLCAD_${upper}_LIBRARIES ${${upper}_LIBRARIES})
      IF("${upper}" MATCHES "^PNG$")
        SET(PNG_INCLUDE_DIR ${PNG_PNG_INCLUDE_DIR})
        MARK_AS_ADVANCED(PNG_PNG_INCLUDE_DIR)
      ENDIF("${upper}" MATCHES "^PNG$")
    ELSE(${upper}_FOUND)
      MESSAGE(SEND_ERROR "BRLCAD_USE_SYSTEM_${upper} is ON, but ${upper}_LIBRARY is NOTFOUND.")
    ENDIF(${upper}_FOUND)
  ELSE(BRLCAD_USE_SYSTEM_${upper})
    SET(BRLCAD_${upper}_LIBRARIES ${lower})
  ENDIF(BRLCAD_USE_SYSTEM_${upper})
ENDMACRO(BRLCAD_THIRD_PARTY_OPTION)

#-----------------------------------------------------------------------------
MACRO(BRLCAD_THIRD_PARTY_INCLUDE upper lower)
  IF(BRLCAD_USE_SYSTEM_${upper})
    IF(${upper}_INCLUDE_DIR)
      SET(BRLCAD_INCLUDE_DIRS_SYSTEM ${BRLCAD_INCLUDE_DIRS_SYSTEM} ${${upper}_INCLUDE_DIR})
    ENDIF(${upper}_INCLUDE_DIR)
  ELSE(BRLCAD_USE_SYSTEM_${upper})
    SET(BRLCAD_INCLUDE_DIRS_SOURCE_TREE ${BRLCAD_INCLUDE_DIRS_SOURCE_TREE}
      ${BRLCAD_BINARY_DIR}/Utilities/${lower}
      ${BRLCAD_SOURCE_DIR}/Utilities/${lower}
    )
  ENDIF(BRLCAD_USE_SYSTEM_${upper})
ENDMACRO(BRLCAD_THIRD_PARTY_INCLUDE)

MACRO(BRLCAD_THIRD_PARTY_INCLUDE2 upper)
  IF(BRLCAD_USE_SYSTEM_${upper})
    IF(${upper}_INCLUDE_DIR)
      SET(BRLCAD_INCLUDE_DIRS_SYSTEM ${BRLCAD_INCLUDE_DIRS_SYSTEM} ${${upper}_INCLUDE_DIR})
    ENDIF(${upper}_INCLUDE_DIR)
  ENDIF(BRLCAD_USE_SYSTEM_${upper})
ENDMACRO(BRLCAD_THIRD_PARTY_INCLUDE2)

#-----------------------------------------------------------------------------
MACRO(BRLCAD_THIRD_PARTY_SUBDIR upper lower)
  IF(NOT BRLCAD_USE_SYSTEM_${upper})
    SUBDIRS(${lower})
  ENDIF(NOT BRLCAD_USE_SYSTEM_${upper})
ENDMACRO(BRLCAD_THIRD_PARTY_SUBDIR)

#-----------------------------------------------------------------------------
MACRO(BRLCAD_THIRD_PARTY_WARNING_SUPPRESS upper lang)
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
ENDMACRO(BRLCAD_THIRD_PARTY_WARNING_SUPPRESS)
