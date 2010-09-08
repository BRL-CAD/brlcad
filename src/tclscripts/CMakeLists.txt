MACRO(pkgIndex_BUILD name srcdir targetdir)
	ADD_CUSTOM_COMMAND(
		OUTPUT ${srcdir}/pkgIndex.tcl
		COMMAND btclsh ${CMAKE_SOURCE_DIR}/src/tclscripts/ampi.tcl ${srcdir}
		DEPENDS btclsh
		)
	ADD_CUSTOM_TARGET(${name}_pkgIndex.tcl ALL DEPENDS ${srcdir}/pkgIndex.tcl)
	INSTALL(FILES ${srcdir}/pkgIndex.tcl DESTINATION ${BRLCAD_INSTALL_DATA_DIR}/${targetdir})
ENDMACRO(pkgIndex_BUILD name srcdir targetdir)

MACRO(tclIndex_BUILD name srcdir targetdir)
	ADD_CUSTOM_COMMAND(
		OUTPUT ${srcdir}/tclIndex
		COMMAND btclsh ${CMAKE_SOURCE_DIR}/src/tclscripts/ami.tcl ${srcdir}
		DEPENDS btclsh
		)
	ADD_CUSTOM_TARGET(${name}_tclIndex ALL DEPENDS ${srcdir}/tclIndex)
	INSTALL(FILES ${srcdir}/tclIndex DESTINATION ${BRLCAD_INSTALL_DATA_DIR}/${targetdir})
ENDMACRO(tclIndex_BUILD name srcdir targetdir)



ADD_SUBDIRECTORY(archer)
ADD_SUBDIRECTORY(boteditor)
ADD_SUBDIRECTORY(geometree)
ADD_SUBDIRECTORY(hv3)
#ADD_SUBDIRECTORY(lib)
#ADD_SUBDIRECTORY(mged)
#ADD_SUBDIRECTORY(nirt)
#ADD_SUBDIRECTORY(pl-dm)
#ADD_SUBDIRECTORY(rtwizard)
#ADD_SUBDIRECTORY(sdialogs)
#ADD_SUBDIRECTORY(swidgets)
#ADD_SUBDIRECTORY(util)

SET(toplevel_TCLSCRIPTS
	ami.tcl 
	ampi.tcl 
	cad_clrpick.tcl 
	cad_dialog.tcl 
	chkexterns.tcl 
	fs_dialog.tk 
	helpcomm.tcl 
	helplib.tcl 
	hoc.tcl 
	html_library.tcl 
	libdm.tcl 
	menu_override.tcl 
	mouse.tcl 
	vmath.tcl
)
INSTALL(FILES ${toplevel_TCLSCRIPTS} DESTINATION ${BRLCAD_INSTALL_DATA_DIR}/tclscripts)

pkgIndex_BUILD(tclscripts ${CMAKE_CURRENT_SOURCE_DIR} tclscripts)
tclIndex_BUILD(tclscripts ${CMAKE_CURRENT_SOURCE_DIR} tclscripts)

SET(toplevel_TCLSCRIPT_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/pkgIndex.tcl
	${CMAKE_CURRENT_SOURCE_DIR}/tclIndex
)
SET_DIRECTORY_PROPERTIES(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "${toplevel_TCLSCRIPT_FILES}")
