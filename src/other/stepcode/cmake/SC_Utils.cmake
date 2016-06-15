
# set compile definitions for dll exports on windows
MACRO(DEFINE_DLL_EXPORTS libname)
    IF( MSVC OR BORLAND )
        if( ${libname} MATCHES "sdai_.*" )
            set( export "SC_SCHEMA_DLL_EXPORTS" )
        else()
            STRING(REGEX REPLACE "lib" "" shortname "${libname}")
            STRING(REGEX REPLACE "step" "" LOWERCORE "${shortname}")
            STRING(TOUPPER ${LOWERCORE} UPPER_CORE)
            set( export "SC_${UPPER_CORE}_DLL_EXPORTS" )
        endif()
        get_target_property(defs ${libname} COMPILE_DEFINITIONS )
        if( defs ) #if no properties, ${defs} will be defs-NOTFOUND which CMake interprets as false
            set( defs "${defs};${export}")
        else( defs )
            set( defs "${export}")
        endif( defs )
        set_target_properties(${libname} PROPERTIES COMPILE_DEFINITIONS "${defs}" )
    ENDIF( MSVC OR BORLAND )
ENDMACRO(DEFINE_DLL_EXPORTS libname)

# set compile definitions for dll imports on windows
MACRO( DEFINE_DLL_IMPORTS tgt libs )
    IF( MSVC OR BORLAND )
        get_target_property(defs ${tgt} COMPILE_DEFINITIONS )
        if( NOT defs ) #if no properties, ${defs} will be defs-NOTFOUND which CMake interprets as false
            set( defs "")
        endif( NOT defs )
        foreach( lib ${libs} )
            STRING(REGEX REPLACE "lib" "" shortname "${lib}")
            STRING(REGEX REPLACE "step" "" LOWERCORE "${shortname}")
            STRING(TOUPPER ${LOWERCORE} UPPER_CORE)
            list( APPEND defs "SC_${UPPER_CORE}_DLL_IMPORTS" )
        endforeach( lib ${libs} )
        if( DEFINED defs )
            if( defs )
                set_target_properties( ${tgt} PROPERTIES COMPILE_DEFINITIONS "${defs}" )
            endif( defs )
        endif( DEFINED defs )
    ENDIF( MSVC OR BORLAND )
ENDMACRO( DEFINE_DLL_IMPORTS tgt libs )

#EXCLUDE_OR_INSTALL(target destination ARGV3)
# installs ${target} in ${destination} unless testing is enabled AND ${arg_3} == "TESTABLE",
# in which case the EXCLUDE_FROM_ALL property is set for testing.
# EXCLUDE_FROM_ALL cannot be set on targets that are to be installed,
# so either test the target or install it - but not both
MACRO(EXCLUDE_OR_INSTALL target dest arg_3 )
    if( NOT ( ( SC_ENABLE_TESTING ) AND ( "${arg_3}" STREQUAL "TESTABLE" ) ) )
        INSTALL(TARGETS ${target}
	       	RUNTIME DESTINATION ${BIN_DIR}
	       	LIBRARY DESTINATION ${LIB_DIR}
	       	ARCHIVE DESTINATION ${LIB_DIR}
		)
    else( NOT ( ( SC_ENABLE_TESTING ) AND ( "${arg_3}" STREQUAL "TESTABLE" ) ) )
        set_target_properties( ${target} PROPERTIES EXCLUDE_FROM_ALL ON )
    endif( NOT ( ( SC_ENABLE_TESTING ) AND ( "${arg_3}" STREQUAL "TESTABLE" ) ) )
ENDMACRO(EXCLUDE_OR_INSTALL target dest arg_3 )

#SC_ADDEXEC( execname "source files" "linked libs" ["TESTABLE"] ["MSVC flag" ...])
# optional 4th argument of "TESTABLE", passed to EXCLUDE_OR_INSTALL macro
# optional args can also be used by MSVC-specific code, but it looks like these two uses
# will not conflict because the MSVC args must contain "STRICT"
MACRO(SC_ADDEXEC execname srcslist libslist)
    add_executable(${execname} ${srcslist})
    target_link_libraries(${execname} ${libslist})
    DEFINE_DLL_IMPORTS(${execname} "${libslist}")  #add import definitions for all libs that the executable is linked to
    EXCLUDE_OR_INSTALL( ${execname} "bin" "${ARGV3}" )
    # Enable extra compiler flags if local executables and/or global options dictate
    SET(LOCAL_COMPILE_FLAGS "")
    FOREACH(extraarg ${ARGN})
        IF(${extraarg} MATCHES "STRICT" AND SC-ENABLE_STRICT)
            SET(LOCAL_COMPILE_FLAGS "${LOCAL_COMPILE_FLAGS} ${STRICT_FLAGS}")
        ENDIF(${extraarg} MATCHES "STRICT" AND SC-ENABLE_STRICT)
    ENDFOREACH(extraarg ${ARGN})
    IF(LOCAL_COMPILE_FLAGS)
        SET_TARGET_PROPERTIES(${execname} PROPERTIES COMPILE_FLAGS ${LOCAL_COMPILE_FLAGS})
    ENDIF(LOCAL_COMPILE_FLAGS)
ENDMACRO(SC_ADDEXEC execname srcslist libslist)

#SC_ADDLIB( libname "source files" "linked libs" ["TESTABLE"] ["MSVC flag" ...])
# optional 4th argument of "TESTABLE", passed to EXCLUDE_OR_INSTALL macro
# optional args can also be used by MSVC-specific code, but it looks like these two uses
# will not conflict because the MSVC args must contain "STRICT"
MACRO(SC_ADDLIB libname srcslist libslist)
  STRING(REGEX REPLACE "-framework;" "-framework " libslist "${libslist1}")
  IF(SC_BUILD_SHARED_LIBS)
      add_library(${libname} SHARED ${srcslist})
      DEFINE_DLL_EXPORTS(${libname})
      if(NOT "${libs}" MATCHES "NONE")
          target_link_libraries(${libname} ${libslist})
          DEFINE_DLL_IMPORTS(${libname} "${libslist}" )
      endif(NOT "${libs}" MATCHES "NONE")
      SET_TARGET_PROPERTIES(${libname} PROPERTIES VERSION ${SC_ABI_VERSION} SOVERSION ${SC_ABI_SOVERSION} )
      EXCLUDE_OR_INSTALL( ${libname} "lib" "${ARGV3}" )
    if(APPLE)
        set_target_properties(${libname} PROPERTIES LINK_FLAGS "-flat_namespace -undefined suppress")
    endif(APPLE)
  ENDIF(SC_BUILD_SHARED_LIBS)
  IF(SC_BUILD_STATIC_LIBS AND NOT MSVC)
      add_library(${libname}-static STATIC ${srcslist})
      DEFINE_DLL_EXPORTS(${libname}-static)
      if(NOT ${libs} MATCHES "NONE")
          target_link_libraries(${libname}-static "${libslist}")
          DEFINE_DLL_IMPORTS(${libname}-static ${libslist} )
      endif(NOT ${libs} MATCHES "NONE")
      IF(NOT WIN32)
          SET_TARGET_PROPERTIES(${libname}-static PROPERTIES OUTPUT_NAME "${libname}")
      ENDIF(NOT WIN32)
      IF(WIN32)
          # We need the lib prefix on win32, so add it even if our add_library
          # wrapper function has removed it due to the target name - see
          # http://www.cmake.org/Wiki/CMake_FAQ#How_do_I_make_my_shared_and_static_libraries_have_the_same_root_name.2C_but_different_suffixes.3F
          SET_TARGET_PROPERTIES(${libname}-static PROPERTIES PREFIX "lib")
      ENDIF(WIN32)
      EXCLUDE_OR_INSTALL( ${libname}-static "lib" "${ARGV3}" )
    if(APPLE)
        set_target_properties(${libname}-static PROPERTIES LINK_FLAGS "-flat_namespace -undefined suppress")
    endif(APPLE)
  ENDIF(SC_BUILD_STATIC_LIBS AND NOT MSVC)
  # Enable extra compiler flags if local libraries and/or global options dictate
  SET(LOCAL_COMPILE_FLAGS "")
  FOREACH(extraarg ${ARGN})
      IF(${extraarg} MATCHES "STRICT" AND SC-ENABLE_STRICT)
          SET(LOCAL_COMPILE_FLAGS "${LOCAL_COMPILE_FLAGS} ${STRICT_FLAGS}")
      ENDIF(${extraarg} MATCHES "STRICT" AND SC-ENABLE_STRICT)
  ENDFOREACH(extraarg ${ARGN})
  IF(LOCAL_COMPILE_FLAGS)
      IF(BUILD_SHARED_LIBS)
          SET_TARGET_PROPERTIES(${libname} PROPERTIES COMPILE_FLAGS ${LOCAL_COMPILE_FLAGS})
      ENDIF(BUILD_SHARED_LIBS)
      IF(BUILD_STATIC_LIBS AND NOT MSVC)
          SET_TARGET_PROPERTIES(${libname}-static PROPERTIES COMPILE_FLAGS ${LOCAL_COMPILE_FLAGS})
      ENDIF(BUILD_STATIC_LIBS AND NOT MSVC)
  ENDIF(LOCAL_COMPILE_FLAGS)
ENDMACRO(SC_ADDLIB libname srcslist libslist)
