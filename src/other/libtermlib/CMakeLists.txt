cmake_minimum_required(VERSION 2.8)
if(COMMAND CMAKE_POLICY)
  cmake_policy(SET CMP0003 NEW)
endif(COMMAND CMAKE_POLICY)

project(LIBTERMLIB)


if(NOT MSVC)

  if(NOT CONF_DIR)
    set(CONF_DIR etc)
  endif(NOT CONF_DIR)

  set(LIBTERMLIB_PRIVATE_HDRS
    termcap.h
    )


  set(LIBTERM_SOURCES
    termcap.c
    tgoto.c
    tputs.c
    )

  add_definitions(
    -DCM_N
    -DCM_GT
    -DCM_B
    -DCM_D
    -DB_TERMCAP=\"${CMAKE_INSTALL_PREFIX}/${CONF_DIR}/termcap\"
    )

  add_library(termlib SHARED ${LIBTERM_SOURCES})
  set_target_properties(termlib PROPERTIES VERSION 1.7.1 SOVERSION 1)
  install(TARGETS termlib
    RUNTIME DESTINATION ${BIN_DIR}
    LIBRARY DESTINATION ${LIB_DIR}
    ARCHIVE DESTINATION ${LIB_DIR})

  add_library(termlib-static STATIC ${LIBTERM_SOURCES})
  install(TARGETS termlib-static
    RUNTIME DESTINATION ${BIN_DIR}
    LIBRARY DESTINATION ${LIB_DIR}
    ARCHIVE DESTINATION ${LIB_DIR})

  set_target_properties(termlib-static PROPERTIES OUTPUT_NAME "termlib")

  if (NOT CMAKE_CONFIGURATION_TYPES)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/termcap.src ${CMAKE_BINARY_DIR}/${CONF_DIR}/termcap COPYONLY)
    install(FILES ${CMAKE_BINARY_DIR}/${CONF_DIR}/termcap DESTINATION ${CONF_DIR})
  else (NOT CMAKE_CONFIGURATION_TYPES)
    foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
      string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
      configure_file(${CMAKE_CURRENT_SOURCE_DIR}/termcap.src ${CMAKE_BINARY_DIR}/${CFG_TYPE_UPPER}/${DATA_DIR}/etc/termcap COPYONLY)
    endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    install(FILES ${CMAKE_BINARY_DIR}/\${BUILD_TYPE}/${CONF_DIR}/termcap DESTINATION ${CONF_DIR})
  endif (NOT CMAKE_CONFIGURATION_TYPES)


endif(NOT MSVC)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
