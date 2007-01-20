#                T A N K W I Z A R D I A . T C L
# BRL-CAD
#
# Copyright (c) 2002-2007 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# as published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this file; see the file named COPYING for more
# information.
#
###
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

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
