include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BOOST_INCLUDE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
)

PERPLEX_TARGET(OBJ_Scanner obj_rules.l ${CMAKE_CURRENT_BINARY_DIR}/obj_rules.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/obj_scanner.h)
LEMON_TARGET(OBJ_Parser obj_grammar.yy obj_grammar.cpp obj_grammar.hpp)
ADD_PERPLEX_LEMON_DEPENDENCY(OBJ_Scanner OBJ_Parser)

set(OBJ_G_SRCS
	${CMAKE_CURRENT_BINARY_DIR}/obj_grammar.cpp
	${CMAKE_CURRENT_BINARY_DIR}/obj_rules.cpp
	obj_parser.cpp
	../../conv/obj-g.c
	tri_face.c
)

BRLCAD_ADDEXEC(obj-g "${OBJ_G_SRCS}" "libbu;libbn;librt;libwdb" NO_STRICT_CXX)

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
