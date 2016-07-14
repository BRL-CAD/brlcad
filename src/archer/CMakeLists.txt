set(BRLMAN_INCLUDE_DIRS
  ${BU_INCLUDE_DIRS}
  ${TCLCAD_INCLUDE_DIRS}
  ${TCL_INCLUDE_DIRS}
  ${TK_INCLUDE_DIRS}
  )
list(REMOVE_DUPLICATES BRLMAN_INCLUDE_DIRS)
include_directories(${BRLMAN_INCLUDE_DIRS})

BRLCAD_ADDDATA(archer_launch.tcl tclscripts/archer)

set(archer_libs libtclcad libbu ${ITCL_LIBRARY} ${ITK_LIBRARY} ${TCL_LIBRARIES})

if(CPP_DLL_DEFINES)
  set(archer_libs ${archer_libs} imm32.lib comctl32.lib)
endif(CPP_DLL_DEFINES)

# NOTE: Building "GUI" doesn't matter except on Windows, but on Windows archer
# currently works only in graphical mode - might as well behave "nicely" there.
# If/when we add MGED's ability to work in "classic" mode, Archer will have
# to be built as a non-GUI application (or we'll have to build too executables)
if(BRLCAD_ENABLE_TK)
  BRLCAD_ADDEXEC(archer archer.c "${archer_libs}" GUI)
  add_dependencies(archer archer_launch_tcl_cp)
  add_dependencies(archer rtwizard)
  ADD_TARGET_DEPS(archer Tkhtml tkpng Tktable)

  foreach(item ${tclindex_target_list})
    add_dependencies(archer ${item})
  endforeach(item ${tclindex_target_list})
endif(BRLCAD_ENABLE_TK)


# Archer plugins

BRLCAD_ADDDATA(plugins/Core/README plugins/archer/Core)
BRLCAD_ADDDATA(plugins/Commands/README plugins/archer/Command)

set(archer_utility_FILES
  plugins/Utility/attrGroupsDisplayUtilityP.tcl
  plugins/Utility/botUtilityP.tcl
  plugins/Utility/lodUtilityP.tcl
  plugins/Utility/README
  )
BRLCAD_ADDDATA(archer_utility_FILES plugins/archer/Utility)

BRLCAD_ADDDATA(plugins/Utility/attrGroupsDisplayUtilityP/AttrGroupsDisplayUtilityP.tcl	plugins/archer/Utility/attrGroupsDisplayUtilityP)
BRLCAD_ADDDATA(plugins/Utility/botUtilityP/BotUtilityP.tcl plugins/archer/Utility/botUtilityP)
BRLCAD_ADDDATA(plugins/Utility/lodUtilityP/LODUtilityP.tcl plugins/archer/Utility/lodUtilityP)

set(archer_wizard_FILES
  plugins/Wizards/humanwizard.tcl
  plugins/Wizards/tankwizard.tcl
  plugins/Wizards/tirewizard.tcl
  )
BRLCAD_ADDDATA(archer_wizard_FILES plugins/archer/Wizards)

BRLCAD_ADDDATA(plugins/Wizards/humanwizard/HumanWizard.tcl plugins/archer/Wizards/humanwizard)
BRLCAD_ADDDATA(plugins/Wizards/tankwizard/TankWizard.tcl plugins/archer/Wizards/tankwizard)
BRLCAD_ADDDATA(plugins/Wizards/tankwizard/images/tank.png plugins/archer/Wizards/tankwizardIA/images)
BRLCAD_ADDDATA(plugins/Wizards/tirewizard/TireWizard.tcl plugins/archer/Wizards/tirewizard)

set(archer_ignore_files
  TODO
  archer.c
  )
CMAKEFILES(${archer_ignore_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
