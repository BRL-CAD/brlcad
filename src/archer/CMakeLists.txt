IF(WIN32)
	configure_file(archer.bat ${CMAKE_BINARY_DIR}/bin/archer.bat COPYONLY)
	INSTALL(PROGRAMS archer.bat DESTINATION bin)
ENDIF(WIN32)
configure_file(archer ${CMAKE_BINARY_DIR}/bin/archer COPYONLY)
INSTALL(PROGRAMS archer DESTINATION bin)

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
