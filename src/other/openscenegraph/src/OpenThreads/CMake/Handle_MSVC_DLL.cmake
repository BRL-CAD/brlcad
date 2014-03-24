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

  SET_OUTPUT_DIR_PROPERTY_260(${LIB_NAME} "")        # Ensure the /Debug /Release are removed
  IF(NOT MSVC_IDE) 
    IF (NOT CMAKE24)
      BUILDER_VERSION_GREATER(2 8 0)
      IF(NOT VALID_BUILDER_VERSION)
	# If CMake < 2.8.1
	SET_TARGET_PROPERTIES(${LIB_NAME} PROPERTIES PREFIX "../bin/${LIB_PREFIX}${LIB_SOVERSION}-" IMPORT_PREFIX "../")
      ELSE(NOT VALID_BUILDER_VERSION)
	SET_TARGET_PROPERTIES(${LIB_NAME} PROPERTIES PREFIX "${LIB_PREFIX}${LIB_SOVERSION}-")
      ENDIF(NOT VALID_BUILDER_VERSION)
    ELSE (NOT CMAKE24)
      SET_TARGET_PROPERTIES(${LIB_NAME} PROPERTIES PREFIX "../bin/${LIB_PREFIX}${LIB_SOVERSION}-" IMPORT_PREFIX "../")
      SET(NEW_LIB_NAME "${OUTPUT_BINDIR}/${LIB_PREFIX}${LIB_SOVERSION}-${LIB_NAME}")
      ADD_CUSTOM_COMMAND(
	TARGET ${LIB_NAME}
	POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy "${NEW_LIB_NAME}.lib"  "${OUTPUT_LIBDIR}/${LIB_NAME}.lib"
	COMMAND ${CMAKE_COMMAND} -E copy "${NEW_LIB_NAME}.exp"  "${OUTPUT_LIBDIR}/${LIB_NAME}.exp"
	COMMAND ${CMAKE_COMMAND} -E remove "${NEW_LIB_NAME}.lib"
	COMMAND ${CMAKE_COMMAND} -E remove "${NEW_LIB_NAME}.exp"
	)
    ENDIF (NOT CMAKE24)
  ELSE(NOT MSVC_IDE)
    IF (NOT CMAKE24)
      BUILDER_VERSION_GREATER(2 8 0)
      IF(NOT VALID_BUILDER_VERSION)
	# If CMake < 2.8.1
	SET_TARGET_PROPERTIES(${LIB_NAME} PROPERTIES PREFIX "../../bin/${LIB_PREFIX}${LIB_SOVERSION}-" IMPORT_PREFIX "../")
      ELSE(NOT VALID_BUILDER_VERSION)
	SET_TARGET_PROPERTIES(${LIB_NAME} PROPERTIES PREFIX "${LIB_PREFIX}${LIB_SOVERSION}-")
      ENDIF(NOT VALID_BUILDER_VERSION)
    ELSE (NOT CMAKE24)
      SET_TARGET_PROPERTIES(${LIB_NAME} PROPERTIES PREFIX "../../bin/${LIB_PREFIX}${LIB_SOVERSION}-" IMPORT_PREFIX "../")
    ENDIF (NOT CMAKE24)
  ENDIF(NOT MSVC_IDE)
ENDMACRO(HANDLE_MSVC_DLL)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

