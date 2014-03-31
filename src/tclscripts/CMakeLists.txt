# The auto_mkindex and pkg_mkIndex commands are called from within Tcl,
# which means we need script files.
file(WRITE "${CMAKE_BINARY_DIR}/CMakeTmp/auto_mkindex.tcl" "
foreach arg $argv {
puts \"Generating tclIndex in $arg\"
catch {auto_mkindex $arg *.tcl *.itcl *.itk *.sh} errout
if {![file exists \"$arg/tclIndex\"]} {
   puts \"$errout\"
   return -code 1
}
}")
file(WRITE "${CMAKE_BINARY_DIR}/CMakeTmp/pkg_mkIndex.tcl" "
foreach arg $argv {
puts \"Generating pkgIndex.tcl in $arg\"
catch {pkg_mkIndex -verbose $arg *.tcl *.itcl *.itk *.sh} errout
if {![file exists \"$arg/pkgIndex.tcl\"]} {
   puts \"$errout\"
   return -code 1
}
}")

# Wrap the logic needed for defining build targets that generate
# tclIndex and pkgIndex.tcl files

macro(general_tcl_index_BUILD cmd outfile targetdir)
  # normalize so we can use it as a name
  string(REGEX REPLACE "/" "_" name ${targetdir})

  # divert output to diff paths depending on whether debug or release
  if(CMAKE_CONFIGURATION_TYPES)
    set(tclindex_outdir "${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/${DATA_DIR}/${targetdir}")
  else(CMAKE_CONFIGURATION_TYPES)
    set(tclindex_outdir "${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}")
  endif(CMAKE_CONFIGURATION_TYPES)

  # get file list so we can make tclindex.tcl depend on them
  BRLCAD_GET_DIR_LIST_CONTENTS(DATA_TARGETS "${CMAKE_CURRENT_BINARY_DIR}" data_target_list)

  # command that builds the index when the dependency is resolved
  add_custom_command(
    OUTPUT ${tclindex_outdir}/${outfile}
    COMMAND btclsh ${CMAKE_BINARY_DIR}/CMakeTmp/${cmd}.tcl ${tclindex_outdir}
    DEPENDS btclsh ${data_target_list}
    )

  # put tclindex.tcl where it goes
  install(FILES ${tclindex_outdir}/${outfile} DESTINATION ${DATA_DIR}/${targetdir})

  # convenience target
  add_custom_target(${name}_${cmd}.tcl ALL DEPENDS ${tclindex_outdir}/${outfile})
  set_target_properties(${name}_${cmd}.tcl PROPERTIES FOLDER "BRL-CAD Tcl Scripts")

  # Maintain a list of all tclindex targets for dependency purposes
  set(tclindex_target_list ${tclindex_target_list} ${name}_${cmd}.tcl)
  list(REMOVE_DUPLICATES tclindex_target_list)
  set(tclindex_target_list "${tclindex_target_list}" CACHE STRING "tclindex target list" FORCE)
endmacro(general_tcl_index_BUILD name targetdir)
mark_as_advanced(tclindex_target_list)

# Type specific generalizations of the general_tcl_index_BUILD macro
macro(pkgIndex_BUILD targetdir)
  general_tcl_index_BUILD(pkg_mkIndex pkgIndex.tcl "${targetdir}")
endmacro()
macro(tclIndex_BUILD targetdir)
  general_tcl_index_BUILD(auto_mkindex tclIndex "${targetdir}")
endmacro()

# Now that the macros are defined, we can add tclscript subdirs
add_subdirectory(archer)
add_subdirectory(boteditor)
add_subdirectory(geometree)
add_subdirectory(igraph)
add_subdirectory(hv3)
add_subdirectory(lib)
add_subdirectory(lod)
add_subdirectory(mged)
add_subdirectory(pl-dm)
add_subdirectory(sdialogs)
add_subdirectory(swidgets)
add_subdirectory(util)

set(tclscripts_TCLSCRIPTS
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
  tkcon.tcl
  vmath.tcl
  )
BRLCAD_ADDDATA(tclscripts_TCLSCRIPTS tclscripts)
pkgIndex_BUILD(tclscripts)
tclIndex_BUILD(tclscripts)

# Put rtwizard last so we can define a build target
# that relies on the other tclscripts being set up.
add_subdirectory(rtwizard)

CMAKEFILES(README hv3_man_browser_test.tcl)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
