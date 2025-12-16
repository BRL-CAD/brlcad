# The auto_mkindex and pkg_mkIndex commands are called from within Tcl,
# which means we need script files.
file(
  WRITE
  "${CMAKE_BINARY_DIR}/CMakeTmp/auto_mkindex.tcl"
  "
foreach arg $argv {
puts \"Generating tclIndex in $arg\"
catch {auto_mkindex $arg *.tcl *.itcl *.itk *.sh} errout
if {![file exists \"$arg/tclIndex\"]} {
puts \"$errout\"
return -code 1
}
}"
)
file(
  WRITE
  "${CMAKE_BINARY_DIR}/CMakeTmp/pkg_mkIndex.tcl"
  "
foreach arg $argv {
puts \"Generating pkgIndex.tcl in $arg\"
catch {pkg_mkIndex -verbose $arg *.tcl *.itcl *.itk *.sh} errout
if {![file exists \"$arg/pkgIndex.tcl\"]} {
puts \"$errout\"
return -code 1
}
}"
)

# Trying to generate Tcl index files in parallel has proven impractical, and
# even the current efforts to do it in series don't seem to always succeed.
# New approach - collect all of the commands that we need to run into a file
# and execute them in series with one script and one custom target
set(index_cmds_file "${CMAKE_BINARY_DIR}/CMakeTmp/index_cmds.cmake")
file(REMOVE "${index_cmds_file}")

define_property(GLOBAL PROPERTY index_cpy_targets BRIEF_DOCS "tclindex cpy" FULL_DOCS "tclIndex cpy targets")
define_property(GLOBAL PROPERTY index_depends_files BRIEF_DOCS "index depends" FULL_DOCS "tclIndex depends files")
define_property(GLOBAL PROPERTY index_install_files BRIEF_DOCS "index install files" FULL_DOCS "tclIndex install files")

# Wrap the logic needed for defining build targets that generate
# tclIndex and pkgIndex.tcl files

macro(general_tcl_index_BUILD cmd outfile targetdir)
  # get file copy target(s) so we can make tclindex.tcl depend on them
  brlcad_get_dir_list_contents(DATA_TARGETS "${CMAKE_CURRENT_BINARY_DIR}" data_target_list)
  set_property(GLOBAL APPEND PROPERTY index_cpy_targets ${data_target_list})

  # We want to be out of date if any of the tcl files in the current directory
  # change, since both pkgIndex and tclIndex are going to scan the directory
  file(GLOB tcl_files "*.tcl")
  set_property(GLOBAL APPEND PROPERTY index_depends_files ${tcl_files})

  # Command that builds the index when the dependency is resolved.  We need
  # to use btclsh here to generate files correctly for Archer, but btclsh
  # will try to read the files we are generating.  As long as they are in a
  # valid state this is fine, but in parallel building it is possible for one
  # btclsh to try to read the partial output of another.  To avoid this, we
  # make each new target depend on all previous targets, so they are forced
  # execute one at a time.
  set(tclindex_outdir "${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}")
  file(
    APPEND
    "${index_cmds_file}"
    "execute_process(COMMAND \${BTCLSH} \"${CMAKE_BINARY_DIR}/CMakeTmp/${cmd}.tcl\" \"${tclindex_outdir}\" RESULT_VARIABLE BRES)\nif(BRES)\n  message(FATAL_ERROR \"Failed to process ${tclindex_outdir}\")\nendif()\n"
  )

  set_property(GLOBAL APPEND PROPERTY index_install_files ${targetdir}/${outfile})

  distclean("${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}/${outfile}")
endmacro(general_tcl_index_BUILD name targetdir)

# Type specific generalizations of the general_tcl_index_BUILD macro
macro(pkgIndex_BUILD targetdir)
  general_tcl_index_build(pkg_mkIndex pkgIndex.tcl "${targetdir}")
endmacro()
macro(tclIndex_BUILD targetdir)
  general_tcl_index_build(auto_mkindex tclIndex "${targetdir}")
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

set(
  tclscripts_TCLSCRIPTS
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
if(BRLCAD_ENABLE_TCL)
  brlcad_adddata(tclscripts_TCLSCRIPTS tclscripts)
  pkgindex_build(tclscripts)
  tclindex_build(tclscripts)
endif(BRLCAD_ENABLE_TCL)

# Put rtwizard last so we can define a build target
# that relies on the other tclscripts being set up.
add_subdirectory(rtwizard)

if(TARGET btclsh)
  # Unpack the full lists
  get_property(index_cpy_targets GLOBAL PROPERTY index_cpy_targets)
  get_property(index_depends_files GLOBAL PROPERTY index_depends_files)
  get_property(index_install_files GLOBAL PROPERTY index_install_files)

  # We've collected all the commands - now add add a single command
  # to run all of them at once.
  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tclindexbld.sentinel
    COMMAND ${CMAKE_COMMAND} -DBTCLSH=$<TARGET_FILE:btclsh> -P ${index_cmds_file}
    COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/tclindexbld.sentinel
    DEPENDS ${index_depends_files} ${index_cpy_targets}
  )
  add_custom_target(TclIndexBld ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/tclindexbld.sentinel)
  distclean(${CMAKE_CURRENT_BINARY_DIR}/tclindexbld.sentinel)
  set_target_properties(perm_test PROPERTIES FOLDER "Compilation Utilities")

  foreach(ifile ${index_install_files})
    # Install logic for index file.
    get_filename_component(targetdir "${ifile}" DIRECTORY)
    get_filename_component(outfile "${ifile}" NAME)
    install(FILES ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}/${outfile} DESTINATION ${DATA_DIR}/${targetdir})
  endforeach(ifile ${index_install_files})
endif(TARGET btclsh)

cmakefiles(
  CMakeLists.txt
  ${tclscripts_TCLSCRIPTS}
  ged_notfound.txt
  README
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
