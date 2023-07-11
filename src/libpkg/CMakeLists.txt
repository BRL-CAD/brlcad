# Include directories needed by libpkg users
set(PKG_INCLUDE_DIRS
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  )
BRLCAD_LIB_INCLUDE_DIRS(pkg PKG_INCLUDE_DIRS "")

set(LIBPKG_SOURCES
  pkg.c
  vers.c
  )

BRLCAD_ADDDATA(tpkg.c sample_applications)
BRLCAD_ADDEXEC(tpkg tpkg.c "libbu;libpkg" NO_INSTALL)

BRLCAD_ADDLIB(libpkg "${LIBPKG_SOURCES}" libbu)
SET_TARGET_PROPERTIES(libpkg PROPERTIES VERSION 20.0.1 SOVERSION 20)

add_subdirectory(example)
add_subdirectory(example_qt)
CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
