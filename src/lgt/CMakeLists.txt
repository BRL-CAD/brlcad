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
  texture.c
  trie.c
)

IF(HAVE_TERMLIB)
   BRLCAD_ADDEXEC(lgt "${lgt_SOURCES}" "librt libfb libtermio libvfont libcursor ${M_LIBRARY}")
   SET_TARGET_PROPERTIES(lgt PROPERTIES COMPILE_DEFINITIONS "BEHIND_ME_TOL=0.05")

	ADD_MAN_PAGE(1 lgt.1)
ENDIF(HAVE_TERMLIB)
SET(lgt_ignore_files
	ascii.h
	cursors.h
	extern.h
	hmenu.h
	lgt.1
	lgt.h
	mat_db.h
	screen.h
	texture.h
	tree.h
	)
CMAKEFILES(${lgt_ignore_files})
