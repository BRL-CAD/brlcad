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

find_package(Doxygen)
IF(DOXYGEN_FOUND)
	configure_file(Doxyfile.in ${CMAKE_BINARY_DIR}/misc/Doxyfile)
	IF(BRLCAD-BUILD_DOXYGEN_DOCS)
		ADD_CUSTOM_COMMAND(
			OUTPUT ${CMAKE_BINARY_DIR}/misc/doxygen_output/html/index.html
			COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_BINARY_DIR}/misc/Doxyfile
			WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
			)
		ADD_CUSTOM_TARGET(dox ALL DEPENDS ${CMAKE_BINARY_DIR}/misc/doxygen_output/html/index.html)
	ENDIF(BRLCAD-BUILD_DOXYGEN_DOCS)
ELSE(DOXYGEN_FOUND)
	IF(BRLCAD-BUILD_DOXYGEN_DOCS)
		MESSAGE("Doxygen build requested but doxygen not found, disabling")
	ENDIF(BRLCAD-BUILD_DOXYGEN_DOCS)
ENDIF(DOXYGEN_FOUND)


