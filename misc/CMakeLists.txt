SET(prefix ${CMAKE_INSTALL_PREFIX})
SET(exec_prefix "\${prefix}")
SET(libdir "\${exec_prefix}/lib")
SET(includedir "\${prefix}/include")
SET(datarootdir "\${prefix}/share")
SET(datadir "\${datarootdir}")

ADD_SUBDIRECTORY(pkgconfig)
# FIXME - need to finish porting enigma to Windows - 
# tried adding postgresql crypt function but it doesn't
# seem to be working.
IF(NOT WIN32)
	ADD_SUBDIRECTORY(enigma)
ENDIF(NOT WIN32)

CONFIGURE_FILE(brlcad-config.in ${CMAKE_BINARY_DIR}/bin/brlcad-config @ONLY)

BRLCAD_ADDFILE(fortran_example.f sample_applications)
INSTALL(PROGRAMS ${CMAKE_BINARY_DIR}/bin/brlcad-config DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_BIN_DIR})
FILE(COPY brlcad-config.1 DESTINATION ${CMAKE_BINARY_DIR}/${MAN_DIR}/man1)
INSTALL(FILES brlcad-config.1 DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_MAN_DIR}/man1)

SET(density_DATAFILES
  GQA_SAMPLE_DENSITIES
  NIST_DENSITIES
)
BRLCAD_ADDDATA(density_DATAFILES data)

BRLCAD_ADDFILE(nsis/brlcad.ico icons)
BRLCAD_ADDFILE(nsis/archer.ico icons)
