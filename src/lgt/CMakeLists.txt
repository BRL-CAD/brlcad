set(LGT_INCLUDE_DIRS
  ${BU_INCLUDE_DIRS}
  ${FB_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  ${TERMIO_INCLUDE_DIRS}
  ${VFONT_INCLUDE_DIRS}
  ${CURSOR_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}
  )
LIST(REMOVE_DUPLICATES LGT_INCLUDE_DIRS)
include_directories(${LGT_INCLUDE_DIRS})

set(lgt_SOURCES
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

if(HAVE_TERMLIB)
  BRLCAD_ADDEXEC(lgt "${lgt_SOURCES}" "librt;libfb;libbu;libtermio;libcursor;${M_LIBRARY}")
  SET_TARGET_PROPERTIES(lgt PROPERTIES COMPILE_DEFINITIONS "BEHIND_ME_TOL=0.05")

endif(HAVE_TERMLIB)
set(lgt_ignore_files
  ascii.h
  cursors.h
  extern.h
  hmenu.h
  lgt.h
  mat_db.h
  screen.h
  texture.h
  tree.h
  )
CMAKEFILES(${lgt_ignore_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
