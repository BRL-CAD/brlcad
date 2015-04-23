include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/wfobj
  ${CMAKE_CURRENT_BINARY_DIR}
  )

PERPLEX_TARGET(OBJ_Scanner wfobj/obj_rules.l
  ${CMAKE_CURRENT_BINARY_DIR}/obj_rules.cpp ${CMAKE_CURRENT_BINARY_DIR}/obj_scanner.h)
LEMON_TARGET(OBJ_Parser wfobj/obj_grammar.yy
  ${CMAKE_CURRENT_BINARY_DIR}/obj_grammar.cpp ${CMAKE_CURRENT_BINARY_DIR}/obj_grammar.hpp)
ADD_PERPLEX_LEMON_DEPENDENCY(OBJ_Scanner OBJ_Parser)

set(OBJ_G_SRCS
  ${CMAKE_CURRENT_BINARY_DIR}/obj_grammar.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/obj_rules.cpp
  wfobj/obj_parser.cpp
  ../conv/obj-g.c
  wfobj/tri_face.c
  )

BRLCAD_ADDEXEC(obj-g "${OBJ_G_SRCS}" "libbu;libbn;librt;libwdb" NO_STRICT_CXX)

set(wfobj_ignore_files
  wfobj/obj_grammar_decls.h
  wfobj/obj_grammar.yy
  wfobj/obj_parser.h
  wfobj/obj_parser_state.h
  wfobj/obj_rules.h
  wfobj/obj_rules.l
  wfobj/obj_token_type.h
  wfobj/tri_face.h
  )
CMAKEFILES(${wfobj_ignore_files})


# Include directories needed by libgcv users
set(GCV_INCLUDE_DIRS
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  )
BRLCAD_LIB_INCLUDE_DIRS(gcv GCV_INCLUDE_DIRS "")

set(LIBGCV_SOURCES
  bottess.c
  plugin.c
  region_end.c
  region_end_mc.c
  tri_intersect.c
  bot_solidity.c
  )

list(APPEND LIBGCV_SOURCES
  setup.cpp
  conv/brlcad.c
  conv/fastgen4/fastgen4_read.c
  conv/fastgen4/fastgen4_write.cpp
  conv/obj/obj_write.c
  conv/stl/stl_read.c
  conv/stl/stl_write.c
  conv/vrml/vrml_write.c

  conv/obj/obj_read.c
  wfobj/obj_parser.cpp
  wfobj/tri_face.c
  ${CMAKE_CURRENT_BINARY_DIR}/obj_grammar.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/obj_rules.cpp
  )

BRLCAD_ADDLIB(libgcv "${LIBGCV_SOURCES}" "librt;libwdb;libbu" NO_STRICT_CXX)
SET_TARGET_PROPERTIES(libgcv PROPERTIES VERSION 20.0.1 SOVERSION 20)

BRLCAD_ADDEXEC(gcv gcv.c libgcv)

enable_testing()
BRLCAD_ADDEXEC(test_bottess test_bottess.c libgcv NO_INSTALL)
if(CPP_DLL_DEFINES)
  SET_PROPERTY(TARGET test_bottess APPEND PROPERTY COMPILE_DEFINITIONS "BOTTESS_DLL_IMPORTS")
endif(CPP_DLL_DEFINES)
add_test(NAME bottess_test COMMAND test_bottess)

set(GCV_IGNORE_FILES
  NOTES
  conv/fastgen4/NOTES
  gcv_private.h
  plugin.h
  soup.h
  tri_intersect.h
  bot_solidity.h
  )
CMAKEFILES(${GCV_IGNORE_FILES})

DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/CTestTestfile.cmake)
DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/wfobj/CTestTestfile.cmake)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
