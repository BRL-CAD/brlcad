if(WIN32)
  BRLCAD_MANAGE_FILES("archer.bat" ${BIN_DIR} EXEC)
endif(WIN32)
BRLCAD_MANAGE_FILES("archer" ${BIN_DIR} EXEC)

# archer depends on the creation/installation of all the
# tclscripts.
add_custom_target(archer ALL DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/archer)
set_target_properties(archer PROPERTIES FOLDER "BRL-CAD Executables")
add_dependencies(archer bwish rtwizard)
foreach(item ${tclindex_target_list})
  add_dependencies(archer ${item})
endforeach(item ${tclindex_target_list})

# other archer dependencies
if(BRLCAD_ENABLE_TK)
  add_dependencies(archer bwish ${ITCL_LIBRARY} ${ITK_LIBRARY})
  get_directory_property(BRLCAD_BUILD_TKHTML DIRECTORY ${CMAKE_SOURCE_DIR}/src/other DEFINITION BRLCAD_TKHTML_BUILD)
  if(BRLCAD_BUILD_TKHTML)
    add_dependencies(archer Tkhtml)
  endif(BRLCAD_BUILD_TKHTML)
  get_directory_property(BRLCAD_BUILD_TKPNG DIRECTORY ${CMAKE_SOURCE_DIR}/src/other DEFINITION BRLCAD_TKPNG_BUILD)
  if(BRLCAD_BUILD_TKPNG)
    add_dependencies(archer tkpng)
  endif(BRLCAD_BUILD_TKPNG)
  get_directory_property(BRLCAD_BUILD_TKTABLE DIRECTORY ${CMAKE_SOURCE_DIR}/src/other	DEFINITION BRLCAD_TKTABLE_BUILD)
  if(BRLCAD_BUILD_TKTABLE)
    add_dependencies(archer Tktable)
  endif(BRLCAD_BUILD_TKTABLE)
endif(BRLCAD_ENABLE_TK)

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
  archer
  archer.bat
  plugins/Commands
  plugins/Core
  plugins/Utility/attrGroupsDisplayUtilityP
  plugins/Utility/botUtilityP
  plugins/Utility/lodUtilityP
  plugins/Wizards/humanwizard
  plugins/Wizards/tankwizard
  plugins/Wizards/tirewizard
  )
CMAKEFILES(${archer_ignore_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
