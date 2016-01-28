add_subdirectory(wfobj)

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
  wfobj/CMake/LEMON_Util.cmake
  wfobj/CMake/PERPLEX_Util.cmake
  wfobj/CMake/Path_Setup.cmake
  wfobj/CMake/RE2C_Util.cmake
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

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

