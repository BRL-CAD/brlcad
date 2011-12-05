IF(WIN32)
	ADD_CUSTOM_COMMAND(
		OUTPUT ${CMAKE_BINARY_DIR}/bin/archer ${CMAKE_BINARY_DIR}/bin/archer.bat
		COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/archer.bat ${CMAKE_BINARY_DIR}/bin/archer.bat
		COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/archer ${CMAKE_BINARY_DIR}/bin/archer
		DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/archer.bat ${CMAKE_CURRENT_SOURCE_DIR}/archer.bat
		)
	ADD_CUSTOM_TARGET(archer ALL DEPENDS ${CMAKE_BINARY_DIR}/bin/archer.bat ${CMAKE_BINARY_DIR}/bin/archer)
	INSTALL(PROGRAMS archer.bat DESTINATION bin)
ELSE(WIN32)
	ADD_CUSTOM_COMMAND(
		OUTPUT ${CMAKE_BINARY_DIR}/bin/archer
		COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_CURRENT_SOURCE_DIR}/archer ${CMAKE_BINARY_DIR}/bin/archer
		DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/archer
		)
	ADD_CUSTOM_TARGET(archer ALL DEPENDS ${CMAKE_BINARY_DIR}/bin/archer)
ENDIF(WIN32)
INSTALL(PROGRAMS archer DESTINATION bin)

# archer depends on the creation/installation of all the
# tclscripts.
FOREACH(item ${pkgIndex_target_list})
	ADD_DEPENDENCIES(archer ${item})
ENDFOREACH(item ${pkgIndex_target_list})
FOREACH(item ${tclIndex_target_list})
	ADD_DEPENDENCIES(archer ${item})
ENDFOREACH(item ${tclIndex_target_list})

# other archer dependencies
IF(BRLCAD_ENABLE_TK)
	ADD_DEPENDENCIES(archer bwish ${ITCL_LIBRARY} ${ITK_LIBRARY})
	get_directory_property(BRLCAD_BUILD_TKHTML DIRECTORY ${CMAKE_SOURCE_DIR}/src/other DEFINITION BRLCAD_TKHTML_BUILD)
	IF(BRLCAD_BUILD_TKHTML)
		ADD_DEPENDENCIES(archer Tkhtml)
	ENDIF(BRLCAD_BUILD_TKHTML)
	get_directory_property(BRLCAD_BUILD_TKPNG DIRECTORY ${CMAKE_SOURCE_DIR}/src/other DEFINITION BRLCAD_TKPNG_BUILD)
	IF(BRLCAD_BUILD_TKPNG)
		ADD_DEPENDENCIES(archer tkpng)
	ENDIF(BRLCAD_BUILD_TKPNG)
	get_directory_property(BRLCAD_BUILD_TKTABLE DIRECTORY ${CMAKE_SOURCE_DIR}/src/other	DEFINITION BRLCAD_TKTABLE_BUILD)
	IF(BRLCAD_BUILD_TKTABLE)
		ADD_DEPENDENCIES(archer Tktable)
	ENDIF(BRLCAD_BUILD_TKTABLE)
	get_directory_property(BRLCAD_BUILD_IWIDGETS DIRECTORY ${CMAKE_SOURCE_DIR}/src/other DEFINITION BRLCAD_IWIDGETS_BUILD)
	IF(BRLCAD_BUILD_IWIDGETS)
		ADD_DEPENDENCIES(archer iwidgets)
	ENDIF(BRLCAD_BUILD_IWIDGETS)
ENDIF(BRLCAD_ENABLE_TK)

BRLCAD_ADDFILE(plugins/Core/README plugins/archer/Core)
BRLCAD_ADDFILE(plugins/Commands/README plugins/archer/Command)

SET(archer_utility_FILES
  plugins/Utility/botUtilityP.tcl
  plugins/Utility/attrGroupsDisplayUtilityP.tcl
  plugins/Utility/README
)
BRLCAD_ADDDATA(archer_utility_FILES plugins/archer/Utility)

BRLCAD_ADDFILE(plugins/Utility/botUtilityP/BotUtilityP.tcl plugins/archer/Utility/botUtilityP)
BRLCAD_ADDFILE(plugins/Utility/attrGroupsDisplayUtilityP/AttrGroupsDisplayUtilityP.tcl	plugins/archer/Utility/attrGroupsDisplayUtilityP)

SET(archer_wizard_FILES
  plugins/Wizards/humanwizard.tcl
  plugins/Wizards/tankwizard.tcl
  plugins/Wizards/tirewizard.tcl
)
BRLCAD_ADDDATA(archer_wizard_FILES plugins/archer/Wizards)

BRLCAD_ADDFILE(plugins/Wizards/humanwizard/HumanWizard.tcl plugins/archer/Wizards/humanwizard)
BRLCAD_ADDFILE(plugins/Wizards/tankwizard/TankWizard.tcl	plugins/archer/Wizards/tankwizard)
BRLCAD_ADDFILE(plugins/Wizards/tankwizard/images/tank.png plugins/archer/Wizards/tankwizardIA/images)
BRLCAD_ADDFILE(plugins/Wizards/tirewizard/TireWizard.tcl plugins/archer/Wizards/tirewizard)

SET(archer_ignore_files
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
