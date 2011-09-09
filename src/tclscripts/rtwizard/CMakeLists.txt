ADD_SUBDIRECTORY(lib)

ADD_CUSTOM_COMMAND(
	OUTPUT  ${CMAKE_BINARY_DIR}/bin/rtwizard
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/rtwizard.tcl ${CMAKE_BINARY_DIR}/bin/rtwizard
	)
ADD_CUSTOM_TARGET(rtwizard ALL DEPENDS ${CMAKE_BINARY_DIR}/bin/rtwizard)
INSTALL(PROGRAMS ${CMAKE_BINARY_DIR}/bin/rtwizard DESTINATION ${BIN_DIR})

SET(rtwizard_TCLSCRIPTS
	RaytraceWizard.tcl
	rtwizard.bat
	rtwizard.tcl
	)
IF(WIN32)
	configure_file(rtwizard.bat ${CMAKE_BINARY_DIR}/bin/rtwizard.bat COPYONLY)
	INSTALL(PROGRAMS rtwizard.bat DESTINATION bin)
ENDIF(WIN32)
BRLCAD_ADDDATA(rtwizard_TCLSCRIPTS tclscripts/rtwizard)
pkgIndex_BUILD(rtwizard tclscripts/rtwizard)
tclIndex_BUILD(rtwizard tclscripts/rtwizard)

# Examples

SET(PictureTypeA_DATA
	examples/PictureTypeA/desc.txt
	examples/PictureTypeA/helpstr.txt
	examples/PictureTypeA/intro.txt
	examples/PictureTypeA/title.txt
	examples/PictureTypeA/preview.small.gif
	)
BRLCAD_ADDDATA(PictureTypeA_DATA tclscripts/rtwizard/examples/PictureTypeA)

SET(PictureTypeB_DATA
	examples/PictureTypeB/desc.txt
	examples/PictureTypeB/helpstr.txt
	examples/PictureTypeB/intro.txt
	examples/PictureTypeB/title.txt
	examples/PictureTypeB/preview.small.gif
	)
BRLCAD_ADDDATA(PictureTypeB_DATA tclscripts/rtwizard/examples/PictureTypeB)

SET(PictureTypeC_DATA
	examples/PictureTypeC/desc.txt
	examples/PictureTypeC/helpstr.txt
	examples/PictureTypeC/intro.txt
	examples/PictureTypeC/title.txt
	examples/PictureTypeC/preview.small.gif
	)
BRLCAD_ADDDATA(PictureTypeC_DATA tclscripts/rtwizard/examples/PictureTypeC)

SET(PictureTypeD_DATA
	examples/PictureTypeD/desc.txt
	examples/PictureTypeD/helpstr.txt
	examples/PictureTypeD/intro.txt
	examples/PictureTypeD/title.txt
	examples/PictureTypeD/preview.small.gif
	)
BRLCAD_ADDDATA(PictureTypeD_DATA tclscripts/rtwizard/examples/PictureTypeD)

SET(PictureTypeE_DATA
	examples/PictureTypeE/desc.txt
	examples/PictureTypeE/helpstr.txt
	examples/PictureTypeE/intro.txt
	examples/PictureTypeE/title.txt
	examples/PictureTypeE/preview.small.gif
	)
BRLCAD_ADDDATA(PictureTypeE_DATA tclscripts/rtwizard/examples/PictureTypeE)

SET(PictureTypeF_DATA
	examples/PictureTypeF/desc.txt
	examples/PictureTypeF/helpstr.txt
	examples/PictureTypeF/intro.txt
	examples/PictureTypeF/title.txt
	examples/PictureTypeF/preview.small.gif
	)
BRLCAD_ADDDATA(PictureTypeF_DATA tclscripts/rtwizard/examples/PictureTypeF)

CMAKEFILES(examples)
