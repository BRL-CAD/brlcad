IF(WIN32)
	configure_file(archer.bat ${CMAKE_BINARY_DIR}/bin/archer.bat COPYONLY)
	INSTALL(PROGRAMS archer.bat DESTINATION bin)
ENDIF(WIN32)
configure_file(archer ${CMAKE_BINARY_DIR}/bin/archer COPYONLY)
INSTALL(PROGRAMS archer DESTINATION bin)

set(archer_plugins
  plugins/utility.tcl
  plugins/wizards.tcl
)
BRLCAD_ADDDATA(archer_plugins plugins/archer)

FILE(COPY plugins/Core/README DESTINATION ${CMAKE_BINARY_DIR}/${DATA_DIR}/plugins/archer/Core)
INSTALL(FILES plugins/Core/README DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}/plugins/archer/Core)

FILE(COPY plugins/Commands/README DESTINATION ${CMAKE_BINARY_DIR}/${DATA_DIR}/plugins/archer/Command)
INSTALL(FILES plugins/Commands/README DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}/plugins/archer/Command)


SET(archer_utility_FILES
  plugins/Utility/botUtilityP.tcl
  plugins/Utility/attrGroupsDisplayUtilityP.tcl
  plugins/Utility/README
)
BRLCAD_ADDDATA(archer_utility_FILES plugins/archer/Utility)

FILE(COPY plugins/Utility/botUtilityP/BotUtilityP.tcl DESTINATION ${CMAKE_BINARY_DIR}/${DATA_DIR}/plugins/archer/Utility/botUtilityP)
INSTALL(FILES plugins/Utility/botUtilityP/BotUtilityP.tcl DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}/plugins/archer/Utility/botUtilityP)

FILE(COPY plugins/Utility/attrGroupsDisplayUtilityP/AttrGroupsDisplayUtilityP.tcl DESTINATION ${CMAKE_BINARY_DIR}/${DATA_DIR}/plugins/archer/Utility/attrGroupsDisplayUtilityP)
INSTALL(FILES plugins/Utility/attrGroupsDisplayUtilityP/AttrGroupsDisplayUtilityP.tcl DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}/plugins/archer/Utility/attrGroupsDisplayUtilityP)

SET(archer_wizard_FILES
  plugins/Wizards/humanwizard.tcl
  plugins/Wizards/tankwizard.tcl
  plugins/Wizards/tirewizard.tcl
)
BRLCAD_ADDDATA(archer_wizard_FILES plugins/archer/Wizards)

FILE(COPY plugins/Wizards/humanwizard/HumanWizard.tcl DESTINATION ${CMAKE_BINARY_DIR}/${DATA_DIR}/plugins/archer/Wizards/humanwizard)
INSTALL(FILES plugins/Wizards/humanwizard/HumanWizard.tcl DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}/plugins/archer/Wizards/humanwizard)

FILE(COPY plugins/Wizards/tankwizard/TankWizard.tcl DESTINATION ${CMAKE_BINARY_DIR}/${DATA_DIR}/plugins/archer/Wizards/tankwizard)
INSTALL(FILES plugins/Wizards/tankwizard/TankWizard.tcl DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}/plugins/archer/Wizards/tankwizard)

FILE(COPY plugins/Wizards/tankwizard/images/tank.png DESTINATION ${CMAKE_BINARY_DIR}/${DATA_DIR}/plugins/archer/Wizards/tankwizardIA/images)
INSTALL(FILES plugins/Wizards/tankwizard/images/tank.png DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}/plugins/archer/Wizards/tankwizardIA/images)

FILE(COPY plugins/Wizards/tirewizard/TireWizard.tcl DESTINATION ${CMAKE_BINARY_DIR}/${DATA_DIR}/plugins/archer/Wizards/tirewizard)
INSTALL(FILES plugins/Wizards/tirewizard/TireWizard.tcl DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}/plugins/archer/Wizards/tirewizard)
