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

LIBGCV_ADD_PLUGIN(obj "obj_read.c;obj_write.c;tri_face.c" "librt;libwfobj")

set(obj_ignore_files
  tri_face.h
  wfobj/CMake
  wfobj/CMake/FindLEMON.cmake
  wfobj/CMake/FindPERPLEX.cmake
  wfobj/CMake/FindRE2C.cmake
  wfobj/CMake/Path_Setup.cmake
  wfobj/obj_grammar.yy
  wfobj/obj_grammar_decls.h
  wfobj/obj_parser.h
  wfobj/obj_parser_state.h
  wfobj/obj_rules.h
  wfobj/obj_rules.l
  wfobj/obj_token_type.h
  wfobj/obj_util.h
)

CMAKEFILES(${obj_ignore_files})

DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/wfobj/WFOBJ_PARSER_obj_grammar/obj_grammar.yy)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

