#                 L O D U T I L I T Y P . T C L
# BRL-CAD
#
# Copyright (c) 2013-2014 United States Government as represented by
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
#	 This is a script for loading/registering the Level of Detail
#        configuration utility.
#

set filename [file join [bu_brlcad_data "plugins"] archer Utility lodUtilityP LODUtilityP.tcl]
if { ![file exists $filename] } {
    # non-tclscript resource, look in the source invocation path
    set filename [file join [bu_brlcad_data "src"] archer plugins Utility lodUtilityP LODUtilityP.tcl]
}
if { ![file exists $filename] } {
    puts "Could not load the LODUtilityP plugin, skipping $filename"
    return
}

if {[catch {source $filename} ret]} {
    puts "Could not load the LODUtilityP plugin for the following reason: $ret"
    return
}

# Load only once
set pluginMajorType $LODUtility::utilityMajorType
set pluginMinorType $LODUtility::utilityMinorType
set pluginName $LODUtility::utilityName
set pluginVersion $LODUtility::utilityVersion

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
    "LODUtilityP" \
    "LODUtilityP.tcl" \
    "Control for configuring Level of Detail Drawing." \
    $pluginVersion \
    "Army Research Laboratory" \
    $iconPath \
    "Control for configuring LOD Drawing." \
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
