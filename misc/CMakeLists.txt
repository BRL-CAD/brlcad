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

FILE(COPY fortran_example.f DESTINATION ${CMAKE_BINARY_DIR}/${DATA_DIR}/sample_applications)
INSTALL(FILES fortran_example.f DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}/sample_applications)
INSTALL(PROGRAMS ${CMAKE_BINARY_DIR}/bin/brlcad-config DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_BIN_DIR})
FILE(COPY brlcad-config.1 DESTINATION ${CMAKE_BINARY_DIR}/${MAN_DIR}/man1)
INSTALL(FILES brlcad-config.1 DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_MAN_DIR}/man1)

SET(density_DATAFILES
  GQA_SAMPLE_DENSITIES
  NIST_DENSITIES
)
FILE(COPY ${density_DATAFILES} DESTINATION ${CMAKE_BINARY_DIR}/${DATA_DIR}/data)
INSTALL(FILES ${density_DATAFILES} DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}/data)

FILE(COPY nsis/brlcad.ico DESTINATION ${CMAKE_BINARY_DIR}/${DATA_DIR}/icons)
FILE(COPY nsis/archer.ico DESTINATION ${CMAKE_BINARY_DIR}/${DATA_DIR}/icons)
INSTALL(FILES nsis/brlcad.ico DESTINATION  ${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}/icons)
INSTALL(FILES nsis/archer.ico DESTINATION  ${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}/icons)
