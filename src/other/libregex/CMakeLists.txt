include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${${CMAKE_PROJECT_NAME}_SOURCE_DIR}/include)

set(LIBREGEX_PUBLIC_HDRS
  regex.h
  )

set(LIBREGEX_SOURCES
  regcomp.c
  regerror.c
  regexec.c
  regfree.c
  )

add_library(regex SHARED ${LIBREGEX_SOURCES})
set_target_properties(regex PROPERTIES VERSION 1.0.4 SOVERSION 1)
if(MSVC)
  set_property(TARGET regex APPEND PROPERTY COMPILE_DEFINITIONS "REGEX_EXPORT_DLL")
  set_property(TARGET regex APPEND PROPERTY COMPILE_DEFINITIONS "BRLCAD_DLL")
endif(MSVC)
install(TARGETS regex
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR})

if(BUILD_STATIC_LIBS)
  add_library(regex-static STATIC ${LIBREGEX_SOURCES})
  if(MSVC)
    # msvc does not append 'lib' - do it here to have consistent name
    set_target_properties(regex-static PROPERTIES PREFIX "lib")
  else(MSVC)
    set_target_properties(regex-static PROPERTIES OUTPUT_NAME "regex")
  endif(MSVC)
  install(TARGETS regex-static
    RUNTIME DESTINATION ${BIN_DIR}
    LIBRARY DESTINATION ${LIB_DIR}
    ARCHIVE DESTINATION ${LIB_DIR})
  if(NOT SKIP_INSTALL_LIBRARIES AND NOT SKIP_INSTALL_ALL)
    install(FILES ${LIBREGEX_PUBLIC_HDRS} DESTINATION include)
  endif(NOT SKIP_INSTALL_LIBRARIES AND NOT SKIP_INSTALL_ALL)
endif(BUILD_STATIC_LIBS)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

