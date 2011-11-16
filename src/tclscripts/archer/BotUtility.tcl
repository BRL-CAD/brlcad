#                B O T U T I L I T Y . T C L
# BRL-CAD
#
# Copyright (c) 2002-2011 United States Government as represented by
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
# Description:
#	This is an Archer class for editing a BoT primitive.
#

package require Tk
package require Itcl
package require Itk
load libbu[info sharedlibextension]

if {[catch {
    set script [file join [bu_brlcad_data "tclscripts"] boteditor botEditor.tcl]
    source $script
} errMsg] > 0} {
    puts "Couldn't load \"$script\"\n$errMsg"
} 

::itcl::class BotUtility {
    inherit Utility

    constructor {_archer _handleToplevel args} {}
    destructor {}

    # public widget variable for holding selected bot
    itk_option define -selectedbot selectedBot Selected "" {}

    public {
	# Override's for the Utility class
	common utilityMajorType $Archer::pluginMajorTypeUtility
	common utilityMinorType $Archer::pluginMinorTypeMged
	common utilityName "Bot Utility"
	common utilityVersion "1.0"
	common utilityClass BotUtility

	method editSelected {}
	method selectBot {bots}
	method editBot {bot}
    }

    protected {
	variable mHandleToplevel 0
	variable mParentBg ""
	variable mToplevel ""

	method updateBgColor {_bg}
    }

    private {
	common instances 0
    }
}

## - constructor
::itcl::body BotUtility::constructor {_archer _handleToplevel args} {
    global env

    set mHandleToplevel $_handleToplevel
    set parent [winfo parent $itk_interior]
    set mParentBg [$parent cget -background]

    # process widget options
    eval itk_initialize $args

    # search for bots
    set bots {}
    catch {set bots [$_archer search -type bot]}
    set numBots [llength $bots]

    if {$numBots == 0} {

	# no bots - show notice
	itk_component add noBots {

	    ttk::label $itk_interior.noticeLbl \
		-text {There are no bots to edit.}
	} {}
	
	grid $itk_component(noBots)

    } else {

	# select from available bots
	selectBot $bots
    }

    # manage plugin window
    set mToplevel [winfo toplevel $itk_interior]

    if {$mHandleToplevel} {

	# center and auto focus window
	BotEditor::localizeDialog $mToplevel
	BotEditor::focusOnEnter $mToplevel

	wm geometry $mToplevel "400x100"
    }
}

::itcl::body BotUtility::destructor {} {
}

::itcl::body BotUtility::updateBgColor {_bg} {
    # TODO: update background color of all widgets
}

# selectBot bots
#     Lets user pick what bot they want to edit by displaying a combobox
#     populated with the elements of the bots list argument.
#
::itcl::body BotUtility::selectBot {bots} {    
    # create container frame
    itk_component add sframe {
	ttk::frame $itk_interior.selectFrame
    } {}

    # create combobox of available bots
    itk_component add combo {
	ttk::combobox $itk_component(sframe).selectCombo -values $bots
    } {}

    # auto-select first bot
    # - initialize selectedbot variable to first bot
    # - make combobox show firstbot
    namespace eval :: "$this configure -selectedbot [lindex $bots 0]"    
    $itk_component(combo) current 0

    # will update selectedbot variable whenever user changes combobox selection
    # 0 return value prevents user from editing the combobox entry
    $itk_component(combo) configure \
        -validate all \
	-validatecommand "$this configure -selectedbot %s; return 0"

    # create button that starts editing for the selected bot
    itk_component add button {
        ttk::button $itk_component(sframe).editButton \
            -text {Edit Selected} \
	    -command "$this editSelected"
    } {}

    # display frame
    grid rowconfigure $itk_interior 0 -weight 1
    grid columnconfigure $itk_interior 0 -weight 1
    grid $itk_component(sframe) -row 0 -column 0 -sticky news

    # display select widgets
    grid rowconfigure $itk_component(sframe) 0 -weight 1
    grid columnconfigure $itk_component(sframe) 0 -weight 1
    grid $itk_component(combo) -row 0 -column 0 -sticky new -padx 5 -pady 5
    grid $itk_component(button) -row 0 -column 1 -sticky nw -padx 5
}

# editBot bot
#     Edit the bot named by the bot argument.
#
::itcl::body BotUtility::editBot {bot} {

    set top [winfo toplevel $itk_interior]
    set last [expr "[string last "." $top] - 1"]
    set root [string range $top 0 $last]

    # create new editor instance
    set editor [BotEditor $root.editor$instances $bot -prefix $root -title {Bot Utility}]
    incr instances

    # center and auto focus editor dialog
    BotEditor::localizeDialog $editor
    BotEditor::focusOnEnter $editor
}

# editSelected
#     Calls editBot with current selection.
#
#     Used as the selectBot button callback.
#
::itcl::body BotUtility::editSelected {} {

    BotUtility::editBot $itk_option(-selectedbot)

    # close original plugin window
    set top [winfo toplevel $itk_interior]
    destroy $top
    
}
	
# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
