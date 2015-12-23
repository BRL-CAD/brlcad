# XXX: This is currently unused.

set (LIBWFOBJ_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${BRLCAD_BINARY_DIR}/include
    ${BRLCAD_SOURCE_DIR}/include
    )

BRLCAD_LIB_INCLUDE_DIRS(wfobj LIBWFOBJ_INCLUDE_DIRS CMAKE_CURRENT_BINARY_DIR)

PERPLEX_TARGET(WFOBJ_SCANNER wfobj_rules.l
  OUT_SRC_FILE wfobj_rules.cpp OUT_HDR_FILE wfobj_scanner.h)
LEMON_TARGET(WFOBJ_PARSER obj_grammar.yy
  OUT_SRC_FILE wfobj_grammar.cpp OUT_HDR_FILE wfobj_grammar.hpp)
ADD_PERPLEX_LEMON_DEPENDENCY(WFOBJ_SCANNER WFOBJ_PARSER)

DISTCLEAN(${PERPLEX_WFOBJ_SCANNER_INCLUDE_DIR})
DISTCLEAN(${LEMON_WFOBJ_PARSER_INCLUDE_DIR})

set(LIBWFOBJ_SOURCES
  ${LEMON_WFOBJ_PARSER_SRC}
  ${PERPLEX_WFOBJ_SCANNER_SRC}
  wfobj_parser.cpp
  tri_face.c
  )

BRLCAD_ADDLIB(libwfobj "${LIBWFOBJ_SOURCES}" "")
set_target_properties(libwfobj PROPERTIES VERSION 20.0.1 SOVERSION 20)

set(wfobj_ignore_files
  wfobj_grammar_decls.h
  wfobj_grammar.yy
  wfobj_parser.h
  wfobj_parser_state.h
  wfobj_rules.h
  wfobj_rules.l
  wfobj_token_type.h
  wfobj_tri_face.h
  )
CMAKEFILES(${wfobj_ignore_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
