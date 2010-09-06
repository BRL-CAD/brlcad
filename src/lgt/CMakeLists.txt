include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_STD_INCLUDES}
  ${TCL_INCLUDE_PATH}
)

add_definitions(
  ${BRLCAD_STD_DEFS}
)

SET(lgt_SOURCES
  char.c 
  do_options.c 
  error.c 
  execshell.c 
  fb.c 
  glob.c 
  grid_rotate.c 
  hmenu.c 
  ir.c 
  lgt.c 
  lgt_db.c 
  mat_db.c 
  octree.c 
  prnt.c 
  reflect.c 
  sgi_dep.c 
  texture.c 
  trie.c
)

BRLCAD_ADDEXEC(lgt "${lgt_SOURCES}" "vfont librt fb termio cursor")
SET_TARGET_PROPERTIES(lgt PROPERTIES COMPILE_DEFINITIONS "BEHIND_ME_TOL=0.05")

install(FILES lgt.1  DESTINATION ${BRLCAD_INSTALL_MAN_DIR}/man1)
