INSTALL(PROGRAMS archer DESTINATION ${BRLCAD_INSTALL_BIN_DIR})

set(archer_plugins
  plugins/utility.tcl
  plugins/wizards.tcl
)

INSTALL(FILES ${archer_plugins} DESTINATION  ${BRLCAD_INSTALL_DATA_DIR}/plugins/archer)

INSTALL(FILES plugins/Core/README DESTINATION ${BRLCAD_INSTALL_DATA_DIR}/plugins/archer/Core)

INSTALL(FILES plugins/Commands/README DESTINATION ${BRLCAD_INSTALL_DATA_DIR}/plugins/archer/Command)


SET(archer_utility_FILES
  plugins/Utility/botUtilityP.tcl
  plugins/Utility/attrGroupsDisplayUtilityP.tcl
  plugins/Utility/README
)
INSTALL(FILES ${archer_utility_FILES} DESTINATION ${BRLCAD_INSTALL_DATA_DIR}/plugins/archer/Utility)

INSTALL(FILES plugins/Utility/botUtilityP/BotUtilityP.tcl DESTINATION ${BRLCAD_INSTALL_DATA_DIR}/plugins/archer/Utility/botUtilityP)

INSTALL(FILES plugins/Utility/attrGroupsDisplayUtilityP/AttrGroupsDisplayUtilityP.tcl DESTINATION ${BRLCAD_INSTALL_DATA_DIR}/plugins/archer/Utility/attrGroupsDisplayUtilityP)

SET(archer_wizard_FILES
  plugins/Wizards/humanwizard.tcl
  plugins/Wizards/tankwizard.tcl
  plugins/Wizards/tirewizard.tcl
)
INSTALL(FILES ${archer_wizard_FILES} DESTINATION ${BRLCAD_INSTALL_DATA_DIR}/plugins/archer/Wizards)

INSTALL(FILES plugins/Wizards/humanwizard/HumanWizard.tcl DESTINATION ${BRLCAD_INSTALL_DATA_DIR}/plugins/archer/Wizards/humanwizard)

INSTALL(FILES plugins/Wizards/tankwizard/TankWizard.tcl DESTINATION ${BRLCAD_INSTALL_DATA_DIR}/plugins/archer/Wizards/tankwizard)

INSTALL(FILES plugins/Wizards/tankwizard/images/tank.png DESTINATION ${BRLCAD_INSTALL_DATA_DIR}/plugins/archer/Wizards/tankwizardIA/images)

INSTALL(FILES plugins/Wizards/tirewizard/TireWizard.tcl DESTINATION ${BRLCAD_INSTALL_DATA_DIR}/plugins/archer/Wizards/tirewizard)
