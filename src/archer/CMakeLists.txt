if(BRLCAD_ENABLE_TCL)
  brlcad_adddata(archer_launch.tcl tclscripts/archer/init)

  # NOTE: Building "GUI" doesn't matter except on Windows, but on Windows archer
  # currently works only in graphical mode - might as well behave "nicely" there.
  # If/when we add MGED's ability to work in "classic" mode, Archer will have
  # to be built as a non-GUI application (or we'll have to build two executables)
  if(BRLCAD_ENABLE_TK)
    set(
      archer_libs
      libtclcad
      libbu
      ${TCL_LIBRARY}
      ${IMM32_LIBRARY}
      ${COMCTL32_LIBRARY}
    )
    set(archer_srcs archer.c)
    if(HAVE_WINDOWS_H)
      # To associate an icon with the application for Windows (needed
      # for the WiX installer) we must use an rc file.
      enable_language(RC)
      set(archer_srcs ${archer_srcs} archer.rc)
    endif(HAVE_WINDOWS_H)
    brlcad_addexec(archer "${archer_srcs}" "${archer_libs}" GUI)
    target_compile_definitions(archer PRIVATE HAVE_TK)
    add_dependencies(archer rtwizard)
    add_target_deps(archer Tkhtml Tktable itcl_pkgIndex itk_pkgIndex TclIndexBld dm_plugins)

    # Archer needs a lot of files to be copied into place to work - just have it
    # depend on ALL the managed files, rather than trying to list specifics
    add_dependencies(archer managed_files)

    # Archer needs the tclIndex and pkgIndex files set up correctly
    add_dependencies(archer TclIndexBld)

    # Archer is one of the programs that gets a start menu entry
    set_property(
      INSTALL "${BIN_DIR}/$<TARGET_FILE_NAME:archer>"
      PROPERTY CPACK_START_MENU_SHORTCUTS "Archer ${BRLCAD_VERSION}"
    )
    set_property(
      INSTALL "${BIN_DIR}/$<TARGET_FILE_NAME:archer>"
      PROPERTY CPACK_DESKTOP_SHORTCUTS "Archer ${BRLCAD_VERSION}"
    )
  endif(BRLCAD_ENABLE_TK)
endif(BRLCAD_ENABLE_TCL)

set(archer_txt archer_ack.txt)
if(BRLCAD_ENABLE_TCL)
  add_doc(archer_txt ".")
endif(BRLCAD_ENABLE_TCL)

# Archer plugins
set(
  archer_utility_FILES
  plugins/Utility/attrGroupsDisplayUtilityP.tcl
  plugins/Utility/botUtilityP.tcl
  plugins/Utility/lodUtilityP.tcl
  plugins/Utility/README
)
set(archer_wizard_FILES plugins/Wizards/humanwizard.tcl plugins/Wizards/tankwizard.tcl plugins/Wizards/tirewizard.tcl)

if(BRLCAD_ENABLE_TCL)
  brlcad_adddata(archer_utility_FILES plugins/archer/Utility)
  brlcad_adddata(archer_wizard_FILES plugins/archer/Wizards)
  brlcad_adddata(plugins/Commands/README plugins/archer/Command)
  brlcad_adddata(plugins/Core/README plugins/archer/Core)
  brlcad_adddata(plugins/Utility/attrGroupsDisplayUtilityP/AttrGroupsDisplayUtilityP.tcl plugins/archer/Utility/attrGroupsDisplayUtilityP)
  brlcad_adddata(plugins/Utility/botUtilityP/BotUtilityP.tcl plugins/archer/Utility/botUtilityP)
  brlcad_adddata(plugins/Utility/lodUtilityP/LODUtilityP.tcl plugins/archer/Utility/lodUtilityP)
  brlcad_adddata(plugins/Wizards/humanwizard/HumanWizard.tcl plugins/archer/Wizards/humanwizard)
  brlcad_adddata(plugins/Wizards/tankwizard/TankWizard.tcl plugins/archer/Wizards/tankwizard)
  brlcad_adddata(plugins/Wizards/tankwizard/images/tank.png plugins/archer/Wizards/tankwizardIA/images)
  brlcad_adddata(plugins/Wizards/tirewizard/TireWizard.tcl plugins/archer/Wizards/tirewizard)
endif(BRLCAD_ENABLE_TCL)

set(
  archer_ignore_files
  CMakeLists.txt
  ${archer_utility_FILES}
  ${archer_wizard_FILES}
  ${archer_txt}
  archer_launch.tcl
  archer.c
  archer.rc
  TODO
  archer.c
  archer.rc
  archer.ico
)
cmakefiles(${archer_ignore_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
