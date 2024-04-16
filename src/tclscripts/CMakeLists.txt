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

# Trying to generate Tcl index files in parallel has proven impractical, and
# even the current efforts to do it in series don't seem to always succeed.
# New approach - collect all of the commands that we need to run into a file
# and execute them in series with one script and one custom target
set(index_cmds_file "${CMAKE_BINARY_DIR}/CMakeTmp/index_cmds.cmake")
file(REMOVE "${index_cmds_file}")

set(index_cpy_targets)
set(index_depends_files)
set(index_install_files)

# Wrap the logic needed for defining build targets that generate
# tclIndex and pkgIndex.tcl files

macro(general_tcl_index_BUILD cmd outfile targetdir)
  # get file copy target(s) so we can make tclindex.tcl depend on them
  BRLCAD_GET_DIR_LIST_CONTENTS(DATA_TARGETS "${CMAKE_CURRENT_BINARY_DIR}" data_target_list)
  list(APPEND index_cpy_targets ${data_target_list})

  # We want to be out of date if any of the tcl files in the current directory
  # change, since both pkgIndex and tclIndex are going to scan the directory
  file(GLOB tcl_files "*.tcl")
  list(APPEND index_depends_files ${tcl_files})

  # Command that builds the index when the dependency is resolved.  We need
  # to use btclsh here to generate files correctly for Archer, but btclsh
  # will try to read the files we are generating.  As long as they are in a
  # valid state this is fine, but in parallel building it is possible for one
  # btclsh to try to read the partial output of another.  To avoid this, we
  # make each new target depend on all previous targets, so they are forced
  # execute one at a time.
  if(CMAKE_CONFIGURATION_TYPES)
    set(tclindex_outdir "${CMAKE_BINARY_DIR}/\${CONFIG}/${DATA_DIR}/${targetdir}")
  else(CMAKE_CONFIGURATION_TYPES)
    set(tclindex_outdir "${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}")
  endif(CMAKE_CONFIGURATION_TYPES)
  file(APPEND "${index_cmds_file}" "execute_process(COMMAND \${BTCLSH} ${CMAKE_BINARY_DIR}/CMakeTmp/${cmd}.tcl ${tclindex_outdir})\n")

  list(APPEND index_install_files ${targetdir}/${outfile})

endmacro(general_tcl_index_BUILD name targetdir)

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
add_subdirectory(checker)
add_subdirectory(geometree)
add_subdirectory(igraph)
add_subdirectory(hv3)
add_subdirectory(lib)
add_subdirectory(lod)
add_subdirectory(mged)
add_subdirectory(plot3-dm)
add_subdirectory(sdialogs)
add_subdirectory(shotvis)
add_subdirectory(swidgets)
add_subdirectory(tcllib)
add_subdirectory(util)
add_subdirectory(misc)

set(tclscripts_TCLSCRIPTS
  cad_clrpick.tcl
  cad_dialog.tcl
  chkexterns.tcl
  fs_dialog.tk
  helpcomm.tcl
  helplib.tcl
  hoc.tcl
  html_library.tcl
  libtclcad.tcl
  man_browser.tcl
  menu_override.tcl
  mouse.tcl
  tkcon.tcl
  vmath.tcl
  )
if (BRLCAD_ENABLE_TCL)
  BRLCAD_ADDDATA(tclscripts_TCLSCRIPTS tclscripts)
  pkgIndex_BUILD(tclscripts)
  tclIndex_BUILD(tclscripts)
endif (BRLCAD_ENABLE_TCL)

# Put rtwizard last so we can define a build target
# that relies on the other tclscripts being set up.
add_subdirectory(rtwizard)

# We've collected all the commands - now add add a single command
# to run all of them at once.
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tclindexbld.sentinel
  COMMAND ${CMAKE_COMMAND} -DCONFIG=$<CONFIG> -DBTCLSH=$<TARGET_FILE:btclsh> -P ${index_cmds_file}
  COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/tclindexbld.sentinel
  DEPENDS ${index_depends_files} ${index_cpy_targets}
  )
add_custom_target(TclIndexBld ALL
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/tclindexbld.sentinel
  )
DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/tclindexbld.sentinel)

foreach(ifile ${index_install_files})
  # Install logic for index file.
  get_filename_component(targetdir "${ifile}" DIRECTORY)
  get_filename_component(outfile "${ifile}" NAME)
  if(CMAKE_CONFIGURATION_TYPES)
    # Need to use $<CONFIG> here rather than CMAKE_CFG_INTDIR
    # as install in interpreted by CMake, not by the build tool.  See
    # https://discourse.cmake.org/t/cmake-cfg-intdir-and-file-install/2169
    install(FILES ${CMAKE_BINARY_DIR}/$<CONFIG>/${DATA_DIR}/${targetdir}/${outfile} DESTINATION ${DATA_DIR}/${targetdir})
  else(CMAKE_CONFIGURATION_TYPES)
    # CMAKE_CFG_INTDIR expands to "." in non-multiconfig builds, which is why we can use it universally in
    # the add_custom_command logic.  However, $<CONFIG> may be Debug or Release even in a singlei
    # configuration.  Therefore, we use a non-config specific path if we're not in multiconfig build mode.
    install(FILES ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}/${outfile} DESTINATION ${DATA_DIR}/${targetdir})
  endif(CMAKE_CONFIGURATION_TYPES)
endforeach(ifile ${index_install_files})

CMAKEFILES(
  CMakeLists.txt
  ${tclscripts_TCLSCRIPTS}
  README
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
