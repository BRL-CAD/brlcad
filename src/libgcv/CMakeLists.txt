include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/wfobj
  ${CMAKE_CURRENT_BINARY_DIR}
  )

# For parallel safety, need separate names and files for each target.
# Should probably rework this whole perplex/lemon combo into one macro,
# since this will be the common pattern.
macro(GEN_OBJ_SRCS targetname)
  file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/OBJ_${targetname}_SRCS)
  PERPLEX_TARGET(OBJ_${targetname}_Scanner wfobj/obj_rules.l
    OUT_SRC_FILE obj_${targetname}_rules.cpp OUT_HDR_FILE obj_scanner.h)
  LEMON_TARGET(OBJ_${targetname}_Parser wfobj/obj_grammar.yy
    OUT_SRC_FILE obj_${targetname}_grammar.cpp OUT_HDR_FILE obj_grammar.hpp)
  ADD_PERPLEX_LEMON_DEPENDENCY(OBJ_${targetname}_Scanner OBJ_${targetname}_Parser)
  DISTCLEAN(${PERPLEX_OBJ_${targetname}_Scanner_INCLUDE_DIR})
  DISTCLEAN(${LEMON_OBJ_${targetname}_Parser_INCLUDE_DIR})
endmacro(GEN_OBJ_SRCS)

GEN_OBJ_SRCS(obj-g)
set(OBJ_G_SRCS
  ${LEMON_OBJ_obj-g_Parser_SRC}
  ${PERPLEX_OBJ_obj-g_Scanner_SRC}
  wfobj/obj_parser.cpp
  ../conv/obj-g.c
  wfobj/tri_face.c
  )

BRLCAD_ADDEXEC(obj-g "${OBJ_G_SRCS}" "libbu;libbn;librt;libwdb")
set_property(TARGET obj-g APPEND PROPERTY INCLUDE_DIRECTORIES "${PERPLEX_OBJ_obj-g_Scanner_INCLUDE_DIR}")
set_property(TARGET obj-g APPEND PROPERTY INCLUDE_DIRECTORIES "${LEMON_OBJ_obj-g_Parser_INCLUDE_DIR}")

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


GEN_OBJ_SRCS(libgcv)
set(GCV_SO_SRCS
  ${LEMON_OBJ_libgcv_Parser_SRC}
  ${PERPLEX_OBJ_libgcv_Scanner_SRC}
  )
GEN_OBJ_SRCS(libgcv-static)
set(GCV_STATIC_SRCS
  ${LEMON_OBJ_libgcv-static_Parser_SRC}
  ${PERPLEX_OBJ_libgcv-static_Scanner_SRC}
  )
set(LIBGCV_SOURCES
  bottess.c
  facetize.c
  filter.c
  region_end.c
  region_end_mc.c
  tri_intersect.c
  bot_solidity.c
  )

set(LIBGCV_PLUGIN_SOURCES
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
  )

list(APPEND LIBGCV_SOURCES ${LIBGCV_PLUGIN_SOURCES})

BRLCAD_ADDLIB(libgcv "${LIBGCV_SOURCES}" "librt;libwdb;libbu" SO_SRCS "${GCV_SO_SRCS}" STATIC_SRCS "${GCV_STATIC_SRCS}")
set_target_properties(libgcv PROPERTIES VERSION 20.0.1 SOVERSION 20)
set_property(TARGET libgcv APPEND PROPERTY INCLUDE_DIRECTORIES "${PERPLEX_OBJ_libgcv_Scanner_INCLUDE_DIR}")
set_property(TARGET libgcv APPEND PROPERTY INCLUDE_DIRECTORIES "${LEMON_OBJ_libgcv_Parser_INCLUDE_DIR}")
if(TARGET libgcv-static)
  set_property(TARGET libgcv-static APPEND PROPERTY INCLUDE_DIRECTORIES "${PERPLEX_OBJ_libgcv-static_Scanner_INCLUDE_DIR}")
  set_property(TARGET libgcv-static APPEND PROPERTY INCLUDE_DIRECTORIES "${LEMON_OBJ_libgcv-static_Parser_INCLUDE_DIR}")
endif(TARGET libgcv-static)

enable_testing()
BRLCAD_ADDEXEC(test_bottess test_bottess.c libgcv NO_INSTALL)
if(CPP_DLL_DEFINES)
  set_property(TARGET test_bottess APPEND PROPERTY COMPILE_DEFINITIONS "BOTTESS_DLL_IMPORTS")
endif(CPP_DLL_DEFINES)
add_test(NAME bottess_test COMMAND test_bottess)

set(GCV_IGNORE_FILES
  NOTES
  conv/fastgen4/NOTES
  soup.h
  tri_intersect.h
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
