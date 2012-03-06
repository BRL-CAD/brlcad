# Convenience macros for building pkgIndex.tcl and tclIndex files

MACRO(pkgIndex_BUILD name targetdir)
  string(REGEX REPLACE "/" "_" targetprefix ${targetdir})
  add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}/pkgIndex.tcl
    COMMAND btclsh ${CMAKE_SOURCE_DIR}/src/tclscripts/ampi.tcl ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}
    DEPENDS btclsh ${targetprefix}.sentinel
    )
  set(${name}_pkgIndex_deps ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}/pkgIndex.tcl)
  install(FILES ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}/pkgIndex.tcl DESTINATION ${DATA_DIR}/${targetdir})
  foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
    add_custom_command(
      OUTPUT ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${DATA_DIR}/${targetdir}/pkgIndex.tcl
      COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${DATA_DIR}/${targetdir}
      COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}/pkgIndex.tcl ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${DATA_DIR}/${targetdir}/pkgIndex.tcl
      DEPENDS btclsh ${targetprefix}.sentinel ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}/pkgIndex.tcl
      )
    set(${name}_pkgIndex_deps ${${name}_pkgIndex_deps} ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${DATA_DIR}/${targetdir}/pkgIndex.tcl)
  endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
  add_custom_target(${name}_pkgIndex.tcl ALL DEPENDS ${${name}_pkgIndex_deps})
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
  install(FILES ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}/tclIndex DESTINATION ${DATA_DIR}/${targetdir})
  set(${name}_tclIndex_deps ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}/tclIndex)
  foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
    add_custom_command(
      OUTPUT ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${DATA_DIR}/${targetdir}/tclIndex
      COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${DATA_DIR}/${targetdir}
      COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}/tclIndex ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${DATA_DIR}/${targetdir}/tclIndex
      DEPENDS btclsh ${targetprefix}.sentinel ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}/tclIndex
      )
    set(${name}_tclIndex_deps ${${name}_tclIndex_deps} ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${DATA_DIR}/${targetdir}/tclIndex)
  endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
  add_custom_target(${name}_tclIndex ALL DEPENDS ${${name}_tclIndex_deps})
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
