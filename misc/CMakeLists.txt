if(BUILD_STATIC_LIBS AND BRLCAD_ENABLE_BRLCAD_LIBRARY)
	ADD_SUBDIRECTORY(win32-msvc)
endif(BUILD_STATIC_LIBS AND BRLCAD_ENABLE_BRLCAD_LIBRARY)

set(prefix ${CMAKE_INSTALL_PREFIX})
set(exec_prefix "\${prefix}")
set(libdir "\${exec_prefix}/lib")
set(includedir "\${prefix}/include")
set(datarootdir "\${prefix}/share")
set(datadir "\${datarootdir}")

ADD_SUBDIRECTORY(pkgconfig)
# FIXME - need to finish porting enigma to Windows -
# tried adding postgresql crypt function but it doesn't
# seem to be working.
if(NOT WIN32)
	ADD_SUBDIRECTORY(enigma)
endif(NOT WIN32)

CONFIGURE_file(brlcad-config.in ${CMAKE_BINARY_DIR}/bin/brlcad-config @ONLY)

BRLCAD_ADDfile(fortran_example.f sample_applications)
install(PROGRAMS ${CMAKE_BINARY_DIR}/bin/brlcad-config DESTINATION ${BIN_DIR})
file(COPY brlcad-config.1 DESTINATION ${CMAKE_BINARY_DIR}/${MAN_DIR}/man1)
install(FILES brlcad-config.1 DESTINATION ${MAN_DIR}/man1)

set(density_DATAFILES
  GQA_SAMPLE_DENSITIES
  NIST_DENSITIES
)
BRLCAD_ADDDATA(density_DATAFILES data)

BRLCAD_ADDfile(nsis/brlcad.ico icons)
BRLCAD_ADDfile(nsis/archer.ico icons)

find_package(Doxygen)
if(DOXYGEN_FOUND)
	if(BRLCAD_BUILD_DOXYGEN_DOCS)
		ADD_CUSTOM_COMMAND(
			OUTPUT ${CMAKE_BINARY_DIR}/doc/doxygen_output/html/index.html
			COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_SOURCE_DIR}/misc/Doxyfile
			WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/doc
			)
		ADD_CUSTOM_TARGET(dox DEPENDS ${CMAKE_BINARY_DIR}/misc/doxygen_output/html/index.html)
	endif(BRLCAD_BUILD_DOXYGEN_DOCS)
else(DOXYGEN_FOUND)
	if(BRLCAD_BUILD_DOXYGEN_DOCS)
		message("Doxygen build requested but doxygen not found, disabling")
	endif(BRLCAD_BUILD_DOXYGEN_DOCS)
endif(DOXYGEN_FOUND)

set(misc_ignore_files
	CMake
	Description.txt
	Doxyfile
	Doxyfile.in
	Makefile.defs
	archlinux
	astyle.opt
	batch-indent-region.el
	brlcad-config.1
	brlcad-config.in
	brlcad.spec.in
	debian
	doxygen_structure
	flawfinder
	libtool.m4
	macosx
	nsis
	win32-msvc
	Makefile.am
	)
CMAKEFILES(${misc_ignore_files})
