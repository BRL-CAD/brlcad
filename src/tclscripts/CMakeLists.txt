# Convenience macros for building pkgIndex.tcl and tclIndex files

MACRO(pkgIndex_BUILD name targetdir)
  BRLCAD_GET_DIR_LIST_CONTENTS(DATA_TARGETS "${CMAKE_CURRENT_BINARY_DIR}" data_target_list)
  string(REGEX REPLACE "/" "_" targetprefix ${targetdir})
  if(CMAKE_CONFIGURATION_TYPES)
    set(pkgIndex_outdir "${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/${DATA_DIR}/${targetdir}")
  else(CMAKE_CONFIGURATION_TYPES)
    set(pkgIndex_outdir "${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}")
  endif(CMAKE_CONFIGURATION_TYPES)
  add_custom_command(
    OUTPUT ${pkgIndex_outdir}/pkgIndex.tcl
    COMMAND btclsh ${CMAKE_SOURCE_DIR}/src/tclscripts/ampi.tcl ${pkgIndex_outdir}
    DEPENDS btclsh ${data_target_list}
    )
  install(FILES ${pkgIndex_outdir}/pkgIndex.tcl DESTINATION ${DATA_DIR}/${targetdir})
  add_custom_target(${name}_pkgIndex.tcl ALL DEPENDS ${pkgIndex_outdir}/pkgIndex.tcl)
  # Maintain a list of all pkgIndex targets for dependency purposes
  set(pkgIndex_target_list ${pkgIndex_target_list} ${name}_pkgIndex.tcl)
  list(REMOVE_DUPLICATES pkgIndex_target_list)
  set(pkgIndex_target_list "${pkgIndex_target_list}" CACHE STRING "pkgIndex target list" FORCE)
ENDMACRO(pkgIndex_BUILD name targetdir)

MACRO(tclIndex_BUILD name targetdir)
  BRLCAD_GET_DIR_LIST_CONTENTS(DATA_TARGETS "${CMAKE_CURRENT_BINARY_DIR}" data_target_list)
  string(REGEX REPLACE "/" "_" targetprefix ${targetdir})
  if(CMAKE_CONFIGURATION_TYPES)
    set(tclIndex_outdir "${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/${DATA_DIR}/${targetdir}")
  else(CMAKE_CONFIGURATION_TYPES)
    set(tclIndex_outdir "${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}")
  endif(CMAKE_CONFIGURATION_TYPES)
  add_custom_command(
    OUTPUT ${tclIndex_outdir}/tclIndex
    COMMAND btclsh ${CMAKE_SOURCE_DIR}/src/tclscripts/ami.tcl ${tclIndex_outdir}
    DEPENDS btclsh ${data_target_list}
    )
  install(FILES ${tclIndex_outdir}/tclIndex DESTINATION ${DATA_DIR}/${targetdir})
  add_custom_target(${name}_tclIndex ALL DEPENDS ${tclIndex_outdir}/tclIndex)
  # Maintain a list of all tclIndex targets for dependency purposes
  set(tclIndex_target_list ${tclIndex_target_list} ${name}_tclIndex)
  list(REMOVE_DUPLICATES tclIndex_target_list)
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

CMAKEFILES(README pkgIndex.tcl tclIndex hv3_man_browser_test.tcl)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
