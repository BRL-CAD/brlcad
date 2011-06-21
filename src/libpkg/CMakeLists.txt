include_directories(
    ${TCL_INCLUDE_DIRS}
)

set(LIBPKG_SOURCES
    pkg.c
    tcl.c
    vers.c
)

BRLCAD_ADDFILE(tpkg.c sample_applications)

BRLCAD_ADDLIB(libpkg "${LIBPKG_SOURCES}" libbu)
SET_TARGET_PROPERTIES(libpkg PROPERTIES VERSION 19.0.1 SOVERSION 19)
