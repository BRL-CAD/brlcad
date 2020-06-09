set(BURST_INCLUDE_DIRS
  ${DM_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}
  )
list(REMOVE_DUPLICATES BURST_INCLUDE_DIRS)
include_directories(${BURST_INCLUDE_DIRS})

set(burst_SOURCES
  Hm.c
  HmGetc.c
  HmGlob.c
  Sc.c
  burst.c
  error.c
  fb.c
  glob.c
  grid.c
  gridrotate.c
  idents.c
  paint.c
  plot.c
  prnt.c
  trie.c
  ui.c
  )


BRLCAD_ADDEXEC(burst "${burst_SOURCES}" "librt;libdm;${M_LIBRARY}")

set(burst_noinst_HEADERS
  CMakeLists.txt
  ascii.h
  burst.h
  extern.h
  Mm.h
  Sc.h
  )
CMAKEFILES(${burst_noinst_HEADERS} burst.format)

CMAKEFILES(TODO.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
