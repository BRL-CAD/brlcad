ADD_SUBDIRECTORY(lib)

ADD_CUSTOM_COMMAND(
	OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/rtwizard
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/rtwizard.tcl ${CMAKE_CURRENT_BINARY_DIR}/rtwizard 
	)
ADD_CUSTOM_TARGET(rtwizard ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/rtwizard)
INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/rtwizard DESTINATION ${BRLCAD_INSTALL_DATA_DIR}/tclscripts/rtwizard)


SET(rtwizard_TCLSCRIPTS
	RaytraceWizard.tcl
	rtwizard.bat
	rtwizard.tcl
	)

INSTALL(FILES ${rtwizard_TCLSCRIPTS} DESTINATION ${BRLCAD_INSTALL_DATA_DIR}/tclscripts/rtwizard)

pkgIndex_BUILD(rtwizard ${CMAKE_CURRENT_SOURCE_DIR} tclscripts/rtwizard)
tclIndex_BUILD(rtwizard ${CMAKE_CURRENT_SOURCE_DIR} tclscripts/rtwizard)

SET(rtwizard_TCLSCRIPT_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/pkgIndex.tcl
	${CMAKE_CURRENT_SOURCE_DIR}/tclIndex
	${CMAKE_CURRENT_BINARY_DIR}/rtwizard
	)
SET_DIRECTORY_PROPERTIES(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "${rtwizard_TCLSCRIPT_FILES}")

# Examples

SET(PictureTypeA_DATA
	examples/PictureTypeA/gdesc.txt 
	examples/PictureTypeA/ghelpstr.txt 
	examples/PictureTypeA/gintro.txt 
	examples/PictureTypeA/gtitle.txt 
	examples/PictureTypeA/gpreview.small.gif
	)
INSTALL(FILES ${PictureTypeA_DATA} DESTINATION ${BRLCAD_INSTALL_DATA_DIR}/tclscripts/rtwizard/examples/PictureTypeA)


SET(PictureTypeB_DATA
	examples/PictureTypeB/gdesc.txt 
	examples/PictureTypeB/ghelpstr.txt 
	examples/PictureTypeB/gintro.txt 
	examples/PictureTypeB/gtitle.txt 
	examples/PictureTypeB/gpreview.small.gif
	)
INSTALL(FILES ${PictureTypeB_DATA} DESTINATION ${BRLCAD_INSTALL_DATA_DIR}/tclscripts/rtwizard/examples/PictureTypeB)


SET(PictureTypeC_DATA
	examples/PictureTypeC/gdesc.txt 
	examples/PictureTypeC/ghelpstr.txt 
	examples/PictureTypeC/gintro.txt 
	examples/PictureTypeC/gtitle.txt 
	examples/PictureTypeC/gpreview.small.gif
	)
INSTALL(FILES ${PictureTypeC_DATA} DESTINATION ${BRLCAD_INSTALL_DATA_DIR}/tclscripts/rtwizard/examples/PictureTypeC)


SET(PictureTypeD_DATA
	examples/PictureTypeD/gdesc.txt 
	examples/PictureTypeD/ghelpstr.txt 
	examples/PictureTypeD/gintro.txt 
	examples/PictureTypeD/gtitle.txt 
	examples/PictureTypeD/gpreview.small.gif
	)
INSTALL(FILES ${PictureTypeD_DATA} DESTINATION ${BRLCAD_INSTALL_DATA_DIR}/tclscripts/rtwizard/examples/PictureTypeD)


SET(PictureTypeE_DATA
	examples/PictureTypeE/gdesc.txt 
	examples/PictureTypeE/ghelpstr.txt 
	examples/PictureTypeE/gintro.txt 
	examples/PictureTypeE/gtitle.txt 
	examples/PictureTypeE/gpreview.small.gif
	)
INSTALL(FILES ${PictureTypeE_DATA} DESTINATION ${BRLCAD_INSTALL_DATA_DIR}/tclscripts/rtwizard/examples/PictureTypeE)


SET(PictureTypeF_DATA
	examples/PictureTypeF/gdesc.txt 
	examples/PictureTypeF/ghelpstr.txt 
	examples/PictureTypeF/gintro.txt 
	examples/PictureTypeF/gtitle.txt 
	examples/PictureTypeF/gpreview.small.gif
	)
INSTALL(FILES ${PictureTypeF_DATA} DESTINATION ${BRLCAD_INSTALL_DATA_DIR}/tclscripts/rtwizard/examples/PictureTypeF)

