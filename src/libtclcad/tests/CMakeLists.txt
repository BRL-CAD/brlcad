if(BRLCAD_ENABLE_TCL)
  brlcad_addexec(tclcad_check_ged_cmds check_tclcad_cmds.cpp "libtclcad;libged" TEST)
  if(TARGET tclcad_check_ged_cmds)
    add_dependencies(tclcad_check_ged_cmds libtclcad ged_plugins)
  endif(TARGET tclcad_check_ged_cmds)
  brlcad_add_test(NAME tclcad_test_ged_cmds_exist COMMAND tclcad_check_ged_cmds)
  brlcad_addexec(tclcad_check_itcl check_itcl.cpp "libtclcad" TEST)
  brlcad_addexec(tclcad_check_multi_interp check_multi_interp.cpp "libtclcad" TEST)
  if(TARGET m35.g)
    add_dependencies(tclcad_check_multi_interp m35.g)
  endif()
  brlcad_add_test(NAME tclcad_test_itcl_version COMMAND tclcad_check_itcl)
  brlcad_add_test(NAME tclcad_test_multi_interp COMMAND tclcad_check_multi_interp)

  if(BRLCAD_ENABLE_TK AND
     BRLCAD_X11_PROVIDER_RESOLVED STREQUAL "XMIN" AND
     Xmin_RUN_EXECUTABLE)
    brlcad_add_gui_test(
      NAME tclcad_test_multi_interp_gui
      SCREEN 1280x960x24
      TIMEOUT 120
      COMMAND $<TARGET_FILE:tclcad_check_multi_interp> --gui
    )
  endif()
endif(BRLCAD_ENABLE_TCL)

cmakefiles(
  CMakeLists.txt
  check_tclcad_cmds.cpp
  check_itcl.cpp
  check_multi_interp.cpp
  itcl4_gui.tcl
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
