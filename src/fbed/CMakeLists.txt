include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_STD_INCLUDES}
  ${BRLCAD_TERMLIB_INCLUDE_DIR}
  ${TCL_INCLUDE_PATH}
)

add_definitions(
  ${BRLCAD_STD_DEFS}
)

SET(fbed_SOURCES
  char.c 
  empty.c 
  execshell.c 
  fbed.c 
  fill_buf.c 
  getinput.c 
  glob.c 
  pos_pad.c 
  prnt.c 
  sgi_dep.c 
  squash.c 
  try.c
)

BRLCAD_ADDEXEC(fbed "${fbed_SOURCES}" "vfont fb termio cursor ${BRLCAD_TERMLIB_LIBRARY}")

