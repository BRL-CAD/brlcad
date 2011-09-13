include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../src/other/boost
  ${CMAKE_CURRENT_BINARY_DIR}
)

LEMON_TARGET(OBJ_Parser obj_grammar.yy ${CMAKE_CURRENT_BINARY_DIR}/obj_grammar.cpp)
RE2C_TARGET(OBJ_Scanner obj_rules.re ${CMAKE_CURRENT_BINARY_DIR}/obj_rules.cpp COMPILE_FLAGS "-Fc")
ADD_RE2C_LEMON_DEPENDENCY(OBJ_Scanner OBJ_Parser)

SET(OBJ_G_SRCS
	${CMAKE_CURRENT_BINARY_DIR}/obj_grammar.cpp
	${CMAKE_CURRENT_BINARY_DIR}/obj_rules.cpp
	re2c_utils.c
	obj_parser.cpp
	../../conv/obj-g_new.c
)

configure_file(obj_grammar.h.in ${CMAKE_CURRENT_BINARY_DIR}/obj_grammar.h COPY_ONLY)

BRLCAD_ADDEXEC(obj-g "${OBJ_G_SRCS}" "libbu libbn libwdb")

set(wfobj_ignore_files
	Makefile.sample
	obj_grammar.h.in
	obj_grammar.yy
	obj_parser.h
	obj_parser_state.h
	obj_rules.h
	obj_rules.re
	obj_token_type.h
	re2c_utils.h
	)
CMAKEFILES(${wfobj_ignore_files})
CMAKEFILES(Makefile.am)
