SET(prefix "${CMAKE_INSTALL_PREFIX}")
SET(exec_prefix "\${prefix}")
SET(libdir "\${exec_prefix}/lib")
SET(includedir "\${prefix}/include")
SET(datarootdir "\${prefix}/share")
SET(datadir "\${datarootdir}")


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
  INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/${filename_root}.pc DESTINATION ${BRLCAD_INSTALL_LIB_DIR}/pkgconfig)
ENDFOREACH(pkgfile ${pkgconfig_DATA})
