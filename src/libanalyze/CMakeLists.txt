set(LIBANALYZE_SOURCES
    density.c
    overlaps.c
)

include_directories(
    ${TCL_INCLUDE_DIRS}
)

BRLCAD_ADDLIB(libanalyze "${LIBANALYZE_SOURCES}" libbu)
SET_TARGET_PROPERTIES(libanalyze PROPERTIES VERSION 19.0.1 SOVERSION 19)
CMAKEFILES(Makefile.am)
