SET(prefix ${BRLCAD_PREFIX})
SET(exec_prefix "\${prefix}")
SET(libdir "\${exec_prefix}/lib")
SET(includedir "\${prefix}/include")
SET(datarootdir "\${prefix}/share")
SET(datadir "\${datarootdir}")

ADD_SUBDIRECTORY(pkgconfig)
ADD_SUBDIRECTORY(enigma)

CONFIGURE_FILE(brlcad-config.in ${CMAKE_CURRENT_BINARY_DIR}/brlcad-config)

INSTALL(FILES fortran_example.f DESTINATION ${BRLCAD_INSTALL_DATA_DIR}/sample_applications)
INSTALL(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/brlcad-config DESTINATION ${BRLCAD_INSTALL_BIN_DIR})
INSTALL(FILES brlcad-config.1 DESTINATION ${BRLCAD_INSTALL_MAN_DIR}/man1)

SET(density_DATAFILES
  GQA_SAMPLE_DENSITIES
  NIST_DENSITIES
)
INSTALL(FILES ${density_DATAFILES} DESTINATION ${BRLCAD_INSTALL_DATA_DIR}/data)
