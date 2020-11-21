add_subdirectory(wfobj)
if(TARGET libwfobj)
  SetTargetFolder(libwfobj "Third Party Libraries")
endif(TARGET libwfobj)
if(TARGET libwfobj-static)
  SetTargetFolder(libwfobj-static "Third Party Libraries")
endif(TARGET libwfobj-static)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/wfobj
  ${CMAKE_CURRENT_BINARY_DIR}/wfobj
  )

set(OBJ_SRCS
  obj_read.c
  obj_write.c
  tri_face.c
  )

gcv_plugin_library(gcv-obj SHARED ${OBJ_SRCS})
target_link_libraries(gcv-obj librt libwfobj)
set_property(TARGET gcv-obj APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
VALIDATE_STYLE(gcv-obj "${OBJ_SRCS}")
PLUGIN_SETUP(gcv-obj gcv)

set(obj_ignore_files
  CMakeLists.txt
  ${OBJ_SRCS}
  tri_face.h
  wfobj/CMake/FindLEMON.cmake
  wfobj/CMake/FindPERPLEX.cmake
  wfobj/CMake/FindRE2C.cmake
  wfobj/CMakeLists.txt
  wfobj/CMake/Path_Setup.cmake
  wfobj/obj_grammar_decls.h
  wfobj/obj_grammar.yy
  wfobj/obj_parser.cpp
  wfobj/obj_parser.h
  wfobj/obj_parser_state.h
  wfobj/obj_rules.h
  wfobj/obj_rules.l
  wfobj/obj_token_type.h
  wfobj/obj_util.cpp
  wfobj/obj_util.h
  )

CMAKEFILES(${obj_ignore_files})

DISTCLEAN(
  ${CMAKE_CURRENT_BINARY_DIR}/wfobj/WFOBJ_PARSER_obj_grammar/obj_grammar.yy
  ${CMAKE_CURRENT_BINARY_DIR}/wfobj/WFOBJ_PARSER_SO_obj_grammar/obj_grammar.yy
  ${CMAKE_CURRENT_BINARY_DIR}/wfobj/WFOBJ_PARSER_STATIC_obj_grammar/obj_grammar.yy
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

