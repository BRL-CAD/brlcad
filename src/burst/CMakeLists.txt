include_directories(
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BRLCAD_SOURCE_DIR}/src/burst
  ${BRLCAD_TERMLIB_INCLUDE_DIR}
  ${TCL_INCLUDE_PATH}
)

add_definitions(
  -DHAVE_CONFIG_H
  -DBRLCADBUILD
)


SET(burst_SOURCES
  burst.c 
  error.c 
  fb.c 
  glob.c 
  grid.c 
  gridrotate.c 
  Hm.c 
  HmGetc.c 
  HmGlob.c 
  idents.c 
  paint.c 
  plot.c 
  prnt.c 
  Sc.c 
  trie.c 
  ui.c
)

BRLCAD_ADDEXEC(burst "${burst_SOURCES}" "librt fb termio ${BRLCAD_TERMLIB_LIBRARY}")


SET(burst_noinst_HEADERS
	ascii.h 
	burst.h 
	extern.h 
	Hm.h 
	Mm.h 
	Sc.h 
	trie.h
)
