if(WIN32)
  if(NOT CMAKE_CONFIGURATION_TYPES)
    add_custom_command(
      OUTPUT ${CMAKE_BINARY_DIR}/bin/archer ${CMAKE_BINARY_DIR}/bin/archer.bat
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/archer.bat ${CMAKE_BINARY_DIR}/bin/archer.bat
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/archer ${CMAKE_BINARY_DIR}/bin/archer
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/archer.bat ${CMAKE_CURRENT_SOURCE_DIR}/archer.bat
      )
    set(archer_cmd_outputs ${CMAKE_BINARY_DIR}/bin/archer ${CMAKE_BINARY_DIR}/bin/archer.bat)
  else(NOT CMAKE_CONFIGURATION_TYPES)
    foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
      string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
      add_custom_command(
	OUTPUT ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/bin/archer ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/bin/archer.bat
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/archer.bat ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/bin/archer.bat
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/archer ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/bin/archer
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/archer.bat ${CMAKE_CURRENT_SOURCE_DIR}/archer.bat
	)
      set(archer_cmd_outputs ${archer_cmd_outputs} ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/bin/archer ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/bin/archer.bat)
    endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
  endif(NOT CMAKE_CONFIGURATION_TYPES)
  install(PROGRAMS archer.bat DESTINATION bin)
  add_custom_target(archer ALL DEPENDS ${archer_cmd_outputs})
else(WIN32)
  if(NOT CMAKE_CONFIGURATION_TYPES)
    add_custom_command(
      OUTPUT ${CMAKE_BINARY_DIR}/bin/archer
      COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_CURRENT_SOURCE_DIR}/archer ${CMAKE_BINARY_DIR}/bin/archer
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/archer
      )
    set(archer_cmd_outputs ${CMAKE_BINARY_DIR}/bin/archer)
  else(NOT CMAKE_CONFIGURATION_TYPES)
    foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
      string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
      add_custom_command(
	OUTPUT ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/bin/archer
	COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_CURRENT_SOURCE_DIR}/archer ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/bin/archer
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/archer
	)
      set(archer_cmd_outputs ${archer_cmd_outputs} ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/bin/archer)
    endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
  endif(NOT CMAKE_CONFIGURATION_TYPES)
  add_custom_target(archer ALL DEPENDS ${archer_cmd_outputs})
endif(WIN32)
install(PROGRAMS archer DESTINATION bin)

# archer depends on the creation/installation of all the
# tclscripts.
foreach(item ${pkgIndex_target_list})
  ADD_DEPENDENCIES(archer ${item})
endforeach(item ${pkgIndex_target_list})
foreach(item ${tclIndex_target_list})
  ADD_DEPENDENCIES(archer ${item})
endforeach(item ${tclIndex_target_list})

# other archer dependencies
if(BRLCAD_ENABLE_TK)
  ADD_DEPENDENCIES(archer bwish ${ITCL_LIBRARY} ${ITK_LIBRARY})
  get_directory_property(BRLCAD_BUILD_TKHTML DIRECTORY ${CMAKE_SOURCE_DIR}/src/other DEFINITION BRLCAD_TKHTML_BUILD)
  if(BRLCAD_BUILD_TKHTML)
    ADD_DEPENDENCIES(archer Tkhtml)
  endif(BRLCAD_BUILD_TKHTML)
  get_directory_property(BRLCAD_BUILD_TKPNG DIRECTORY ${CMAKE_SOURCE_DIR}/src/other DEFINITION BRLCAD_TKPNG_BUILD)
  if(BRLCAD_BUILD_TKPNG)
    ADD_DEPENDENCIES(archer tkpng)
  endif(BRLCAD_BUILD_TKPNG)
  get_directory_property(BRLCAD_BUILD_TKTABLE DIRECTORY ${CMAKE_SOURCE_DIR}/src/other	DEFINITION BRLCAD_TKTABLE_BUILD)
  if(BRLCAD_BUILD_TKTABLE)
    ADD_DEPENDENCIES(archer Tktable)
  endif(BRLCAD_BUILD_TKTABLE)
endif(BRLCAD_ENABLE_TK)

BRLCAD_ADDDATA(plugins/Core/README plugins/archer/Core)
BRLCAD_ADDDATA(plugins/Commands/README plugins/archer/Command)

set(archer_utility_FILES
  plugins/Utility/botUtilityP.tcl
  plugins/Utility/attrGroupsDisplayUtilityP.tcl
  plugins/Utility/README
  )
BRLCAD_ADDDATA(archer_utility_FILES plugins/archer/Utility)

BRLCAD_ADDDATA(plugins/Utility/botUtilityP/BotUtilityP.tcl plugins/archer/Utility/botUtilityP)
BRLCAD_ADDDATA(plugins/Utility/attrGroupsDisplayUtilityP/AttrGroupsDisplayUtilityP.tcl	plugins/archer/Utility/attrGroupsDisplayUtilityP)

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
  plugins/Wizards/humanwizard
  plugins/Wizards/tankwizard
  plugins/Wizards/tirewizard
  )
CMAKEFILES(${archer_ignore_files})
CMAKEFILES(Makefile.am plugins/Makefile.am plugins/Utility/Makefile.am plugins/Wizards/Makefile.am)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
