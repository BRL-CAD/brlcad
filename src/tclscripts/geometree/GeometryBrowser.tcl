#!/bin/tclsh
#             G E O M E T R Y B R O W S E R . T C L
# BRL-CAD
#
# Copyright (C) 2004-2005 United States Government as represented by
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
# Library General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this file; see the file named COPYING for more
# information.
#
###
#
# Description -
#
# This is the GeometryBrowser object script.  It's the actual guts and magic
# that "is" the geometree browser.  It generates a hierarchical view of
# geometry database objects.
#
# Author -
# 	Christopher Sean Morrison
#
# Source -
#		The U.S. Army Research Laboratory
#		Aberdeen Proving Ground, Maryland  21005
#
# Bugs -
# 
# Besides a whole slew of them that have been run into with the hierarchy
# megawidget, there are a bunch of little nitpicks that would be nice to 
# see eventually get fixed.  Off the top of my head, they are as follows:
#   1) mged has no publish/subscribe model in place so we poll.  that sucks.
#   2) nice fancy icons.  need a good way to describe the difference 
#      between a combination and a region, and to distinguish different prims
#   3) better context menu bindings.  tcl or the widget has akward key 
#      bindings that require holding a mouse while clicking a second to select.
#   4) implement the right info panel.  the code for the panel is in place,
#      just absolutely no logic is implemented to describe them.  I'm thinking
#      a representative raytrace of the primitive types, all of their
#      potential parameters and current values.
#   5) weak recognition of db changes.  the hierarchy updates, but not the
#      way it probably should.  if we open two databases in a row, and then
#      open the original again, it would appear as though the widget kept
#      the prior hierarchy nodes/settings (just not visible/active).  This
#      could get nasty after working on a set of databases for a while.
#   6) color selection support (right now, it's a hard-wired list)
#

package require Itcl
package require Itk
package require Iwidgets

# go ahead and blow away the class if we are reloading
#if [ catch {delete class GeometryBrowser} error ] { 
#	puts $error
#}

package provide GeometryBrowser 1.0

class GeometryBrowser {
	inherit itk::Toplevel	

	constructor {} {}
	destructor {} 
	
	public {
		method getNodeChildren { { node "" } { updateLists "no" } } {} 
		method toggleNode { { string "" } } {} 
		method updateGeometryLists { { node "" } } {}

		method displayNode { { node "" } { display "appended" } } {} 
		method undisplayNode { { node "" } } {} 

		method setNodeColor { { node "" } { color "" } } {}		
		method setDisplayedToColor { { color "" } } {}		

		method clearDisplay {} {} 
		method autosizeDisplay {} {}
		method zoomDisplay { { zoom "in" } } {}

		method renderPreview { {rtoptions "-P4 -R -B" } } {}
		method raytracePanel {} {}
		method raytraceWizard {} {}

		method toggleAutosizing { { state "" } } {}
		method toggleAutorender { { state "" } } {}

	}

	protected {
		# hook to the idle loop callback for automatic updates
		variable _updateHook

		# menu toggle options
		variable _showAllGeometry
		variable _autoRender

		# hooks to the heirarchy pop-up menus
		variable _itemMenu
		variable _bgMenu

		# list of geometry to validate against on events, if something changes
		# we need to redraw asap
		variable _goodGeometry
		variable _badGeometry

		method prepNodeMenu {} {} 
		method getObjectType { node } {}
		method validateGeometry {} {}
	}

	private {
		variable _debug

		# track certain menu items that are dynamic
		variable _displayItemMenuIndex
		variable _setcolorItemMenuIndex
		variable _autosizeItemMenuIndex
		variable _autosizeBgMenuIndex
		variable _autorenderItemMenuIndex
		variable _autorenderBgMenuIndex

		# port used for preview fbserv rendering
		variable _fbservPort
		variable _weStartedFbserv

		# save the mged framebuffer name
		variable _mgedFramebufferId

		method rgbToHex { { rgb "0 0 0" } } {}
		method checkAutoRender {} {}
		method extractNodeName { { node "" } } {}
	}
}


###########
# begin constructor/destructor
###########

body GeometryBrowser::constructor {} {
	# used to determine the mged port number
	global port
	global mged_players

	# set to 1/0 to turn call path debug messages on/off
	set _debug 0

	set _showAllGeometry 0
	set _autoRender 0
	set _popup ""

	set _goodGeometry ""
	set _badGeometry ""

	# pick some randomly high port for preview rendering and hope it, or one of
	# its neighbors is open.

	if [ catch { set port } _fbservPort ] {
		set _fbservPort 0
	}

	set _weStartedFbserv 0

	# determine the framebuffer window id
	if { [ catch { set mged_players } _mgedFramebufferId ] } {
		puts $_mgedFramebufferId
		puts "assuming default mged framebuffer id: id_0"
		set _mgedFramebufferId "id_0"
	}
	# just in case there are more than one returned
	set _mgedFramebufferId [ lindex $_mgedFramebufferId 0 ]

	# set the window title
	$this configure -title "Geometry Browser"

	itk_component add menubar {
		menu $itk_interior.menubar 
	}

	menu $itk_interior.menubar.close -title "Close"
	$itk_interior.menubar.close add command -label "Close" -underline 0 -command ""

	# set up the adjustable sliding pane with a left and right side
	itk_component add pw_pane {
		panedwindow $itk_interior.pw_pane -orient vertical -height 512 -width 256
	}

	$itk_interior.pw_pane add left -margin 5
	$itk_interior.pw_pane add right -margin 5 -minimum 5
	# XXX keep the left side hidden away for now...
	$itk_interior.pw_pane hide right
	set children [$itk_interior.pw_pane childsite]
	set pane(0) [lindex $children 0]
#	set pane(1) [lindex $children 1]

	itk_component add cadtree {
		Hierarchy $itk_interior.cadtree \
				-labeltext "...loading..." \
				-querycommand [ code $this getNodeChildren %n yes ] \
				-imagecommand [ code $this updateGeometryLists %n ] \
				-dblclickcommand [ code $this displayNode %n ] \
				-textmenuloadcommand [ code $this prepNodeMenu ] \
				-imagemenuloadcommand [ code $this prepNodeMenu ] \
				-markforeground blue \
				-markbackground red   \
				-selectforeground black \
				-selectbackground yellow \
				-visibleitems 20x40 \
				-alwaysquery 1
	}
#				-imagemenuloadcommand [ code $this prepNodeMenu ]
#				-selectcommand [ code $this toggleNode %n ] 
#				-imagedblcommand [ code $this displayNode %n ] 

	# save hooks to the cadtree pop-up menus for efficiency and convenience
	set _itemMenu [ $itk_interior.cadtree component itemMenu ]
	set _bgMenu [ $itk_interior.cadtree component bgMenu ]


	# itemMenu and bgMenu are the two pop-up menus available from hierarchy class
	# XXX for some reason, the current call is not returing a value properly
	$_itemMenu add command \
			-label "Display" \
			-command [ code $this displayNode [ $itk_interior.cadtree current ] "appended" ]
	# save the index of the menu entry so we may modify its label later
	set _displayItemMenuIndex [ $_itemMenu index end ]

	$_itemMenu add command \
			-label "Clear & Display" \
			-command [ code $this displayNode [ $itk_interior.cadtree current ] "alone" ]

	$_itemMenu add separator

	# configure the color drop down menu for the pop-up
	itk_component add colorMenu {
		menu $_itemMenu.colorMenu -tearoff 0
	} {
		usual
		ignore -tearoff
		rename -cursor -menucursor menuCursor Cursor
	}

	$_itemMenu.colorMenu add command \
			-label "Blue" -background [ $this rgbToHex "0 0 255" ] \
			-command [ code $this setNodeColor [ $itk_interior.cadtree current ] "0 0 255" ]
	$_itemMenu.colorMenu add command \
			-label "Cyan" -background [ $this rgbToHex "0 255 255" ] \
			-command [ code $this setNodeColor [ $itk_interior.cadtree current ] "0 255 255" ]
	$_itemMenu.colorMenu add command \
			-label "Dark Blue" -background [ $this rgbToHex "50 0 175" ] \
			-command [ code $this setNodeColor [ $itk_interior.cadtree current ] "50 0 175" ]
	$_itemMenu.colorMenu add command \
			-label "Dark Red" -background [ $this rgbToHex "79 47 47" ] \
			-command [ code $this setNodeColor [ $itk_interior.cadtree current ] "79 47 47" ]
	$_itemMenu.colorMenu add command \
			-label "Forest Green" -background [ $this rgbToHex "50 145 20" ] \
			-command [ code $this setNodeColor [ $itk_interior.cadtree current ] "50 145 20" ]
	$_itemMenu.colorMenu add command \
			-label "Green" -background [ $this rgbToHex "0 255 0" ] \
			-command [ code $this setNodeColor [ $itk_interior.cadtree current ] "0 255 0" ]
	$_itemMenu.colorMenu add command \
			-label "Grey" -background [ $this rgbToHex "80 80 80" ] \
			-command [ code $this setNodeColor [ $itk_interior.cadtree current ] "80 80 80" ]
	$_itemMenu.colorMenu add command \
			-label "Light Brown" -background [ $this rgbToHex "159 159 95" ] \
			-command [ code $this setNodeColor [ $itk_interior.cadtree current ] "159 159 95" ]
	$_itemMenu.colorMenu add command \
			-label "Lime Green" -background [ $this rgbToHex "50 204 50" ] \
			-command [ code $this setNodeColor [ $itk_interior.cadtree current ] "50 204 50" ]
	$_itemMenu.colorMenu add command \
			-label "Magenta" -background [ $this rgbToHex "255 0 255" ] \
			-command [ code $this setNodeColor [ $itk_interior.cadtree current ] "255 0 255" ]
	$_itemMenu.colorMenu add command \
			-label "Orange" -background [ $this rgbToHex "204 50 50" ] \
			-command [ code $this setNodeColor [ $itk_interior.cadtree current ] "204 50 50" ]
	$_itemMenu.colorMenu add command \
			-label "Peach" -background [ $this rgbToHex "234 100 30" ] \
			-command [ code $this setNodeColor [ $itk_interior.cadtree current ] "234 100 30" ]
	$_itemMenu.colorMenu add command \
			-label "Pink" -background [ $this rgbToHex "255 145 145" ] \
			-command [ code $this setNodeColor [ $itk_interior.cadtree current ] "255 145 145" ]
	$_itemMenu.colorMenu add command \
			-label "Red" -background [ $this rgbToHex "255 0 0" ] \
			-command [ code $this setNodeColor [ $itk_interior.cadtree current ] "255 0 0" ]
	$_itemMenu.colorMenu add command \
			-label "Redish" -background [ $this rgbToHex "255 50 50" ] \
			-command [ code $this setNodeColor [ $itk_interior.cadtree current ] "255 50 50" ]
	$_itemMenu.colorMenu add command \
			-label "Tan" -background [ $this rgbToHex "200 150 100" ] \
			-command [ code $this setNodeColor [ $itk_interior.cadtree current ] "200 150 100" ]
	$_itemMenu.colorMenu add command \
			-label "Yellow" -background [ $this rgbToHex "255 255 0" ] \
			-command [ code $this setNodeColor [ $itk_interior.cadtree current ] "255 255 0" ]


	$_itemMenu add cascade -label "Set Color" -menu $_itemMenu.colorMenu
	set _setcolorItemMenuIndex [ $_itemMenu index end ]

	$_itemMenu add command \
			-label "Edit" -state disabled \
			-command [ code $this editNode [ $itk_interior.cadtree current ] ]

#	$_itemMenu add separator

#	$_itemMenu add command \
#			-label "Render Preview" -command "[ code $this renderPreview ]"
#	$_itemMenu add command \
#			-label "Raytrace Panel" -command "[ code $this raytracePanel ]"
#	$_itemMenu add command \
#			-label "Raytrace Wizard" -command "[ code $this raytraceWizard ]"

#	$_itemMenu add separator

#	$_itemMenu add command \
#			-label "Fit the View" -command "[ code $this autosizeDisplay ]"
#	$_itemMenu add command \
#			-label "Zoom In" -command "[ code $this zoomDisplay in ]"
#	$_itemMenu add command \
#			-label "Zoom Out" -command "[ code $this zoomDisplay out ]"
#	$_itemMenu add command \
#			-label "Clear View" -command "[ code $this clearDisplay ]"

#	$_itemMenu add separator

#	$_itemMenu add command \
#			-label [ $this toggleAutosizing same ] \
#			-command "[ code $this toggleAutosizing  ]"
#	# save the index of this menu entry so we may modify its label later
#	set _autosizeItemMenuIndex [ $_itemMenu index end ]
#	$_itemMenu add command \
#			-label [ $this toggleAutorender same ] \
#			-command "[ code $this toggleAutorender  ]"
#	# save the index of this menu entry so we may modify its label later
#	set _autorenderItemMenuIndex [ $_itemMenu index end ]


	# configure the color drop down menu for the pop-up
	itk_component add bgColorMenu {
		menu $_bgMenu.bgColorMenu -tearoff 0
	} {
		usual
		ignore -tearoff
		rename -cursor -menucursor menuCursor Cursor
	}

	$_bgMenu.bgColorMenu add command \
			-label "Blue" -background [ $this rgbToHex "0 0 255" ] \
			-command [ code $this setDisplayedToColor "0 0 255" ]
	$_bgMenu.bgColorMenu add command \
			-label "Cyan" -background [ $this rgbToHex "0 255 255" ] \
			-command [ code $this setDisplayedToColor "0 255 255" ]
	$_bgMenu.bgColorMenu add command \
			-label "Dark Blue" -background [ $this rgbToHex "50 0 175" ] \
			-command [ code $this setDisplayedToColor "50 0 175" ]
	$_bgMenu.bgColorMenu add command \
			-label "Dark Red" -background [ $this rgbToHex "79 47 47" ] \
			-command [ code $this setDisplayedToColor "79 47 47" ]
	$_bgMenu.bgColorMenu add command \
			-label "Green" -background [ $this rgbToHex "50 145 20" ] \
			-command [ code $this setDisplayedToColor "50 145 20" ]
	$_bgMenu.bgColorMenu add command \
			-label "Grey" -background [ $this rgbToHex "80 80 80" ] \
			-command [ code $this setDisplayedToColor "80 80 80" ]
	$_bgMenu.bgColorMenu add command \
			-label "Light Brown" -background [ $this rgbToHex "159 159 95" ] \
			-command [ code $this setDisplayedToColor "159 159 95" ]
	$_bgMenu.bgColorMenu add command \
			-label "Lime Green" -background [ $this rgbToHex "50 204 50" ] \
			-command [ code $this setDisplayedToColor "50 204 50" ]
	$_bgMenu.bgColorMenu add command \
			-label "Magenta" -background [ $this rgbToHex "255 0 255" ] \
			-command [ code $this setDisplayedToColor "255 0 255" ]
	$_bgMenu.bgColorMenu add command \
			-label "Orange" -background [ $this rgbToHex "204 50 50" ] \
			-command [ code $this setDisplayedToColor "204 50 50" ]
	$_bgMenu.bgColorMenu add command \
			-label "Peach" -background [ $this rgbToHex "234 100 30" ] \
			-command [ code $this setDisplayedToColor "234 100 30" ]
	$_bgMenu.bgColorMenu add command \
			-label "Pink" -background [ $this rgbToHex "255 145 145" ] \
			-command [ code $this setDisplayedToColor "255 145 145" ]
	$_bgMenu.bgColorMenu add command \
			-label "Red" -background [ $this rgbToHex "255 0 0" ] \
			-command [ code $this setDisplayedToColor "255 0 0" ]
	$_bgMenu.bgColorMenu add command \
			-label "Redish" -background [ $this rgbToHex "255 50 50" ] \
			-command [ code $this setDisplayedToColor "255 50 50" ]
	$_bgMenu.bgColorMenu add command \
			-label "Tan" -background [ $this rgbToHex "200 150 100" ] \
			-command [ code $this setDisplayedToColor "200 150 100" ]
	$_bgMenu.bgColorMenu add command \
			-label "Yellow" -background [ $this rgbToHex "255 255 0" ] \
			-command [ code $this setDisplayedToColor "255 255 0" ]

	$_bgMenu add cascade \
			-label "Set Color All Displayed" -menu $_bgMenu.bgColorMenu

	$_bgMenu add separator

	# Non node menu
	$_bgMenu add command \
			-label "Render Preview" -command [ code $this renderPreview ]
	$_bgMenu add command \
			-label "Raytrace Panel" -command [ code $this raytracePanel ]
	$_bgMenu add command \
			-label "Raytrace Wizard" -command [ code $this raytraceWizard ] -state disabled

	$_bgMenu add separator

	$_bgMenu add command \
			-label "Fit the View" -command "[ code $this autosizeDisplay ]"
	$_bgMenu add command \
			-label "Zoom In" -command "[ code $this zoomDisplay in ]"
	$_bgMenu add command \
			-label "Zoom Out" -command "[ code $this zoomDisplay out ]"
	$_bgMenu add command \
			-label "Clear View" -command "[ code $this clearDisplay ]"

	$_bgMenu add separator

	$_bgMenu add command \
			-label [ $this toggleAutosizing same ] \
			-command "[ code $this toggleAutosizing  ]"
	# save the index of this menu entry so we may modify its label later
	set _autosizeBgMenuIndex [ $_bgMenu index end ]

	$_bgMenu add command \
			-label [ $this toggleAutorender same ] \
			-command "[ code $this toggleAutorender  ]"
	# save the index of this menu entry so we may modify its label later
	set _autorenderBgMenuIndex [ $_bgMenu index end ]

#	itk_component add t_info {
#		text $pane(1).t_info -relief sunken -bd 1 -yscrollcommand "$pane(1).s_info set"
#	}
	
#	itk_component add s_info {
#		scrollbar $pane(1).s_info -command "$itk_component(t_info) yview"
#	}

	grid $itk_component(cadtree) -sticky nsew -in $pane(0) -row 0 -column 0
#	grid $itk_component(t_info) -sticky nsew -in $pane(1) -row 0 -column 0
#	grid $itk_component(s_info) -sticky ns -in $pane(1) -row 0 -column 1
	grid $itk_component(pw_pane) -sticky nsew
	
	grid rowconfigure $itk_interior 0 -weight 1
	grid columnconfigure $itk_interior 0 -weight 1
	grid rowconfigure $pane(0) 0 -weight 1
	grid columnconfigure $pane(0) 0 -weight 1
#	grid rowconfigure $pane(1) 0 -weight 1
#	grid columnconfigure $pane(1) 0 -weight 1

	# run redraw now and start initial updateHook
	$this validateGeometry
}


body GeometryBrowser::destructor {} {
	if { $_debug } {
		puts "destructor"
	}

#	itk_component delete [ $itk_interior component colorMenu ]
#	itk_component delete [ $itk_interior component menubar ]
#	itk_component delete [ $itk_interior component cadtree ]
#	itk_component delete [ $itk_interior component pw_pane ]
	
	# destroy the framebuffer, if we opened it
	if { $_weStartedFbserv } {
		puts "cleaning up fbserv"
		if { [ catch { exec fbfree -F $_fbservPort } error ] } {
			puts $error
			puts "Unable to properly clean up after our fbserv"
		}
	}

	# cancel our callback if it is pending
	after cancel $_updateHook

}

###########
# end constructor/destructor
###########


###########
# begin public methods
###########

# getNodeChildren is the -querycommand
#
# returns the geometry at any node in the directed tree.  it also maintains a
# list of what geometry is displayed in the browser, regardless of whether the
# geometry is actually a valid database object (it keeps a list of good and
# bad geometry).  every event notification, the list is validated (see
# validateGeometry) and if it does not match, getNodeChildren gets recalled
# to update the list and reinform geometry
#
# the second parameter controls whether the displayed geometry lists will be
# maintained or whether we are merely getting a list of children at a point
# 
body GeometryBrowser::getNodeChildren { { node "" } { updateLists "no" }} {
	if { $_debug } {
		puts "getNodeChildren $node $updateLists"
	}

	# get a list of children for the current node.  the result in childList
	# should be a list of adorned children nodes.
	set childList ""
	if {$node == ""} {

		# figure out what title to put in the label space.  XXX we could use
		# the format command to generate a nicely formatted paragraph, but
		# it's behavior is not consistent.  Plus... the titles could potentially
		# be huge and we'd still need to trim, so just trim short anyways.
		set titleLabel ""
		if [ catch { title } tit ] {
			set titleLabel "No database open"
		} else {
			set titleLabel "$tit"
		}
		if { [ string length $titleLabel ] >= 32 } {
			set titleLabel "[ format "%32.32s" [ title ] ]..."
		}
		$itk_interior.cadtree configure -labeltext $titleLabel

		# process top geometry
		
		if { $_showAllGeometry } {
			set topsCommand "tops -n"
		} else {
			# XXX the -u option only works on v6.0.1+
			set topsCommand "tops -n -u"
		}

		if [ catch $topsCommand roots ] {
			# puts $roots
			set _goodGeometry ""
			set _badGeometry ""
			return
		}

		# the children are all of the top geometry
		set childList ""
		foreach topItem $roots {
			# XXX handle the case where tops returns decorated paths
			if { [ llength [ split $topItem / ] ] >= 2 } {
				set topItem [ lindex [ split $topItem / ] 0 ]
			}
			lappend childList "/$topItem"
		}

	} else {
		# process some combination or region or primitive

		set parentName [ lindex [ split $node / ] end ]

		set childrenPairs ""
		if [ catch { lt $parentName } childrenPairs ] {
			set childrenPairs ""
		}

		foreach child $childrenPairs {
			lappend childList "$node/[ lindex $child 1 ]"
		}
	}

	# generate the final child list including determining whether the object is a
	# primitive or a combinatorial (branch or leaf node).
	set children ""
	foreach child $childList {

		set childName [ lindex [ split $child / ] end ]

		# we do not call getObjectType for performance reasons (big directories
		# can choke.  Sides, we do not need to know the type, just whether the
		# node is a branch or not.
		if { [ catch { ls $childName } lsName ] } {
			puts "$lsName"
			if { [ string compare $updateLists "yes" ] == 0 } {
				if { [ lsearch -exact $_badGeometry $childName ] == -1 } {
					lappend _badGeometry "$childName"
				}
			}
			continue
		}

		# if the node could not be stat'd with ls, mark it (in red)
		if { [ string compare $lsName "" ] == 0 } {
			# !!! for some reason the mark blabbers back that the node is not valid?!?
			# WTF?!?  And, of course, it works if type the exact same thing into mged..
			#			puts "WANT TO MARK NODE INVALID BUT CANNOT (hierarchy widget command fails):\n$itk_interior.cadtree mark add $node/$childName"
			# $itk_interior.cadtree mark add $node/$childName

			if { [ string compare $updateLists "yes" ] == 0 } {
				if { [ lsearch -exact $_badGeometry $childName ] == -1 } {
					lappend _badGeometry "$childName"
				}
			}
		}

		set nodeType leaf
		if { [ string compare "/" [ string index $lsName end-1 ] ] == 0 } {
			set nodeType branch
		} elseif { [ string compare "R" [ string index $lsName end-1 ] ] == 0 } {
			if { [ string compare "/" [ string index $lsName end-2 ] ] == 0 } {
				set nodeType branch
			}
		}

		# check whether to update good/bad lists
		if { [ string compare $updateLists "yes" ] == 0 } {
			# if we did not just add it as bad geometry, it must be good..
			if { [ string compare [ lindex $_badGeometry end ] $childName ] != 0 } {
				
				# make sure it is not a repeat
				if { [ lsearch -exact $_goodGeometry $childName ] == -1 } {
					lappend _goodGeometry "$childName"
				}
			}
		}

		lappend children [ list "$node/$childName" "$childName" $nodeType ]
	}
	# done iterating over children

	return $children
}


# toggleNode is the -clickcommand
#
# simply expands the hierarchy just as if the icon had been selected. later it
# will be able to display the appropriate details to the info panel.
# 
body GeometryBrowser::toggleNode { { node "" } } {
	if { $_debug } {
		puts "toggleNode [ lindex $node 0 ]"
	}

	$itk_interior.cadtree toggle [ lindex $node 1 ]

	return
}


# updateGeometryLists is the -imagecommand
#
# updates the lists of valid and invalid geometry based on the user expanding
# and collapsing nodes.  getNodeChildren adds new nodes as they are expanded.
# this routine removes them as they are collapsed.
# 
body GeometryBrowser::updateGeometryLists { { node "" } } {
	if { $_debug } {
		puts "updateGeometryLists $node"
	}

	if { $_showAllGeometry } {
		set topsCommand "tops -n"
	} else {
		# XXX the -u option only works on v6.0.1+
		set topsCommand "tops -n -u"
	}
	
	if [ catch $topsCommand objs ] {
		puts $objs
		return
	}
	set objects ""
	foreach obj $objs {

		# XXX handle the case where tops returns decorated paths
		if { [ llength [ split $obj / ] ] >= 2 } {
			set obj [ lindex [ split $obj / ] 0 ]
		}
		lappend objects $obj
	}
	
	# iterate over nodes displayed and generate list of geometry
	foreach currentNode [ $itk_interior.cadtree expState ] {
		set children [ $this getNodeChildren $currentNode no ]
		foreach child $children {
			set childName [ lindex $child 1 ]
			if { [ lsearch -exact $objects $childName ] == -1 } {
				lappend objects $childName
			}
		}
	}

	#regenerate good/bad lists
	set _goodGeometry ""
	set _badGeometry ""
	foreach object $objects {

		# catch an ls failure (should not happen)
		if { [ catch { ls $object } lsName ] } {
			puts "$lsName"
			if { [ lsearch -exact $_badGeometry $object ] == -1 } {
				lappend _badGeometry $object
			}
			continue
		}
		# if the node could not be stat'd with ls, add to bad list
		if { [ string compare $lsName "" ] == 0 } {
			if { [ lsearch -exact $_badGeometry $object ] == -1 } {
				lappend _badGeometry $object
			}
		} else {
			if { [ lsearch -exact $_goodGeometry $object ] == -1 } {
				lappend _goodGeometry $object
			}
		}
	}

	return
}


# displayNode is the -dblclickcommand
#
# displayes the geometry of a given node to the mged window.  later it will be
# able to display the selected geometry detailed information to a side panel.
#
# there are two special keywords for how to display the node.  it may either be
# displayed by itself with the "alone" keyword, or it may be simply added to
# the display (default) with the "appended" keyword.
# 
# XXX for some reason "node" ends up getting passed the branch/leaf identifier
# in the wrong place (non root nodes are in the wrong order)
#
body GeometryBrowser::displayNode { { node "" } { display "appended" } } {
	if { $_debug } {
		puts "displayNode $node $display"
	}

	# XXX hack to handle the pop-up window since it does not call current properly
	if { $node == "" } {
		set node [ $itk_interior.cadtree current ]
	} elseif { [ string compare $node "/" ] == 0 } {
		set node ""
	}

	# if we are still empty, we need to stop
	if { $node == "" } {
		error "Must specify a node!"
	}

	if { [ string compare $display "" ] == 0 } {
		set display "appended"
	}

	set nodeName [ $this extractNodeName $node ]

	# output so the user can see the effective text-command being used
	if { [ string compare $display "alone" ] == 0 } {
		puts "B $nodeName"
		set retval [ B $nodeName ]
		$this checkAutoRender
		return $retval
	}
	
	puts "e $nodeName"
	if { [ catch { e $nodeName } retval ] } {
		puts $retval
		return
	}
	$this checkAutoRender
	return $retval
}


# undisplayNode 
#
# does the opposite of displayNode.  It removed a node from the display if it
# is displayed.
#
body GeometryBrowser::undisplayNode { { node "" } } {
	if { $_debug } {
		puts "undisplayNode $node"
	}

	# XXX hack to handle the pop-up window since it does not call current properly
	if { $node == "" } {
		set node [ $itk_interior.cadtree current ]
	}

	set nodeName [ $this extractNodeName $node ]

	# still empty? -- cannot undisplay root so we return
	if { $nodeName == "" } {
		return
	}

	puts "erase $nodeName"
	set retval [ erase $nodeName ]
	$this checkAutoRender
	return $retval
}

# setNodeColor -- used by pop-up menu 
#
# sets the color of a particular node in the heirarchy, regardless of the node
# type.  node is the path to a piece of geometry like /all.g/compartment/reg.r
# color is expected to be 3 rgb values like "255 0 0".
#
body GeometryBrowser::setNodeColor { { node "" } { color "" } } {
	if { $_debug } {
		puts "setNodeColor $node $color"
	}

	# if we did not get a node, assume we were clicked on by the popup window
	# just like displayNode() does.
	if { [ string compare $node "" ] == 0 } {
		set node [ $itk_interior.cadtree current ]
	}

	set nodeName [ $this extractNodeName $node ] 

	# if no color was given, we unset the color
	if { [ string compare $color "" ] == 0 } {
		puts "attr rm $nodeName rgb"
		set retval [ attr rm $nodeName rgb ]
		$this checkAutoRender
		return $retval
	}

	# use comb_color if the color was specified in triplet form
	if { [ llength $color ] == 3 } {
		scan $color {%d %d %d} r g b
		puts "comb_color $nodeName $r $g $b"
		set retval [ comb_color $nodeName $r $g $b ]
		$this checkAutoRender
		return $retval
	}

	# check if we have a "slashed" color value
	if { [ llength [ split $rgb "/" ] ] == 3 } {
		puts "attr set $nodeName $rgb"
		set retval [ attr set $nodeName $rgb ]
		$this checkAutoRender
		return $retval
	}

	# color is unknown
	puts "Unrecognized color format.  Expecting either space or slash separated values (e.g. \"255 0 0\")"
	return
}


# setDisplayedToColor -- used by pop-up menu
#
# sets the color of all displayed objects to a particular color, regardless of
# the node type.  color is expected to be 3 rgb values like "255 0 0" or even in
# "slashed" style ala 255/0/0.
#
body GeometryBrowser::setDisplayedToColor { { color "" } } {
	if { $_debug } {
		puts "setDisplayedToColor $color"
	}

	if { [ catch { who } displayed ] } {
		puts "$displayed"
		return
	}

	foreach object $displayed {
		$this setNodeColor "$object" "$color"
	}

	return
}


# clearDisplay
#
# simply clears any displayed nodes from the graphics window
#
body GeometryBrowser::clearDisplay {} {
	if { $_debug } {
		puts "clearDisplay"
	}

	# verbosely describe what we are doing
	puts "Z"
	set retval [ Z ]
	$this checkAutoRender
	return $retval
}


# autosizeDisplay
#
# auto-fits the currently displayed objects to the current view size
#
body GeometryBrowser::autosizeDisplay {} {
	if { $_debug } {
		puts "autosizeDisplay"
	}

	# verbosely describe what we are doing
	puts "autoview"
	set retval [ autoview ]
	$this checkAutoRender
	return $retval
}


# zoomDisplay
#
# zooms the display in or out either in jumps via the keywords "in" and "out"
# or via a specified amount.
#
body GeometryBrowser::zoomDisplay { { zoom "in"} } {
	if { $_debug } {
		puts "zoomDisplay $zoom"
	}

	if { [ string compare $zoom "" ] == 0 } {
		set zoom "in"
	} 

	if { [ string compare $zoom "in" ] == 0 } {
		puts "zoomin"
		return [ zoomin ]
	} elseif { [ string compare $zoom "out" ] == 0 } {
		puts "zoomout"
		set retval [ zoomout ]
		$this checkAutoRender
		return $retval
	} 


	# XXX we do not check if the value is a valid number, we should
	puts "zoom $zoom"
	set retval [ zoom $zoom ]
	$this checkAutoRender
	return $retval
}


# renderPreview
#
# generates a small preview image of what geometry is presently displayed to a
# small temporary framebuffer
#
body GeometryBrowser::renderPreview { { rtoptions "-P4 -R -B" } } {

	# mged provides the port number it has available
	global port
	global mged_gui

	set size 128
	set device /dev/X
	set rgb "255 255 255"
	set rtrun ""

	# see if we can try to use the mged graphics window instead of firing up our own framebuffer
	set useMgedWindow 0
	if { [ set mged_gui($_mgedFramebufferId,fb) ] == 1 } {
		if { [ set mged_gui($_mgedFramebufferId,listen) ] == 1 } {
			set useMgedWindow 1
		}
	}

	if { $_debug } {
		if { $useMgedWindow } {
			puts "Using MGED Graphics Window for raytrace"
		} else {
			puts "Attempting to use our own fbserv"
		}
	}

	if { $useMgedWindow } {
		
		# if we previously started up a framebuffer, shut it down
		if { $_weStartedFbserv } {
			puts "cleaning up fbserv"
			if { [ catch { exec fbfree -F $_fbservPort } error ] } {
				puts $error
				puts "Unable to properly clean up after our fbserv"
			}
			set _weStartedFbserv 0
		}
		
		# if we got here, then we should be able to attach to a running framebuffer.
		scan $rgb {%d %d %d} r g b

		# make sure we are using the mged framebuffer port
		set _fbservPort [ set port ]

		# get the image dimensions of the mged graphics window
		set windowSize [ rt_set_fb_size $_mgedFramebufferId ]
		set windowWidth [ lindex [ split $windowSize x ] 0 ]
		set windowHeight [ lindex [ split $windowSize x ] 1 ]

		# scale the image to the proportional window dimensions so that the raytrace
		# "matches" the proportion size of the window.
		if { $windowWidth > $windowHeight } {
			set scaledHeight [ expr round(double($size) / double($windowWidth) * double($windowHeight)) ]
			set rtrun "rt [ split $rtoptions ] -F $_fbservPort -w$size -n$scaledHeight -C$r/$g/$b" 
		} else {
			set scaledWidth [ expr round(double($size) / double($windowHeight) * double($windowWidth)) ]
			set rtrun "rt [ split $rtoptions ] -F $_fbservPort -w$scaledWidth -n$size -C$r/$g/$b" 
		}

	} else {
		# cannot use mged graphics window for whatever reason, so make do.
		# try to fire up our own framebuffer.

		# see if there is an fbserv running
		if { [ catch { exec fbclear -c -F $_fbservPort -s$size $rgb } error ] } {

			if { $_debug } {
				puts "$error"
			}

			# failed, so try to start one
			puts "exec fbserv -S $size -p $_fbservPort -F $device > /dev/null &"
			exec fbserv -S $size -p $_fbservPort -F $device > /dev/null &
			exec sleep 1

			# keep track of the fact that we started this fbserv, so we have to clean up
			set _weStartedFbserv 1

			# try again
			if { [ catch { exec fbclear -c -F $_fbservPort $rgb } error ] } {
			
				if { $_debug } {
				puts "$error"
				}
				
				# try the next port
				incr _fbservPort
				
				# still failing, try to start one again
				puts "exec fbserv -S $size -p $_fbservPort -F $device > /dev/null &"
				exec fbserv -S $size -p $_fbservPort -F $device > /dev/null &
				exec sleep 1
				
				# try again
				if { [ catch { exec fbclear -c -F $_fbservPort $rgb } error ] } {
					
					if { $_debug } {
						puts "$error"
					}
					
					incr _fbservPort
					
					# still failing, try to start one again
					puts "exec fbserv -S $size -p $_fbservPort -F $device > /dev/null &"
					exec fbserv -S $size $_fbservPort $device > /dev/null &
					exec sleep 1
					
					# last try
					if { [ catch { exec fbclear -c -F $_fbservPort $rgb } error ] } {
						# strike three, give up!
						puts $error
						puts "Unable to attach to a framebuffer"
						
						# guess we did not really get to start it
						set _weStartedFbserv 0
						
						return
					}
				}
				# end try three
			}
			# end try two
		}
		# end try one

		# write out some status lines for status-rendering cheesily via exec.
		# these hang for some reason if sent to the mged graphics window through
		# mged (though they work fine via command-line).
		exec fbline -F $_fbservPort -r 255 -g 0 -b 0 [expr $size - 1] 0 [expr $size - 1] [expr $size - 1]
		exec fbline -F $_fbservPort -r 0 -g 255 -b 0 [expr $size - 2] 0 [expr $size - 2] [expr $size - 1]
		exec fbline -F $_fbservPort -r 0 -g 0 -b 255 [expr $size - 3] 0 [expr $size - 3] [expr $size - 1]

		# if we got here, then we were able to attach to a running framebuffer..
		scan $rgb {%d %d %d} r g b
		set rtrun "rt [ split $rtoptions ] -F $_fbservPort -s$size -C$r/$g/$b" 
	}
	# end check for using mged graphics window or using fbserv

	puts "$rtrun"	
	return [ eval $rtrun ]
}


# raytracePanel
#
# simply opens up the raytrace control panel
#
body GeometryBrowser::raytracePanel {} {

	init_Raytrace $_mgedFramebufferId

	return
}


# raytraceWizard
#
# simply fires off rtwizard 
#
body GeometryBrowser::raytraceWizard {} {
	puts "exec rtwizard &"
	return [ exec rtwizard & ]
}



# toggleAutosizing
#
# makes it so that the view is either auto-sizing or not. 
# XXX if the menu entries get more complex than what is already below (8 references),
# it should really be reorganized.
#
body GeometryBrowser::toggleAutosizing { { state "" } } {
	if { $_debug } {
		puts "toggleAutosizing $state"
	}

	global autosize

	# handle different states
	if { [ string compare $state "same" ] == 0 } {
		# this one is used by the constructor to set the label to the current state
		# we return the current state as the label
		if { [ set autosize ] == 0 } {
			return "Turn Autosizing On"
		} 
		return "Turn Autosizing Off"

	} elseif { [ string compare $state "off" ] == 0 } {
		# specifically turning the thing off
#		$_itemMenu entryconfigure $_autosizeItemMenuIndex -label "Turn Autosizing On"
		$_bgMenu entryconfigure $_autosizeBgMenuIndex -label "Turn Autosizing On"
		puts "set autosize 0"
		return [ set autosize 0 ]
	} elseif { [ string compare $state "on" ] == 0 } {
#		$_itemMenu entryconfigure $_autosizeItemMenuIndex -label "Turn Autosizing Off"
		$_bgMenu entryconfigure $_autosizeBgMenuIndex -label "Turn Autosizing Off"
		puts "set autosize 1"
		return [ set autosize 1 ]
	}

	if { [ set autosize ] == 0 } {
#		$_itemMenu entryconfigure $_autosizeItemMenuIndex -label "Turn Autosizing Off"
		$_bgMenu entryconfigure $_autosizeBgMenuIndex -label "Turn Autosizing Off"
		puts "set autosize 1"
		return [ set autosize 1 ]
	}

#	$_itemMenu entryconfigure $_autosizeItemMenuIndex -label "Turn Autosizing On"
	$_bgMenu entryconfigure $_autosizeBgMenuIndex -label "Turn Autosizing On"
	puts "set autosize 0"
	return [ set autosize 0 ]
}



# toggleAutorender
#
# makes it so that the view is either auto-sizing or not. 
# XXX if the menu entries get more complex than what is already below (8 references),
# it should really be reorganized.
#
body GeometryBrowser::toggleAutorender { { state "" } } {
	if { $_debug } {
		puts "toggleAutorender $state"
	}

	# handle different states
	if { [ string compare $state "same" ] == 0 } {
		# this one is used by the constructor to set the label to the current state
		# we return the current state as the label
		if { [ set _autoRender ] == 0 } {
			return "Turn Auto-Render On"
		} 
		return "Turn Auto-Render Off"

	} elseif { [ string compare $state "off" ] == 0 } {
		# specifically turning the thing off
#		$_itemMenu entryconfigure $_autorenderItemMenuIndex -label "Turn Auto-Render On"
		$_bgMenu entryconfigure $_autorenderBgMenuIndex -label "Turn Auto-Render On"
		puts "set _autoRender 0"
		set retval [ set _autoRender 0 ]
		$this checkAutoRender
		return $retval

	} elseif { [ string compare $state "on" ] == 0 } {
#		$_itemMenu entryconfigure $_autorenderItemMenuIndex -label "Turn Auto-Render Off"
		$_bgMenu entryconfigure $_autorenderBgMenuIndex -label "Turn Auto-Render Off"
		puts "set _autoRender 1"
		set retval [ set _autoRender 1 ]
		$this checkAutoRender
		return $retval
	}

	# we are not handling a specific request, so really toggle whatever the real
	# value is.

	if { [ set _autoRender ] == 0 } {
#		$_itemMenu entryconfigure $_autorenderItemMenuIndex -label "Turn Auto-Render Off"
		$_bgMenu entryconfigure $_autorenderBgMenuIndex -label "Turn Auto-Render Off"
		puts "set _autoRender 1"
		set retval [ set _autoRender 1 ]
		$this checkAutoRender
		return $retval
	}

#	$_itemMenu entryconfigure $_autorenderItemMenuIndex -label "Turn Auto-Render On"
	$_bgMenu entryconfigure $_autorenderBgMenuIndex -label "Turn Auto-Render On"
	puts "set _autoRender 0"
	set retval [ set _autoRender 0 ]
	$this checkAutoRender
	return $retval
}

##########
# end public methods
##########


##########
# begin protected methods
##########

# prepNodeMenu is the -textmenuloadcommand
#
# does something with the right-click menu, before it actually gets displayed
# based on what was selected.  e.g. it handles the dynamic menu items.
# 
body GeometryBrowser::prepNodeMenu { } {
	if { $_debug } {
		puts "prepNodeMenu"
	}
	
	set node "[ $itk_interior.cadtree current ]"

	set nodeName [ $this extractNodeName $node ]

	# see if we are a primitive, comb, or region
	if { [ catch { $this getObjectType $nodeName } type ] } {
		puts "$type"
		return
	}
	
	# we are a primitive, don't allow user to set color
	if { [ string compare $type "p" ] == 0 } {
		$_itemMenu entryconfigure $_setcolorItemMenuIndex -state disabled
	} else {
		$_itemMenu entryconfigure $_setcolorItemMenuIndex -state normal
	}

	# get a list of what is displayed
	if { [ catch { who } displayed ] } {
		puts "$displayed"
		return
	}

	# iterate over the displayed objects and see if our selected node is one
	foreach object $displayed {
		if { [ string compare $object $nodeName ] == 0 } {
			# match ! -- need to update menu to be "Remove from display"

			# update the menu entry and modify the command to use undisplayNode
			$_itemMenu entryconfigure $_displayItemMenuIndex -label "Remove from Display"
			$_itemMenu entryconfigure $_displayItemMenuIndex -command [ code $this undisplayNode $node ]
			return
		}
	}
	
	# did not find it, so make sure the menu is proper
	$_itemMenu entryconfigure $_displayItemMenuIndex -label "Display"
	$_itemMenu entryconfigure $_displayItemMenuIndex -command [ code $this displayNode $node ]

	return
}


# getObjectType 
#
# returns a "p", "c", or "r" depending on whether a node is a primitive,
# returns a "E" if the node cannot be stat'd via ls.
# combination, or region.
# 
body GeometryBrowser::getObjectType { node } {
	if { $_debug } {
		puts "getObjectType $node"
	}

	# get just the name, drop the path, if it is there
	set nodeName [ lindex [ split [ lindex $node 0 ] / ] end ]

	if { [ catch { ls $nodeName } lsName ] } {
		error $lsName
	}
	if { $lsName == "" } {
		puts "Unable to perform ls command on $nodeName"
		return "E"
	}

	set nodeType "leaf"
	if { [ string compare "/" [ string index $lsName end-1 ] ] == 0 } {
		return "c"
	} elseif { [ string compare "R" [ string index $lsName end-1 ] ] == 0 } {
		if { [ string compare "/" [ string index $lsName end-2 ] ] == 0 } {
			return "r"
		}
	}	 
	return "p"
}

# validateGeometry
#
# iterates over all of the geometry actually discovered and makes sure that
# nothing has changed
#
# XXX since there is no publish/subscribe model set up in mged to get
# interrupt notifications to update our model/view, we manually poll (eck..)
# periodically.  
#
body GeometryBrowser::validateGeometry { } {
	if { $_debug } {
		puts "validateGeometry"
	}

	set redraw 0
	set dbNotOpen 0

	# see if the database is open
	if [ catch { opendb } notopen ] {
		set dbNotOpen 1
	}
	if { [ string compare $notopen "" ] == 0 } {
		set dbNotOpen 1
	}

	# do nothing until a database is open
	if { $dbNotOpen == 0 } {

		# if there is no good or bad geometry listed, try to redraw regardless
		if { $_goodGeometry == "" } {
			incr redraw 
		} else {
			# make sure our lists match up, the lists are maintained by getNodeChildren
			# and updateGeometryLists.
			foreach geom $_goodGeometry {
				# only need one redraw command
				if { $redraw == 0 } {
					if { [ catch { ls $geom } shouldFind ] } {
						incr redraw
					}
					if { $shouldFind == "" } {
						incr redraw
					}
				}
			}
#  XXX we need a better way than "ls" to stat objects to avoid the garbage
#  that is output.
#			foreach geom $_badGeometry {
#				if { $redraw == 0 } {
#					if { ! [ catch { ls $geom } shouldNot ] } {
#						incr redraw
#					}
#				}
#			}
		}

		# redraw, invokes a recall of getNodeChildren for all displayed nodes
		if { $redraw > 0 } {
			# if a database is open, recompute the display lists
			if { $dbNotOpen == 0 } {
				$this updateGeometryLists
			} else {
				set _goodGeometry ""
				set _badGeometry ""
			}
			# !!! for some reason, one fails, but two is ok.. (!)
			# heh.. silly tcl, tk's for kids.
			$itk_interior.cadtree draw
			$itk_interior.cadtree draw
		}
	}

	# cancel any prior pending hook
	after cancel _updateHook

	# if the database is not open, poll a little slower
	if { $dbNotOpen == 1 } {
		# set up the next hook for 7 seconds
		set _updateHook [ after 7000 [ code $this validateGeometry ] ]
	} else {
		# set up the next hook for 4 seconds
		set _updateHook [ after 4000 [ code $this validateGeometry ] ]
	}
	return
}


##########
# end protected methods
##########

##########
# begin private methods
##########

# rgbToHex 
#
# takes an rgb triplet string as input and returns an html-style
# color entity that tcl understands.
# 
body GeometryBrowser::rgbToHex { { rgb "0 0 0" } } { 
	if { $_debug } {
		puts "rgbToHex $rgb"
	}

	if { [ string compare $rgb "" ] == 0 } {
		set rgb "0 0 0"
	}

	scan $rgb {%d %d %d} r g b

	# clamp the values between 0 and 255
	if { [ expr $r > 255 ] } {
		set r 255
	} elseif { [ expr $r < 0 ] } {
		set r 0
	}
	if { [ expr $g > 255 ] } {
		set g 255
	} elseif { [ expr $g < 0 ] } {
		set g 0
	}
	if { [ expr $b > 255 ] } {
		set b 255
	} elseif { [ expr $b < 0 ] } {
		set b 0
	}

	if [ catch { format {#%02x%02x%02x} $r $g $b } color ] {
		puts $color
		return "#000000"
	}

	return $color
}


# checkAutoRender
#
# simply checks if a render needs to occur automatically
#
body GeometryBrowser::checkAutoRender {} {
	if { $_debug } {
		puts "checkAutoRender"
	}

	if { $_autoRender == 1 } {
		$this renderPreview
	}

	return
}

# extractNodeName
#
# handles the oddball node name that gets passed in from the hierarchy widget
# during callback.  The nodes are expected to be named in a directory-style
# slashed format and are optionally preceeded or followed by a "branch" or 
# "leaf" identifier. (XXX roots are preceeded, children are followed.. ! )
#
body GeometryBrowser::extractNodeName { { node "" } } {
	if { $_debug } {
		puts "extractNodeName $node"
	}

	# assume empty is root node
	if { [ string compare $node "" ] == 0 } {
		set $node "/"
	}

	# handle the extra nodetype keyword nastiness
	set wordOne [ lindex [ split [ lindex $node 0 ] / ] end ]
	set wordTwo [ lindex [ split [ lindex $node 1 ] / ] end ]
	set nodeName "$wordOne"
	if { [ llength $node ] > 0 } {
		if { [ string compare "branch" $wordOne ] == 0 } {
			set nodeName $wordTwo
		} elseif { [ string compare "leaf" $wordOne ] == 0 } {
			set nodeName $wordTwo
		}
	}

	return $nodeName
}

##########
# end private methods
##########


#GeometryBrowser .gm 

# !!! remove glob hack once included into menu properly
# restore previous globbing mode
#if { [ catch { set glob_compat_mode [ set prevGlobCompatMode ] } error ] } {
#	puts "unable to restore glob_compat_mode?"
#}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
