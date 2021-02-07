#                     M E N U . T C L
# BRL-CAD
#
# Copyright (c) 2018-2021 United States Government as represented by
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
#
# This is the Overlap Menu which is used to run the overlap file tool, browse
# for any existing overlaps file and run the checker tool to evaluate the
# overlaps.
#
package require Tk
package require Itcl
package require Itk
package require OverlapFileTool
package require GeometryChecker

# replace existing class
catch {delete class OverlapMenu} error

::itcl::class OverlapMenu {
    inherit ::itk::Widget
    constructor { args } {}
    public {
	variable firstFlag
	variable ovfile
	method browseOverlapFile {} {}
	method loadOverlapFile { filename } {}
	method runOvFileTool {} {}
	method runCheckerTool {} {}
    }
    private {
	variable _hintText
	method handleHintText { text } {}
    }
}

::itcl::body OverlapMenu::constructor { args } {
    set firstFlag false
    set ovfile ""

    itk_component add buttonsFrame {
	ttk::frame $itk_interior.buttonsFrame -padding 4
    } {}
    itk_component add newFileFrame {
	ttk::labelframe $itk_component(buttonsFrame).newFileFrame -padding 8 -text " New File "
    } {}
    itk_component add buttonRunOvFileGen {
	ttk::button $itk_component(newFileFrame).buttonRunOvFileGen \
	-text "Create New Overlaps File" -padding 8 -command [ code $this runOvFileTool ]
    } {}
    itk_component add existingFileFrame {
	ttk::labelframe $itk_component(buttonsFrame).existingFileFrame -padding 8 -text " Exisiting File "
    } {}
    itk_component add buttonBrowse {
	ttk::button $itk_component(existingFileFrame).buttonBrowse \
	-text "Browse Overlaps File" -padding 8 -command [ code $this browseOverlapFile ]
    } {}
    itk_component add buttonLastFile {
	ttk::button $itk_component(existingFileFrame).buttonLastFile \
	-text "Use Last File" -padding 8 -state disabled -command [ code $this runCheckerTool ]
    } {}
    itk_component add hintLabel {
	ttk::label $itk_interior.hintLabel \
	-textvariable [scope _hintText] -justify center -wraplength 500 \
	-padding 10
    } {}

    eval itk_initialize $args

    grid $itk_component(buttonsFrame) -sticky ew
    grid $itk_component(hintLabel)
    grid columnconfigure $itk_interior 0 -weight 1

    grid $itk_component(newFileFrame) $itk_component(existingFileFrame) -padx 4

    grid $itk_component(buttonRunOvFileGen)

    grid $itk_component(buttonLastFile) $itk_component(buttonBrowse) -padx 2

    bind $itk_component(buttonRunOvFileGen) <Enter> [code $this handleHintText "Creates a new overlaps file with specified objects and runs checker tool on the created overlaps file"]
    bind $itk_component(buttonRunOvFileGen) <Leave> [code $this handleHintText ""]

    bind $itk_component(buttonBrowse) <Enter> [code $this handleHintText "Select an overlaps file and run checker tool\n"]
    bind $itk_component(buttonBrowse) <Leave> [code $this handleHintText ""]

    #load default hint text
    $this handleHintText ""

    #check for overlap file
    set db_path [eval opendb]
    set dir [file dirname $db_path]
    set name [file tail $db_path]
    set ol_path [file join $dir "${name}.ck" "ck.${name}.overlaps"]
    if {[file exists $ol_path]} {
	set ovfile $ol_path
    }
    $this loadOverlapFile $ovfile

    bind $itk_component(buttonLastFile) <Enter> [code $this handleHintText "Run checker tool on previously created overlaps file\n$ovfile"]
    bind $itk_component(buttonLastFile) <Leave> [code $this handleHintText ""]
}

###########
# begin public methods
###########

# runCheckerTool
#
# runs the checker tool
#
body OverlapMenu::runCheckerTool { } {
    set parent ""

    if {[winfo exists $parent.checker]} {
	destroy $parent.checker
    }

    set checkerWindow [toplevel $parent.checker]
    set checker [GeometryChecker $checkerWindow.ck]

    $checker setMode $firstFlag
    $checker registerWhoCallback [code who]
    $checker registerDrawCallbacks [code drawLeft] [code drawRight]
    $checker registerEraseCallback [code erase]
    $checker registerOverlapCallback [code subtractRightFromLeft]

    if {[catch {$checker loadOverlaps $ovfile} result]} {
	wm withdraw $checkerWindow
	destroy $checkerWindow
	return -code error $result
    }

    if {$firstFlag} {
	puts "WARNING: Running with -F means check will assume that only the first unioned"
	puts "         solid in a region is responsible for any overlap. When subtracting"
	puts "         region A from overlapping region B, the first unioned solid in A will"
	puts "         be subtracted from the first unioned solid in B. This may cause the"
	puts "         wrong volume to be subtracted, leaving the overlap unresolved."
	puts ""
    }

    wm title $checkerWindow "Geometry Checker"
    pack $checker -expand true -fill both

    # calculate default geometry
    update

    # ensure window isn't too narrow
    set geom [split [wm geometry $checkerWindow] "=x+-"]
    if {[lindex $geom 0] > [lindex $geom 1]} {
	lreplace $geom 1 1 [lindex $geom 0]
    }
    wm geometry $checkerWindow "=[::tcl::mathfunc::round [expr 1.62 * [lindex $geom 1]]]x[lindex $geom 1]"

    # raise to front
    wm deiconify $checkerWindow

    destroy $parent.overlapmenu
}

# runOvFileTool
#
# runs the overlaps file tool
#
body OverlapMenu::runOvFileTool { } {
    set parent ".overlapmenu"
    if {[winfo exists $parent.overlapfiletool]} {
	destroy $parent.overlapfiletool
    }
    set overlapfilegenWindow [toplevel $parent.overlapfiletool]
    set overlapfiletool [OverlapFileTool $overlapfilegenWindow.overlapfiletool]

    wm title $overlapfilegenWindow "Overlap File Tool"
    pack $overlapfiletool -expand true -fill both
    $overlapfiletool configure -firstFlag $firstFlag
    $overlapfiletool configure -runCheckCallback [code $this runCheckerTool]
    grab set $overlapfilegenWindow
}

# browseOverlapFile
#
# opens the openfile window to select the overlaps file
#
body OverlapMenu::browseOverlapFile { } {
    set filename [tk_getOpenFile -filetypes { { {Overlaps File} {.overlaps} TEXT } }]

    if {$filename eq ""} {
	return
    }

    if { [validateOvFile $filename] == 0 } { 
	set ovfile $filename
	$this runCheckerTool
    }
}

# loadOverlapFile
#
# looks for overlaps file
#
body OverlapMenu::loadOverlapFile { filename } {
    if {$filename eq ""} {
	return
    }
    set ovfile $filename
    $itk_component(buttonLastFile) configure -state active
}
###########
# end public methods
###########

# handleHintText
#
# updates the hint text
#
body OverlapMenu::handleHintText { text } {
    if { $text eq "" } {
	#load default text
	set _hintText "'Create new overlaps' or 'Use existing overlaps file'\n"
    } else {
	set _hintText $text
    }
}

# validateOvFile
#
# it validates the browsed overlaps file with the database loaded
#
proc validateOvFile { filename } {
    set ovfile [open $filename "r"]
    fconfigure "$ovfile" -encoding utf-8
    while {[gets "$ovfile" line] >= 0} {
	set path_left ""
	set path_right ""

	set left [lindex $line 0]
	set right [lindex $line 1]

	catch { set path_left [ paths $left ] }
	catch { set path_right [ paths $right ] }
	if { $path_left eq "" || $path_right eq ""  } {
	    tk_messageBox -icon error -type ok -title "Bad Overlaps File" -message "Unrecognized region pair:\n$left $right\n\nLoad correct overlaps file!"
	    return -1
	}
    }
    return 0
}

# entry point
proc overlaps_tool { args } {
    set parent ""
    set filename ""
    set usage false
    set firstFlag false

    if {[llength $args] == 1} {
	if {[lindex $args 0] == "-F"} {
	    set firstFlag true
	} else {
	    set filename $args
	}
    } elseif {[llength $args] == 2} {
	if {[lindex $args 0] == "-F"} {
	    set firstFlag true
	    set filename [lindex $args 1]
	} else {
	    set usage true
	}
    } elseif {[llength $args] > 3} {
	set usage true
    }

    if {$usage} {
	return -code error {Usage: overlaps_tool [-F] [overlaps_file]}
    }

    if {[winfo exists $parent.overlapmenu]} {
	destroy $parent.overlapmenu
    }

    catch {set db_path [opendb]}
    if {$db_path eq ""} {
	return -code 1 "no database seems to be open"
    }

    set overlapmenuWindow [toplevel $parent.overlapmenu]
    set overlapmenu [OverlapMenu $overlapmenuWindow.overlapmenu]
    wm title $overlapmenuWindow "Overlap Menu"
    pack $overlapmenu

    $overlapmenu configure -firstFlag $firstFlag

    # if filename is specified directly run checker tool
    if {$filename != ""} {
	$overlapmenu configure -ovfile $filename
	$overlapmenu runCheckerTool
    }
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
