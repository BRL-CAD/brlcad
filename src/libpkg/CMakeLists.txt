# Include directories needed by libpkg users
set(PKG_INCLUDE_DIRS
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  )
BRLCAD_LIB_INCLUDE_DIRS(pkg PKG_INCLUDE_DIRS "")

set(LIBPKG_SOURCES
  pkg.c
  tcl.c
  vers.c
  )

BRLCAD_ADDDATA(tpkg.c sample_applications)

BRLCAD_ADDLIB(libpkg "${LIBPKG_SOURCES}" libbu)
SET_TARGET_PROPERTIES(libpkg PROPERTIES VERSION 19.0.1 SOVERSION 19)
CMAKEFILES(Makefile.am)

add_subdirectory(example)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
