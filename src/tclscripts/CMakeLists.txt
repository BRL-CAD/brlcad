# Convenience macros for building pkgIndex.tcl and tclIndex files

MACRO(pkgIndex_BUILD name targetdir)
	STRING(REGEX REPLACE "/" "_" targetprefix ${targetdir})
	ADD_CUSTOM_COMMAND(
		OUTPUT ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}/pkgIndex.tcl
		COMMAND btclsh ${CMAKE_SOURCE_DIR}/src/tclscripts/ampi.tcl ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}
		DEPENDS btclsh ${targetprefix}.sentinel
		)
	ADD_CUSTOM_TARGET(${name}_pkgIndex.tcl ALL DEPENDS ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}/pkgIndex.tcl)
	INSTALL(FILES ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}/pkgIndex.tcl DESTINATION ${DATA_DIR}/${targetdir})
	# Maintain a list of all pkgIndex targets for dependency purposes
	SET(pkgIndex_target_list ${pkgIndex_target_list} ${name}_pkgIndex.tcl)
	LIST(REMOVE_DUPLICATES pkgIndex_target_list)
	SET(pkgIndex_target_list "${pkgIndex_target_list}" CACHE STRING "pkgIndex target list" FORCE)
ENDMACRO(pkgIndex_BUILD name targetdir)

MACRO(tclIndex_BUILD name targetdir)
	STRING(REGEX REPLACE "/" "_" targetprefix ${targetdir})
	ADD_CUSTOM_COMMAND(
		OUTPUT ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}/tclIndex
		COMMAND btclsh ${CMAKE_SOURCE_DIR}/src/tclscripts/ami.tcl ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}
		DEPENDS btclsh ${targetprefix}.sentinel
		)
	ADD_CUSTOM_TARGET(${name}_tclIndex ALL DEPENDS ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}/tclIndex)
	INSTALL(FILES ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}/tclIndex DESTINATION ${DATA_DIR}/${targetdir})
	# Maintain a list of all tclIndex targets for dependency purposes
	SET(tclIndex_target_list ${tclIndex_target_list} ${name}_tclIndex)
	LIST(REMOVE_DUPLICATES tclIndex_target_list)
	SET(tclIndex_target_list "${tclIndex_target_list}" CACHE STRING "tclIndex target list" FORCE)
ENDMACRO(tclIndex_BUILD name targetdir)

MARK_AS_ADVANCED(pkgIndex_target_list)
MARK_AS_ADVANCED(tclIndex_target_list)


# Now that the macros are defined, we can add tclscript subdirs
ADD_SUBDIRECTORY(archer)
ADD_SUBDIRECTORY(boteditor)
ADD_SUBDIRECTORY(geometree)
ADD_SUBDIRECTORY(hv3)
ADD_SUBDIRECTORY(lib)
ADD_SUBDIRECTORY(mged)
ADD_SUBDIRECTORY(pl-dm)
ADD_SUBDIRECTORY(rtwizard)
ADD_SUBDIRECTORY(sdialogs)
ADD_SUBDIRECTORY(swidgets)
ADD_SUBDIRECTORY(util)

SET(tclscripts_TCLSCRIPTS
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
	man_browser.tcl
	menu_override.tcl
	mouse.tcl
	vmath.tcl
)
BRLCAD_ADDDATA(tclscripts_TCLSCRIPTS tclscripts)
pkgIndex_BUILD(tclscripts tclscripts)
tclIndex_BUILD(tclscripts tclscripts)

CMAKEFILES(README Makefile.am pkgIndex.tcl tclIndex hv3_man_browser_test.tcl)
