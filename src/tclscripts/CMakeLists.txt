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
  if (BRLCAD_ENABLE_TCL)
    # normalize so we can use it as a name
    string(REGEX REPLACE "/" "_" name ${targetdir})

    # get file copy target(s) so we can make tclindex.tcl depend on them
    BRLCAD_GET_DIR_LIST_CONTENTS(DATA_TARGETS "${CMAKE_CURRENT_BINARY_DIR}" data_target_list)

    # We want to be out of date if any of the tcl files in the current directory
    # change, since both pkgIndex and tclIndex are going to scan the directory
    file(GLOB tcl_files "*.tcl")

    # command that builds the index when the dependency is resolved
    set(tclindex_outdir "${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/${DATA_DIR}/${targetdir}")
    add_custom_command(
      OUTPUT ${tclindex_outdir}/${outfile}
      COMMAND btclsh ${CMAKE_BINARY_DIR}/CMakeTmp/${cmd}.tcl ${tclindex_outdir} >> ${CMAKE_CURRENT_BINARY_DIR}/${cmd}_index_gen.log 2>&1
      DEPENDS btclsh ${data_target_list} ${tcl_files}
      )

    # Install logic for index file.
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

    # convenience target
    add_custom_target(${name}_${cmd}.tcl ALL DEPENDS ${tclindex_outdir}/${outfile})
    set_target_properties(${name}_${cmd}.tcl PROPERTIES FOLDER "BRL-CAD Tcl Scripts")

    # Maintain a list of all tclindex targets for dependency purposes
    set(tclindex_target_list ${tclindex_target_list} ${name}_${cmd}.tcl)
    list(REMOVE_DUPLICATES tclindex_target_list)
    set(tclindex_target_list "${tclindex_target_list}" CACHE STRING "tclindex target list" FORCE)

    set(LOG_FILES "${LOG_FILES};${CMAKE_CURRENT_BINARY_DIR}/${cmd}_index_gen.log")

    DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/${cmd}_index_gen.log)
  endif (BRLCAD_ENABLE_TCL)
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
BRLCAD_ADDDATA(tclscripts_TCLSCRIPTS tclscripts)
pkgIndex_BUILD(tclscripts)
tclIndex_BUILD(tclscripts)

# Put rtwizard last so we can define a build target
# that relies on the other tclscripts being set up.
add_subdirectory(rtwizard)

CMAKEFILES(
  CMakeLists.txt
  README
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
