include_directories(
  ${BRLCAD_STD_INCLUDES}
  ${TCL_INCLUDE_PATH}
)

add_definitions(
  ${BRLCAD_STD_DEFS}
)

SET(halftone_SOURCES
  main.c 
  sharpen.c 
  tone_classic.c 
  tone_floyd.c 
  tone_folly.c 
  tone_simple.c 
  tonescale.c
)

BRLCAD_ADDEXEC(halftone "${halftone_SOURCES}" "librt fb")

install(FILES halftone.1 DESTINATION ${BRLCAD_INSTALL_MAN_DIR}/man1)
