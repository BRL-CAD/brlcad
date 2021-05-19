# Include directories needed by libpkg users
set(PKG_STDIO_INCLUDES
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BU_INCLUDE_DIRS}
  ${PKG_INCLUDE_DIRS}
  )
include_directories(${PKG_EXAMPLE_INCLUDES})

BRLCAD_ADDEXEC(pkg_stdio_main main.c "libbu;libpkg" NO_INSTALL)
BRLCAD_ADDEXEC(pkg_stdio_subp subp.c "libbu;libpkg" NO_INSTALL)

CMAKEFILES(ncp.h)
CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
