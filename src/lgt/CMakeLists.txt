include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${TCL_INCLUDE_DIRS}
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

IF(HAVE_TERMLIB)
   BRLCAD_ADDEXEC(lgt "${lgt_SOURCES}" "librt libfb libtermio libvfont libcursor")
   SET_TARGET_PROPERTIES(lgt PROPERTIES COMPILE_DEFINITIONS "BEHIND_ME_TOL=0.05")

   install(FILES lgt.1  DESTINATION ${MAN_DIR}/man1)
ENDIF(HAVE_TERMLIB)
