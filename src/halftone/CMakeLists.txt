include_directories(
  ${TCL_INCLUDE_DIRS}
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

BRLCAD_ADDEXEC(halftone "${halftone_SOURCES}" "librt libfb")

ADD_MAN_PAGE(1 halftone.1)
