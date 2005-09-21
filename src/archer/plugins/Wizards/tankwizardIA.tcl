##
# Portions Copyright (c) 2002 SURVICE Engineering Company. All Rights Reserved.
# This file contains Original Code and/or Modifications of Original Code as
# defined in and that are subject to the SURVICE Public Source License
# (Version 1.3, dated March 12, 2002).
#
# Author -
#          Bob Parker
#
# Description -
#	 This is a script for loading/registering the tank wizard.
#

set brlcadDataPath [bu_brlcad_data ""]
source [file join $brlcadDataPath plugins archer Wizards tankwizardIA TankWizardIA.tcl]

# Load only once
set pluginMajorType $TankWizardIA::wizardMajorType
set pluginMinorType $TankWizardIA::wizardMinorType
set pluginName $TankWizardIA::wizardName
set pluginVersion $TankWizardIA::wizardVersion

# check to see if already loaded
set plugin [Archer::pluginQuery $pluginName]
if {$plugin != ""} {
    if {[$plugin get -version] == $pluginVersion} {
	# already loaded ... don't bother doing it again
	return
    }
}

#set iconPath [file join $brlcadDataPath plugins archer Wizards tankwizardIA images tank.png]
set iconPath ""

# register plugin with Archer's interface
Archer::pluginRegister $pluginMajorType \
    $pluginMinorType \
    $pluginName \
    "TankWizardIA" \
    "TankWizardIA.tcl" \
    "Build tank geometry (zone based)." \
    $pluginVersion \
    "SURVICE Engineering" \
    $iconPath \
    "Build a tank object" \
    "buildTank" \
    "buildTankXML"
