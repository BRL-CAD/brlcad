#TODO - once we're not doing both autotools and CMake, re-examine
#this pkgconfig setup

SET(prefix "${CMAKE_INSTALL_PREFIX}")
SET(exec_prefix "\${prefix}")
SET(libdir "\${exec_prefix}/lib")
SET(includedir "\${prefix}/include")
SET(datarootdir "\${prefix}/share")
SET(datadir "\${datarootdir}")
SET(LIBTCL "-ltcl${TCL_VERSION_MAJOR}.${TCL_VERSION_MINOR}")
SET(LIBM "${M_LINKOPT}")
#The CMake system relies on the C++ compiler to find
#this, so we set it manually for pkgconfig
SET(LIBSTDCXX "-lstdc++")
#Also provide the X11 and PNG link strings - generally
#speaking CMake tends to use full paths, so this is a
#bit hackish - need to investigate best practices for later
#also, are pkgconfig systems generally X11 only? (Haiku
#and Aqua build on OSX come to mind, but are pkgconfig setups
#used there?
SET(X_LIBS "-lX11 -lXext -lXi")
SET(LIBPNG "-lpng")

SET(pkgconfig_DATA
  libanalyze.pc.in
  libbn.pc.in
  libbrlcad.pc.in
  libbu.pc.in
  libdm.pc.in
  libfb.pc.in
  libfft.pc.in
  libgcv.pc.in
  libged.pc.in
  libicv.pc.in
  libmultispectral.pc.in
  liboptical.pc.in
  libpc.pc.in
  libpkg.pc.in
  librt.pc.in
  libwdb.pc.in
)

FOREACH(pkgfile ${pkgconfig_DATA})
  STRING(REGEX REPLACE "([0-9a-z_-]*).pc.in" "\\1" filename_root "${pkgfile}")
  configure_file(${pkgfile} ${CMAKE_CURRENT_BINARY_DIR}/${filename_root}.pc @ONLY)
  INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/${filename_root}.pc DESTINATION ${LIB_DIR}/pkgconfig)
ENDFOREACH(pkgfile ${pkgconfig_DATA})
