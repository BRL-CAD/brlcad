if(BRLCAD_ENABLE_TCL)
  brlcad_addexec(tclcad_check_ged_cmds check_tclcad_cmds.cpp "libtclcad;libged" TEST)
  if(TARGET tclcad_check_ged_cmds)
    add_dependencies(tclcad_check_ged_cmds libtclcad ged_plugins)
  endif(TARGET tclcad_check_ged_cmds)
  brlcad_add_test(NAME tclcad_test_ged_cmds_exist COMMAND tclcad_check_ged_cmds)
  brlcad_addexec(tclcad_check_itcl check_itcl.cpp "libtclcad" TEST)
  brlcad_add_test(NAME tclcad_test_itcl_version COMMAND tclcad_check_itcl)
endif(BRLCAD_ENABLE_TCL)

cmakefiles(
  CMakeLists.txt
  check_tclcad_cmds.cpp
  check_itcl.cpp
  itcl4_gui.tcl
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
