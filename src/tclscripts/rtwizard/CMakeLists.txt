ADD_SUBDIRECTORY(lib)

ADD_CUSTOM_COMMAND(
	OUTPUT  ${CMAKE_BINARY_DIR}/bin/rtwizard
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/rtwizard.tcl ${CMAKE_BINARY_DIR}/bin/rtwizard 
	)
ADD_CUSTOM_TARGET(rtwizard ALL DEPENDS ${CMAKE_BINARY_DIR}/bin/rtwizard)
INSTALL(PROGRAMS ${CMAKE_BINARY_DIR}/bin/rtwizard DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_BIN_DIR})

SET(rtwizard_TCLSCRIPTS
	RaytraceWizard.tcl
	rtwizard.bat
	rtwizard.tcl
	)

IF(WIN32)
	configure_file(rtwizard.bat ${CMAKE_BINARY_DIR}/bin/rtwizard.bat COPYONLY)
	INSTALL(PROGRAMS rtwizard.bat DESTINATION bin)
ENDIF(WIN32)

FILE(COPY ${rtwizard_TCLSCRIPTS} DESTINATION ${CMAKE_BINARY_DIR}/${DATA_DIR}/tclscripts/rtwizard)
INSTALL(FILES ${rtwizard_TCLSCRIPTS} DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}/tclscripts/rtwizard)

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
FILE(COPY ${PictureTypeA_DATA} DESTINATION ${CMAKE_BINARY_DIR}/${DATA_DIR}/tclscripts/rtwizard/examples/PictureTypeA)
INSTALL(FILES ${PictureTypeA_DATA} DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}/tclscripts/rtwizard/examples/PictureTypeA)


SET(PictureTypeB_DATA
	examples/PictureTypeB/desc.txt 
	examples/PictureTypeB/helpstr.txt 
	examples/PictureTypeB/intro.txt 
	examples/PictureTypeB/title.txt 
	examples/PictureTypeB/preview.small.gif
	)
FILE(COPY ${PictureTypeB_DATA} DESTINATION ${CMAKE_BINARY_DIR}/${DATA_DIR}/tclscripts/rtwizard/examples/PictureTypeB)
INSTALL(FILES ${PictureTypeB_DATA} DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}/tclscripts/rtwizard/examples/PictureTypeB)


SET(PictureTypeC_DATA
	examples/PictureTypeC/desc.txt 
	examples/PictureTypeC/helpstr.txt 
	examples/PictureTypeC/intro.txt 
	examples/PictureTypeC/title.txt 
	examples/PictureTypeC/preview.small.gif
	)
FILE(COPY ${PictureTypeC_DATA} DESTINATION ${CMAKE_BINARY_DIR}/${DATA_DIR}/tclscripts/rtwizard/examples/PictureTypeC)
INSTALL(FILES ${PictureTypeC_DATA} DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}/tclscripts/rtwizard/examples/PictureTypeC)


SET(PictureTypeD_DATA
	examples/PictureTypeD/desc.txt 
	examples/PictureTypeD/helpstr.txt 
	examples/PictureTypeD/intro.txt 
	examples/PictureTypeD/title.txt 
	examples/PictureTypeD/preview.small.gif
	)
FILE(COPY ${PictureTypeD_DATA} DESTINATION ${CMAKE_BINARY_DIR}/${DATA_DIR}/tclscripts/rtwizard/examples/PictureTypeD)
INSTALL(FILES ${PictureTypeD_DATA} DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}/tclscripts/rtwizard/examples/PictureTypeD)


SET(PictureTypeE_DATA
	examples/PictureTypeE/desc.txt 
	examples/PictureTypeE/helpstr.txt 
	examples/PictureTypeE/intro.txt 
	examples/PictureTypeE/title.txt 
	examples/PictureTypeE/preview.small.gif
	)
FILE(COPY ${PictureTypeE_DATA} DESTINATION ${CMAKE_BINARY_DIR}/${DATA_DIR}/tclscripts/rtwizard/examples/PictureTypeE)
INSTALL(FILES ${PictureTypeE_DATA} DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}/tclscripts/rtwizard/examples/PictureTypeE)


SET(PictureTypeF_DATA
	examples/PictureTypeF/desc.txt 
	examples/PictureTypeF/helpstr.txt 
	examples/PictureTypeF/intro.txt 
	examples/PictureTypeF/title.txt 
	examples/PictureTypeF/preview.small.gif
	)
FILE(COPY ${PictureTypeF_DATA} DESTINATION ${CMAKE_BINARY_DIR}/${DATA_DIR}/tclscripts/rtwizard/examples/PictureTypeF)
INSTALL(FILES ${PictureTypeF_DATA} DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}/tclscripts/rtwizard/examples/PictureTypeF)

