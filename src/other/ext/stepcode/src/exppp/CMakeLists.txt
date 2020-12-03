set(LIBEXPPP_SOURCES
  exppp.c
  pretty_alg.c
  pretty_case.c
  pretty_entity.c
  pretty_expr.c
  pretty_express.c
  pretty_func.c
  pretty_loop.c
  pretty_proc.c
  pretty_ref.c
  pretty_rule.c
  pretty_schema.c
  pretty_scope.c
  pretty_stmt.c
  pretty_subtype.c
  pretty_type.c
  pretty_where.c
 )

set(EXPPP_SOURCES
  ../express/fedex.c
  exppp-main.c
 )

include_directories(
  ${SC_SOURCE_DIR}/include
  ${SC_SOURCE_DIR}/include/exppp
  ${SC_SOURCE_DIR}/src/base
  ${SC_SOURCE_DIR}/src/express
 )

add_library(libexppp-obj OBJECT ${LIBEXPPP_SOURCES})
set_target_properties(libexppp-obj PROPERTIES PREFIX "")
set_property(TARGET libexppp-obj PROPERTY POSITION_INDEPENDENT_CODE ON)
add_dependencies(libexppp-obj base)
if(MSVC)
  set_property(TARGET libexppp-obj APPEND PROPERTY COMPILE_DEFINITIONS "SC_EXPPP_DLL_EXPORTS")
endif(MSVC)

add_library(libexppp SHARED $<TARGET_OBJECTS:libexppp-obj>)
target_link_libraries(libexppp base)
set_target_properties(libexppp PROPERTIES PREFIX "")
if(OPENBSD)
  set_target_properties(libexppp PROPERTIES VERSION ${SC_VERSION_MAJOR}.${SC_VERSION_MINOR})
else(OPENBSD)
  set_target_properties(libexppp PROPERTIES VERSION ${SC_VERSION} SOVERSION ${SC_VERSION_MAJOR})
endif(OPENBSD)
if(APPLE)
  set_property(TARGET libexppp APPEND PROPERTY LINK_FLAGS "-flat_namespace -undefined suppress")
endif(APPLE)
if(MSVC)
  set_property(TARGET libexppp APPEND PROPERTY COMPILE_DEFINITIONS "SC_EXPPP_DLL_EXPORTS")
  set_property(TARGET libexppp APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS "SC_EXPPP_DLL_IMPORTS")
endif(MSVC)
install(TARGETS libexppp
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR})

if (BUILD_STATIC_LIBS)
  add_library(libexppp-static STATIC $<TARGET_OBJECTS:libexppp-obj>)
  set_target_properties(libexppp-static PROPERTIES PREFIX "")
  install(TARGETS libexppp-static
    RUNTIME DESTINATION ${BIN_DIR}
    LIBRARY DESTINATION ${LIB_DIR}
    ARCHIVE DESTINATION ${LIB_DIR})
endif (BUILD_STATIC_LIBS)

add_executable(exppp ${EXPPP_SOURCES})
target_link_libraries(exppp libexppp express base)
install(TARGETS exppp
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR})

if(SC_ENABLE_TESTING)
  add_subdirectory(test)
endif(SC_ENABLE_TESTING)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

