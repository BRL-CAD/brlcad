#-----------------------------------------------------------------------------
MACRO(THIRD_PARTY_OPTION upper lower)
	IF(${CMAKE_PROJECT_NAME}-ENABLE_ALL_LOCAL_LIBS)
		OPTION(${CMAKE_PROJECT_NAME}_BUILD_LOCAL_${upper} "Build the local ${upper} library." ON)
		SET(${CMAKE_PROJECT_NAME}_BUILD_LOCAL_${upper} ON CACHE BOOL "Build the local ${upper} library." FORCE)
	ELSE(${CMAKE_PROJECT_NAME}-ENABLE_ALL_LOCAL_LIBS)
		OPTION(${CMAKE_PROJECT_NAME}_BUILD_LOCAL_${upper} "Build the local ${upper} library." OFF)
	ENDIF(${CMAKE_PROJECT_NAME}-ENABLE_ALL_LOCAL_LIBS)
	IF(NOT ${CMAKE_PROJECT_NAME}_BUILD_LOCAL_${upper} OR ${CMAKE_PROJECT_NAME}-ENABLE_SYSTEM_LIBS_ONLY)
		SET(${upper}_FOUND "${upper}-NOTFOUND" CACHE STRING "${upper}_FOUND" FORCE)
		MARK_AS_ADVANCED(${upper}_FOUND)
		SET(${upper}_LIBRARY "${upper}-NOTFOUND" CACHE STRING "${upper}_LIBRARY" FORCE)
		MARK_AS_ADVANCED(${upper}_LIBRARY)
		SET(${upper}_INCLUDE_DIR "${upper}-NOTFOUND" CACHE STRING "${upper}_INCLUDE_DIR" FORCE)
		MARK_AS_ADVANCED(${upper}_INCLUDE_DIR)
		IF(EXISTS ${${CMAKE_PROJECT_NAME}_CMAKE_DIR}/Find${upper}.cmake)
			INCLUDE(${${CMAKE_PROJECT_NAME}_CMAKE_DIR}/Find${upper}.cmake)
		ELSE(EXISTS ${${CMAKE_PROJECT_NAME}_CMAKE_DIR}/Find${upper}.cmake)
			INCLUDE(${CMAKE_ROOT}/Modules/Find${upper}.cmake)
		ENDIF(EXISTS ${${CMAKE_PROJECT_NAME}_CMAKE_DIR}/Find${upper}.cmake)
		IF(NOT ${upper}_FOUND)
			IF(NOT ${CMAKE_PROJECT_NAME}-ENABLE_SYSTEM_LIBS_ONLY) 
				SET(${CMAKE_PROJECT_NAME}_BUILD_LOCAL_${upper} ON CACHE BOOL "Build the local ${upper} library." FORCE)
				SET(${upper}_LIBRARY "${lower}" CACHE STRING "set by THIRD_PARTY macro" FORCE)
			ENDIF(NOT ${CMAKE_PROJECT_NAME}-ENABLE_SYSTEM_LIBS_ONLY) 
      ELSE(NOT ${upper}_FOUND)
		# We have to remove any previously built output from enabled local copies of the
		# library in question, or the linker will get confused - a system lib was found and
		# system libraries are to be preferred with current options.  This is unfortunate in
		# that it may introduce extra build work just from trying configure options, but appears
		# to be essential to ensuring that the build "just works" each time.
		STRING(REGEX REPLACE "lib" "" rootname "${lower}")
		FILE(GLOB STALE_FILES "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}${rootname}*${CMAKE_SHARED_LIBRARY_SUFFIX}*")
		MESSAGE("STALE_FILES: ${STALE_FILES}")
		FOREACH(stale_file ${STALE_FILES})
		   EXEC_PROGRAM(
			   ${CMAKE_COMMAND} ARGS -E remove ${stale_file}
					 OUTPUT_VARIABLE rm_out
					 RETURN_VALUE rm_retval
				 )
		ENDFOREACH(stale_file ${STALE_FILES})

		ENDIF(NOT ${upper}_FOUND)
	ELSE(NOT ${CMAKE_PROJECT_NAME}_BUILD_LOCAL_${upper} OR ${CMAKE_PROJECT_NAME}-ENABLE_SYSTEM_LIBS_ONLY)
		SET(${upper}_LIBRARY "${lower}" CACHE STRING "set by THIRD_PARTY macro" FORCE)
	ENDIF(NOT ${CMAKE_PROJECT_NAME}_BUILD_LOCAL_${upper} OR ${CMAKE_PROJECT_NAME}-ENABLE_SYSTEM_LIBS_ONLY)
	MARK_AS_ADVANCED(${CMAKE_PROJECT_NAME}_BUILD_LOCAL_${upper})
ENDMACRO(THIRD_PARTY_OPTION)

#-----------------------------------------------------------------------------
MACRO(THIRD_PARTY_SUBDIR upper lower)
	IF(${CMAKE_PROJECT_NAME}_BUILD_LOCAL_${upper} AND NOT ${CMAKE_PROJECT_NAME}-ENABLE_SYSTEM_LIBS_ONLY)
		ADD_SUBDIRECTORY(${lower})
		SET(${upper}_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/${lower};${CMAKE_CURRENT_BINARY_DIR}/${lower} CACHE STRING "set by THIRD_PARTY_SUBDIR macro" FORCE)
	ENDIF(${CMAKE_PROJECT_NAME}_BUILD_LOCAL_${upper} AND NOT ${CMAKE_PROJECT_NAME}-ENABLE_SYSTEM_LIBS_ONLY)
ENDMACRO(THIRD_PARTY_SUBDIR)

#-----------------------------------------------------------------------------

include(ExternalProject)

MACRO(THIRD_PARTY_NMAKE_EXTERNAL_PROJECT upper projname projpath srcpath extraopts)
	IF(${CMAKE_PROJECT_NAME}_BUILD_LOCAL_${upper} AND NOT ${CMAKE_PROJECT_NAME}-ENABLE_SYSTEM_LIBS_ONLY)
		ExternalProject_Add(
			${projname}
			DOWNLOAD_COMMAND ""
			PREFIX ${${CMAKE_PROJECT_NAME}_SOURCE_DIR}/${projpath}/${srcpath}
			SOURCE_DIR ${${CMAKE_PROJECT_NAME}_SOURCE_DIR}/${projpath}/${srcpath}
			CONFIGURE_COMMAND ""
			BUILD_COMMAND cd <SOURCE_DIR> && nmake -f makefile.vc INSTALLDIR=${CMAKE_INSTALL_PREFIX} ${extraopts}
			INSTALL_COMMAND  cd <SOURCE_DIR> && nmake -f makefile.vc INSTALLDIR=${CMAKE_INSTALL_PREFIX} ${extraopts} install
			)
		SET(CMAKE_EXTERNAL_TARGET_LIST "${CMAKE_EXTERNAL_TARGET_LIST};${projname}" CACHE STRING "external target list" FORCE)
	ENDIF(${CMAKE_PROJECT_NAME}_BUILD_LOCAL_${upper} AND NOT ${CMAKE_PROJECT_NAME}-ENABLE_SYSTEM_LIBS_ONLY)
ENDMACRO(THIRD_PARTY_NMAKE_EXTERNAL_PROJECT)


MACRO(THIRD_PARTY_CONFIGURE_EXTERNAL_PROJECT upper projname projpath srcpath extraopts)
	IF(${CMAKE_PROJECT_NAME}_BUILD_LOCAL_${upper} AND NOT ${CMAKE_PROJECT_NAME}-ENABLE_SYSTEM_LIBS_ONLY)
		ExternalProject_Add(
			${projname}
			DOWNLOAD_COMMAND ""
			PREFIX ${${CMAKE_PROJECT_NAME}_BINARY_DIR}
			SOURCE_DIR ${${CMAKE_PROJECT_NAME}_SOURCE_DIR}/${projpath}/${srcpath}
			CONFIGURE_COMMAND mkdir -p
			${${CMAKE_PROJECT_NAME}_BINARY_DIR}/${projpath}/ && cd
			${${CMAKE_PROJECT_NAME}_BINARY_DIR}/${projpath}/ &&
			<SOURCE_DIR>/configure --prefix=${${CMAKE_PROJECT_NAME}_PREFIX} --exec-prefix=${${CMAKE_PROJECT_NAME}_PREFIX} --mandir=${${CMAKE_PROJECT_NAME}_INSTALL_MAN_DIR} ${extraopts}
			BUILD_COMMAND cd ${${CMAKE_PROJECT_NAME}_BINARY_DIR}/${projpath}/ && $(MAKE)
			INSTALL_COMMAND  cd ${${CMAKE_PROJECT_NAME}_BINARY_DIR}/${projpath}/ && $(MAKE) install
			)
		SET(CMAKE_EXTERNAL_TARGET_LIST "${CMAKE_EXTERNAL_TARGET_LIST};${projname}" CACHE STRING "external target list" FORCE)
	ENDIF(${CMAKE_PROJECT_NAME}_BUILD_LOCAL_${upper} AND NOT ${CMAKE_PROJECT_NAME}-ENABLE_SYSTEM_LIBS_ONLY)
ENDMACRO(THIRD_PARTY_CONFIGURE_EXTERNAL_PROJECT)

MACRO(THIRD_PARTY_AUTOCONF_EXTERNAL_PROJECT upper projname projpath srcpath extraopts)
	IF(${CMAKE_PROJECT_NAME}_BUILD_LOCAL_${upper} AND NOT ${CMAKE_PROJECT_NAME}-ENABLE_SYSTEM_LIBS_ONLY)
		ExternalProject_Add(
			${projname}
			DOWNLOAD_COMMAND ""
			PREFIX ${${CMAKE_PROJECT_NAME}_BINARY_DIR}
			SOURCE_DIR ${${CMAKE_PROJECT_NAME}_SOURCE_DIR}/${projpath}/${srcpath}
			CONFIGURE_COMMAND autoconf -I <SOURCE_DIR> -o
			<SOURCE_DIR>/configure <SOURCE_DIR>/configure.in && mkdir -p
			${${CMAKE_PROJECT_NAME}_BINARY_DIR}/${projpath}/ && cd
			${${CMAKE_PROJECT_NAME}_BINARY_DIR}/${projpath}/ &&
			<SOURCE_DIR>/configure --prefix=${${CMAKE_PROJECT_NAME}_PREFIX} --exec-prefix=${${CMAKE_PROJECT_NAME}_PREFIX} --mandir=${${CMAKE_PROJECT_NAME}_INSTALL_MAN_DIR} ${extraopts}
			BUILD_COMMAND cd ${${CMAKE_PROJECT_NAME}_BINARY_DIR}/${projpath}/ && $(MAKE)
			INSTALL_COMMAND  cd ${${CMAKE_PROJECT_NAME}_BINARY_DIR}/${projpath}/ && $(MAKE) install
			)
		SET(CMAKE_EXTERNAL_TARGET_LIST "${CMAKE_EXTERNAL_TARGET_LIST};${projname}" CACHE STRING "external target list" FORCE)
	ENDIF(${CMAKE_PROJECT_NAME}_BUILD_LOCAL_${upper} AND NOT ${CMAKE_PROJECT_NAME}-ENABLE_SYSTEM_LIBS_ONLY)
ENDMACRO(THIRD_PARTY_AUTOCONF_EXTERNAL_PROJECT)


MACRO(THIRD_PARTY_AUTORECONF_EXTERNAL_PROJECT upper projname projpath srcpath extraopts)
	IF(${CMAKE_PROJECT_NAME}_BUILD_LOCAL_${upper} AND NOT ${CMAKE_PROJECT_NAME}-ENABLE_SYSTEM_LIBS_ONLY)
		ExternalProject_Add(
			${projname}
			DOWNLOAD_COMMAND ""
			PREFIX ${${CMAKE_PROJECT_NAME}_BINARY_DIR}
			SOURCE_DIR ${${CMAKE_PROJECT_NAME}_SOURCE_DIR}/${projpath}/${srcpath}
			CONFIGURE_COMMAND cd <SOURCE_DIR> && autoreconf -i -f && mkdir -p
			${${CMAKE_PROJECT_NAME}_BINARY_DIR}/${projpath}/ && cd
			${${CMAKE_PROJECT_NAME}_BINARY_DIR}/${projpath}/ &&
			<SOURCE_DIR>/configure --prefix=${${CMAKE_PROJECT_NAME}_PREFIX} --exec-prefix=${${CMAKE_PROJECT_NAME}_PREFIX} --mandir=${${CMAKE_PROJECT_NAME}_INSTALL_MAN_DIR} ${extraopts}
			BUILD_COMMAND cd ${${CMAKE_PROJECT_NAME}_BINARY_DIR}/${projpath}/ && $(MAKE)
			INSTALL_COMMAND  cd ${${CMAKE_PROJECT_NAME}_BINARY_DIR}/${projpath}/ && $(MAKE) install
			)
		SET(CMAKE_EXTERNAL_TARGET_LIST "${CMAKE_EXTERNAL_TARGET_LIST};${projname}" CACHE STRING "external target list" FORCE)
	ENDIF(${CMAKE_PROJECT_NAME}_BUILD_LOCAL_${upper} AND NOT ${CMAKE_PROJECT_NAME}-ENABLE_SYSTEM_LIBS_ONLY)
ENDMACRO(THIRD_PARTY_AUTORECONF_EXTERNAL_PROJECT)


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
