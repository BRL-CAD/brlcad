# Convenience macros for building pkgIndex.tcl and tclIndex files

MACRO(pkgIndex_BUILD name targetdir)
  string(REGEX REPLACE "/" "_" targetprefix ${targetdir})
  add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}/pkgIndex.tcl
    COMMAND btclsh ${CMAKE_SOURCE_DIR}/src/tclscripts/ampi.tcl ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}
    DEPENDS btclsh ${targetprefix}.sentinel
    )
  add_custom_target(${name}_pkgIndex.tcl ALL DEPENDS ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}/pkgIndex.tcl)
  install(FILES ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}/pkgIndex.tcl DESTINATION ${DATA_DIR}/${targetdir})
  # Maintain a list of all pkgIndex targets for dependency purposes
  set(pkgIndex_target_list ${pkgIndex_target_list} ${name}_pkgIndex.tcl)
  LIST(REMOVE_DUPLICATES pkgIndex_target_list)
  set(pkgIndex_target_list "${pkgIndex_target_list}" CACHE STRING "pkgIndex target list" FORCE)
ENDMACRO(pkgIndex_BUILD name targetdir)

MACRO(tclIndex_BUILD name targetdir)
  string(REGEX REPLACE "/" "_" targetprefix ${targetdir})
  add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}/tclIndex
    COMMAND btclsh ${CMAKE_SOURCE_DIR}/src/tclscripts/ami.tcl ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}
    DEPENDS btclsh ${targetprefix}.sentinel
    )
  add_custom_target(${name}_tclIndex ALL DEPENDS ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}/tclIndex)
  install(FILES ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}/tclIndex DESTINATION ${DATA_DIR}/${targetdir})
  # Maintain a list of all tclIndex targets for dependency purposes
  set(tclIndex_target_list ${tclIndex_target_list} ${name}_tclIndex)
  LIST(REMOVE_DUPLICATES tclIndex_target_list)
  set(tclIndex_target_list "${tclIndex_target_list}" CACHE STRING "tclIndex target list" FORCE)
ENDMACRO(tclIndex_BUILD name targetdir)

mark_as_advanced(pkgIndex_target_list)
mark_as_advanced(tclIndex_target_list)


# Now that the macros are defined, we can add tclscript subdirs
add_subdirectory(archer)
add_subdirectory(boteditor)
add_subdirectory(geometree)
add_subdirectory(hv3)
add_subdirectory(lib)
add_subdirectory(mged)
add_subdirectory(pl-dm)
add_subdirectory(rtwizard)
add_subdirectory(sdialogs)
add_subdirectory(swidgets)
add_subdirectory(util)

set(tclscripts_TCLSCRIPTS
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
