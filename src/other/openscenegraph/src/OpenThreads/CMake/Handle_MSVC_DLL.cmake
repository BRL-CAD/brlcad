# Sets the output directory property for CMake >= 2.8.0, giving an output path RELATIVE to default one
MACRO(SET_OUTPUT_DIR_PROPERTY_280 TARGET_TARGETNAME RELATIVE_OUTDIR)
  # Global properties (All generators but VS & Xcode)
  FILE(TO_CMAKE_PATH TMPVAR "CMAKE_ARCHIVE_OUTPUT_DIRECTORY/${RELATIVE_OUTDIR}")
  SET_TARGET_PROPERTIES(${TARGET_TARGETNAME} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${TMPVAR}")
  FILE(TO_CMAKE_PATH TMPVAR "CMAKE_RUNTIME_OUTPUT_DIRECTORY/${RELATIVE_OUTDIR}")
  SET_TARGET_PROPERTIES(${TARGET_TARGETNAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TMPVAR}")
  FILE(TO_CMAKE_PATH TMPVAR "CMAKE_LIBRARY_OUTPUT_DIRECTORY/${RELATIVE_OUTDIR}")
  SET_TARGET_PROPERTIES(${TARGET_TARGETNAME} PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${TMPVAR}")

  # Per-configuration property (VS, Xcode)
  FOREACH(CONF ${CMAKE_CONFIGURATION_TYPES})        # For each configuration (Debug, Release, MinSizeRel... and/or anything the user chooses)
    STRING(TOUPPER "${CONF}" CONF)                # Go uppercase (DEBUG, RELEASE...)

    # We use "FILE(TO_CMAKE_PATH", to create nice looking paths
    FILE(TO_CMAKE_PATH "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CONF}}/${RELATIVE_OUTDIR}" TMPVAR)
    SET_TARGET_PROPERTIES(${TARGET_TARGETNAME} PROPERTIES "ARCHIVE_OUTPUT_DIRECTORY_${CONF}" "${TMPVAR}")
    FILE(TO_CMAKE_PATH "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONF}}/${RELATIVE_OUTDIR}" TMPVAR)
    SET_TARGET_PROPERTIES(${TARGET_TARGETNAME} PROPERTIES "RUNTIME_OUTPUT_DIRECTORY_${CONF}" "${TMPVAR}")
    FILE(TO_CMAKE_PATH "${CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CONF}}/${RELATIVE_OUTDIR}" TMPVAR)
    SET_TARGET_PROPERTIES(${TARGET_TARGETNAME} PROPERTIES "LIBRARY_OUTPUT_DIRECTORY_${CONF}" "${TMPVAR}")
  ENDFOREACH(CONF ${CMAKE_CONFIGURATION_TYPES})
ENDMACRO(SET_OUTPUT_DIR_PROPERTY_280 TARGET_TARGETNAME RELATIVE_OUTDIR)


# Takes two optional arguments -- osg prefix and osg version
MACRO(HANDLE_MSVC_DLL)
  #this is a hack... the build place is set to lib/<debug or release> by LIBARARY_OUTPUT_PATH equal to OUTPUT_LIBDIR
  #the .lib will be crated in ../ so going straight in lib by the IMPORT_PREFIX property
  #because we want dll placed in OUTPUT_BINDIR ie the bin folder sibling of lib, we can use ../../bin to go there,
  #it is hardcoded, we should compute OUTPUT_BINDIR position relative to OUTPUT_LIBDIR ... to be implemented
  #changing bin to something else breaks this hack
  #the dll are versioned by prefixing the name with osg${OPENSCENEGRAPH_SOVERSION}-

  # LIB_PREFIX: use "osg" by default, else whatever we've been given.
  IF(${ARGC} GREATER 0)
    SET(LIB_PREFIX ${ARGV0})
  ELSE(${ARGC} GREATER 0)
    SET(LIB_PREFIX osg)
  ENDIF(${ARGC} GREATER 0)

  # LIB_SOVERSION: use OSG's soversion by default, else whatever we've been given
  IF(${ARGC} GREATER 1)
    SET(LIB_SOVERSION ${ARGV1})
  ELSE(${ARGC} GREATER 1)
    SET(LIB_SOVERSION ${OPENSCENEGRAPH_SOVERSION})
  ENDIF(${ARGC} GREATER 1)

  SET_OUTPUT_DIR_PROPERTY_280(${LIB_NAME} "")        # Ensure the /Debug /Release are removed
  SET_TARGET_PROPERTIES(${LIB_NAME} PROPERTIES PREFIX "${LIB_PREFIX}${LIB_SOVERSION}-")
ENDMACRO(HANDLE_MSVC_DLL)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

