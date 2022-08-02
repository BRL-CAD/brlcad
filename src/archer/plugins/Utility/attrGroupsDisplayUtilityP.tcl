#                A T T R G R O U P S D I S P L A Y U T I L I T Y P . T C L
# BRL-CAD
#
# Copyright (c) 2002-2022 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# version 2.1 as published by the Free Software Foundation.
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
# Description -
#	 This is a script for loading/registering the attribute groups display utility.
#

set brlcadDataPath [file join [bu_dir data] plugins]
set filename [file join $brlcadDataPath archer Utility attrGroupsDisplayUtilityP AttrGroupsDisplayUtilityP.tcl]
if { ![file exists $filename] } {
    puts "Could not load the AttrGroupsDisplayUtilityP plugin, skipping $filename"
    return
}

if {[catch {source $filename} ret]} {
    puts "Could not load the AttrGroupsDisplayUtilityP plugin for the following reason: $ret"
    return
}

# Load only once
set pluginMajorType $AttrGroupsDisplayUtility::utilityMajorType
set pluginMinorType $AttrGroupsDisplayUtility::utilityMinorType
set pluginName $AttrGroupsDisplayUtility::utilityName
set pluginVersion $AttrGroupsDisplayUtility::utilityVersion

# check to see if already loaded
set plugin [Archer::pluginQuery $pluginName]
if {$plugin != ""} {
    if {[$plugin get -version] == $pluginVersion} {
	# already loaded ... don't bother doing it again
	return
    }
}

set iconPath ""

# register plugin with Archer's interface
Archer::pluginRegister $pluginMajorType \
    $pluginMinorType \
    $pluginName \
    "AttrGroupsDisplayUtilityP" \
    "AttrGroupsDisplayUtilityP.tcl" \
    "Control for displaying geometry using attributes" \
    $pluginVersion \
    "Army Research Laboratory" \
    $iconPath \
    "Control for displaying geometry using attributes" \
    "" \
    ""

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
