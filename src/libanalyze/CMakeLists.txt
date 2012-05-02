# Include directories needed by libanalyze users
set(ANALYZE_INCLUDE_DIRS
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  )
BRLCAD_LIB_INCLUDE_DIRS(analyze ANALYZE_INCLUDE_DIRS "")

set(LIBANALYZE_SOURCES
  density.c
  overlaps.c
  )

#BRLCAD_ADDEXEC(test_density test_density.c "libanalyze;libbu" NO_INSTALL)

BRLCAD_ADDLIB(libanalyze "${LIBANALYZE_SOURCES}" libbu)
SET_TARGET_PROPERTIES(libanalyze PROPERTIES VERSION 19.0.1 SOVERSION 19)
CMAKEFILES(Makefile.am test_density.c)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
