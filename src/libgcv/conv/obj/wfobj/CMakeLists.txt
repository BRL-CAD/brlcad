set (LIBWFOBJ_INCLUDE_DIRS
    ${BRLCAD_BINARY_DIR}/include
    ${BRLCAD_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    )


BRLCAD_LIB_INCLUDE_DIRS(wfobj LIBWFOBJ_INCLUDE_DIRS CMAKE_CURRENT_BINARY_DIR)


PERPLEX_TARGET(WFOBJ_SCANNER obj_rules.l
  OUT_SRC_FILE obj_rules.cpp OUT_HDR_FILE obj_scanner.h)
LEMON_TARGET(WFOBJ_PARSER obj_grammar.yy
  OUT_SRC_FILE obj_grammar.cpp OUT_HDR_FILE obj_grammar.hpp)
ADD_PERPLEX_LEMON_DEPENDENCY(WFOBJ_SCANNER WFOBJ_PARSER)

DISTCLEAN(${PERPLEX_WFOBJ_SCANNER_INCLUDE_DIR})
DISTCLEAN(${LEMON_WFOBJ_PARSER_INCLUDE_DIR})

set(LIBWFOBJ_SOURCES
  ${LEMON_WFOBJ_PARSER_SRC}
  ${PERPLEX_WFOBJ_SCANNER_SRC}
  obj_parser.cpp
  tri_face.c
  )

BRLCAD_ADDLIB(libwfobj "${LIBWFOBJ_SOURCES}" "librt;libbu")
set_target_properties(libwfobj PROPERTIES VERSION 20.0.1 SOVERSION 20)

set_property(TARGET libwfobj APPEND PROPERTY INCLUDE_DIRECTORIES "${PERPLEX_WFOBJ_SCANNER_INCLUDE_DIR}")
set_property(TARGET libwfobj APPEND PROPERTY INCLUDE_DIRECTORIES "${LEMON_WFOBJ_PARSER_INCLUDE_DIR}")



set(wfobj_ignore_files
  obj_grammar_decls.h
  obj_grammar.yy
  obj_parser.h
  obj_parser_state.h
  obj_rules.h
  obj_rules.l
  obj_token_type.h
  tri_face.h
  )
CMAKEFILES(${wfobj_ignore_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
