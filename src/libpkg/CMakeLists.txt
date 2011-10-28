# Include directories needed by libpkg users
SET(PKG_INCLUDE_DIRS
	${BRLCAD_BINARY_DIR}/include
	${BRLCAD_SOURCE_DIR}/include
	${BU_INCLUDE_DIRS}
	)
BRLCAD_INCLUDE_DIRS(PKG_INCLUDE_DIRS)

set(LIBPKG_SOURCES
    pkg.c
    tcl.c
    vers.c
)

BRLCAD_ADDFILE(tpkg.c sample_applications)

BRLCAD_ADDLIB(libpkg "${LIBPKG_SOURCES}" libbu)
SET_TARGET_PROPERTIES(libpkg PROPERTIES VERSION 19.0.1 SOVERSION 19)
CMAKEFILES(Makefile.am)
