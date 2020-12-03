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

SET(EXPPP_SOURCES
  ../express/fedex.c
  exppp-main.c
 )

include_directories(
  ${SC_SOURCE_DIR}/include
  ${SC_SOURCE_DIR}/include/exppp
  ${SC_SOURCE_DIR}/src/base
  ${SC_SOURCE_DIR}/src/express
 )

if($CACHE{SC_BUILD_SHARED_LIBS})
  SC_ADDLIB(libexppp SHARED SOURCES ${LIBEXPPP_SOURCES} LINK_LIBRARIES express base)
  set_target_properties(libexppp PROPERTIES PREFIX "")
  if(WIN32)
    target_compile_definitions(libexppp PRIVATE SC_EXPPP_DLL_EXPORTS)
  endif()
endif()

if($CACHE{SC_BUILD_STATIC_LIBS})
  SC_ADDLIB(libexppp-static STATIC SOURCES ${LIBEXPPP_SOURCES} LINK_LIBRARIES express-static base-static)
  set_target_properties(libexppp-static PROPERTIES PREFIX "")
endif()

SC_ADDEXEC(exppp SOURCES ${EXPPP_SOURCES} LINK_LIBRARIES libexppp express base)

if(SC_ENABLE_TESTING)
  add_subdirectory(test)
endif(SC_ENABLE_TESTING)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

