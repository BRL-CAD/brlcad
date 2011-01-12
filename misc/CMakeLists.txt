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

CONFIGURE_FILE(brlcad-config.in ${CMAKE_CURRENT_BINARY_DIR}/brlcad-config @ONLY)

INSTALL(FILES fortran_example.f DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}/sample_applications)
INSTALL(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/brlcad-config DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_BIN_DIR})
INSTALL(FILES brlcad-config.1 DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_MAN_DIR}/man1)

SET(density_DATAFILES
  GQA_SAMPLE_DENSITIES
  NIST_DENSITIES
)
INSTALL(FILES ${density_DATAFILES} DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}/data)
