# Convenience macros for building pkgIndex.tcl and tclIndex files

MACRO(pkgIndex_BUILD name srcdir targetdir)
	ADD_CUSTOM_COMMAND(
		OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl
		COMMAND btclsh ${CMAKE_SOURCE_DIR}/src/tclscripts/ampi.tcl ${srcdir}
	   COMMAND ${CMAKE_COMMAND} -E copy ${srcdir}/pkgIndex.tcl ${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl
		COMMAND ${CMAKE_COMMAND} -E remove ${srcdir}/pkgIndex.tcl
		DEPENDS btclsh
		)
	ADD_CUSTOM_TARGET(${name}_pkgIndex.tcl ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl)
	INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}/${targetdir})
ENDMACRO(pkgIndex_BUILD name srcdir targetdir)

MACRO(tclIndex_BUILD name srcdir targetdir)
	ADD_CUSTOM_COMMAND(
		OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tclIndex
		COMMAND btclsh ${CMAKE_SOURCE_DIR}/src/tclscripts/ami.tcl ${srcdir} 
		COMMAND ${CMAKE_COMMAND} -E copy ${srcdir}/tclIndex ${CMAKE_CURRENT_BINARY_DIR}/tclIndex
		COMMAND ${CMAKE_COMMAND} -E remove ${srcdir}/tclIndex
		DEPENDS btclsh
		)
	ADD_CUSTOM_TARGET(${name}_tclIndex ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/tclIndex)
	INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/tclIndex DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}/${targetdir})
ENDMACRO(tclIndex_BUILD name srcdir targetdir)


# Now that the macros are defined, we can add tclscript subdirs
ADD_SUBDIRECTORY(archer)
ADD_SUBDIRECTORY(boteditor)
ADD_SUBDIRECTORY(geometree)
ADD_SUBDIRECTORY(hv3)
ADD_SUBDIRECTORY(lib)
ADD_SUBDIRECTORY(mged)
ADD_SUBDIRECTORY(nirt)
ADD_SUBDIRECTORY(pl-dm)
ADD_SUBDIRECTORY(rtwizard)
ADD_SUBDIRECTORY(sdialogs)
ADD_SUBDIRECTORY(swidgets)
ADD_SUBDIRECTORY(util)

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
INSTALL(FILES ${toplevel_TCLSCRIPTS} DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}/tclscripts)

pkgIndex_BUILD(tclscripts ${CMAKE_CURRENT_SOURCE_DIR} tclscripts)
tclIndex_BUILD(tclscripts ${CMAKE_CURRENT_SOURCE_DIR} tclscripts)

SET(toplevel_TCLSCRIPT_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/pkgIndex.tcl
	${CMAKE_CURRENT_SOURCE_DIR}/tclIndex
)
SET_DIRECTORY_PROPERTIES(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "${toplevel_TCLSCRIPT_FILES}")
