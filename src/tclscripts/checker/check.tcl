#                     C H E C K E R . T C L
# BRL-CAD
#
# Copyright (c) 2016 United States Government as represented by
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
# This is the Geometry Checker GUI.  It presents a list of geometry
# objects that overlap or have other problems.
#

package require Tk
package require Itcl
package require Itk


# go ahead and blow away the class if we are reloading
catch {delete class GeometryChecker} error


::itcl::class GeometryChecker {
    inherit ::itk::Widget

    constructor { args } {}
    destructor {}

    public {
	method loadOverlaps { args } {}
	method sortBy { col direction } {}
    }

    private {
	variable _ck
	variable _status

	variable _arrowUp
	variable _arrowDown
	variable _arrowOff

	variable _colorOdd
	variable _colorEven
    }
}


###########
# begin constructor/destructor
###########

::itcl::body GeometryChecker::constructor {args} {

    # image create bitmap _arrowUp -data {
    # #define arrowUp_width 7
    # #define arrowUp_height 4
    # 	static char arrowUp_bits[] = {
    #     0x08, 0x1c, 0x3e, 0x7f
    # 	};
    # }
    # image create bitmap _arrowDown -data {
    # #define arrowDown_width 7
    # #define arrowDown_height 4
    # 	static char arrowDown_bits[] = {
    #     0x7f, 0x3e, 0x1c, 0x08
    # 	};
    # }

    image create bitmap _arrowUp -data {
    #define up_width 11
    #define up_height 11
	static char up_bits = {
        0x00, 0x00, 0x20, 0x00, 0x70, 0x00, 0xf8, 0x00, 0xfc, 0x01, 0xfe,
        0x03, 0x70, 0x00, 0x70, 0x00, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00
	}
    }
    image create bitmap _arrowDown -data {
    #define down_width 11
    #define down_height 11
	static char down_bits = {
        0x00, 0x00, 0x00, 0x00, 0x70, 0x00, 0x70, 0x00, 0x70, 0x00, 0xfe,
        0x03, 0xfc, 0x01, 0xf8, 0x00, 0x70, 0x00, 0x20, 0x00, 0x00, 0x00
	}
    }
    image create bitmap _arrowOff -data {
    #define arrowOff_width 11
    #define arrowOff_height 11
	static char arrowOff_bits[] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
    }

    itk_component add checkFrame {
    	ttk::frame $itk_interior.checkFrame -padding 2 
    } {}

    itk_component add checkLabelIntro {
    	ttk::label $itk_component(checkFrame).checkLabelIntro -text "This tool is intended to help resolve geometry overlaps and is a work-in-progress"
    } {}

    itk_component add checkLabelStatus {
    	ttk::label $itk_component(checkFrame).checkLabelStatus -text "Data Not Yet Loaded"
    } {}

    itk_component add checkList {
    	ttk::treeview $itk_component(checkFrame).checkList \
	    -columns "ID Left Right Size" \
	    -show headings \
	    -yscroll [ code $itk_component(checkFrame).checkScroll set]
    } {}

    itk_component add checkScroll {
	ttk::scrollbar $itk_component(checkFrame).checkScroll -orient vertical -command [ code $itk_component(checkFrame).checkList yview ]
    } {}

    itk_component add checkButtonFrame {
    	ttk::frame $itk_interior.checkButtonFrame -padding 0
    } {}

    itk_component add checkGrip {
    	ttk::sizegrip $itk_component(checkButtonFrame).checkGrip
    } {}

    itk_component add buttonLeft {
	ttk::button $itk_component(checkButtonFrame).buttonLeft -text "Subtract Left" -padding 10
    } {}
    itk_component add buttonRight {
	ttk::button $itk_component(checkButtonFrame).buttonRight -text "Subtract Right" -padding 10
    } {}
    itk_component add buttonBoth {
	ttk::button $itk_component(checkButtonFrame).buttonBoth -text "Subtract Both" -padding 10 -state disabled
    } {}
    itk_component add buttonNext {
	ttk::button $itk_component(checkButtonFrame).buttonNext -text "Skip to Next" -padding 10
    } {}


    eval itk_initialize $args

    set _colorOdd \#ffffff
    set _colorEven \#f0fdf0

    set _ck $itk_component(checkFrame).checkList
    set _status $itk_component(checkFrame).checkLabelStatus

    puts $_ck

    $_ck column ID -anchor center -width 100 -minwidth 50
    $_ck column Left -anchor w -width 200 -minwidth 100
    $_ck column Right -anchor w -width 200 -minwidth 100
    $_ck column Size -anchor e -width 100 -minwidth 50
    $_ck heading ID -image _arrowDown -anchor w -command [list $this sortBy ID 1]
    $_ck heading Left -text "Left" -image _arrowOff -anchor center -command [list $this sortBy Left 0]
    $_ck heading Right -text "Right" -image _arrowOff -anchor center -command [list $this sortBy Right 0]
    $_ck heading Size -text "Vol. Est." -image _arrowOff -anchor e -command [list $this sortBy Size 0]

    pack $itk_component(checkFrame) -expand true -fill both -anchor center
    pack $itk_component(checkButtonFrame).checkGrip -side right -anchor se
#    pack $itk_component(checkFrame).checkLabelIntro -fill y -padx 10 -pady 10
    pack $itk_component(checkFrame).checkLabelStatus -side top -fill x -padx 10 -pady 10

    pack $itk_component(checkButtonFrame) -side bottom -expand true -fill both
    pack $itk_component(checkButtonFrame).buttonRight -side right -pady 10
#    pack $itk_component(checkButtonFrame).buttonBoth -side right -pady 10
    pack $itk_component(checkButtonFrame).buttonLeft -side right -pady 10

    pack $itk_component(checkFrame).checkScroll -side right -fill y 
    pack $itk_component(checkFrame).checkList -expand 1 -fill both -padx {20 0}
#    pack $itk_component(checkButtonFrame).buttonNext -side bottom -fill x
#     pack $itk_component(checkFrame).buttonLeft -side bottom -fill x -padx 10 -pady 10
#     pack $itk_component(checkFrame).buttonBoth -side bottom -fill x -padx 10 -pady 10
#     pack $itk_component(checkFrame).buttonRight -side bottom -fill x -padx 10 -pady 10
#     pack $itk_component(checkFrame).buttonNext -side bottom -fill x -padx 10 -pady 10
}


::itcl::body GeometryChecker::destructor {} {
}

###########
# end constructor/destructor
###########


###########
# begin public methods
###########

body GeometryChecker::loadOverlaps {{filename ""}} {
    if {![file exists "$filename"]} {
	return
    }

    puts "Loading from $filename"
    set ovfile [open "$filename" r]
    fconfigure "$ovfile" -encoding utf-8

    set font [::ttk::style lookup [$_ck cget -style] -font]

    set count 1
    set width_right 1
    set width_left 1
    set width_size 1
    set comment_or_empty {^[[:blank:]]*#|^[[:blank:]]*$}
    while {[gets "$ovfile" line] >= 0} {
	if {[regexp "$comment_or_empty" "$line"]} {
	    continue
	}
	set left [lindex $line 0]
	set right [lindex $line 1]
	set size [lindex $line 2]
	set len_left [font measure $font $left]
	set len_right [font measure $font $right]
	set len_size [font measure $font $size]

	if { $width_left < $len_left } {
	    set width_left $len_left
	}
	if { $width_right < $len_right } {
	    set width_right $len_right
	}
	if { $width_size < $len_size } {
	    set width_size $len_size
	}

	if {$count % 2 == 0} {
	    set tag "even"
	} else {
	    set tag "odd"
	}
	$_ck insert {} end -id $count -text $count -values [list $count $left $right [format %.2f $size]] -tags "$tag"

	incr count
    }
    set width_count [font measure $font $count]
    set width_wm [lindex [wm maxsize .] 0]

    if {$width_count + $width_left + $width_right + $width_size > $width_wm} {
	set width_left [expr ($width_wm - $width_count - $width_size - 100) / 2]
	set width_right [expr ($width_wm - $width_count - $width_size - 100) / 2]
    }
    $_ck column ID -anchor center -width [expr $width_count + 20] -minwidth $width_count
    $_ck column Left -anchor w -width [expr $width_left + 20] -minwidth [expr $width_left / 10]
    $_ck column Right -anchor w -width [expr $width_right + 20] -minwidth [expr $width_right / 10]
    $_ck column Size -anchor e -width [expr $width_size + 20] -minwidth $width_size

    $_ck tag configure "odd" -background $_colorOdd
    $_ck tag configure "even" -background $_colorEven

    close $ovfile

    $_status configure -text "$count overlaps"
}


# sortBy
#
# toggles sorting of a particular table column
#
body GeometryChecker::sortBy {column direction} {
    # pull out the column being sorted on
    set data {}
    foreach row [$_ck children {}] {
	lappend data [list [$_ck set $row $column] $row]
    }

    # sort that column and reshuffle all rows to match
    set r -1
    set dir [expr {$direction ? "-decreasing" : "-increasing"}]
    foreach info [lsort -dictionary -index 0 $dir $data] {
	$_ck move [lindex $info 1] {} [incr r]
    }

    # update header code so we sort opposite direction next time
    $_ck heading $column -command [list $this sortBy $column [expr {!$direction}]]

    # update the header arrows
    set idx -1
    foreach col [$_ck cget -columns] {
	incr idx
	if {$col ==  $column} {
	    set arrow [expr {$direction ? "_arrowUp" : "_arrowDown"}]
	    $_ck heading $idx -image $arrow
	} else {
	    $_ck heading $idx -image _arrowOff
	}
    }

    # update row tags (colors)
    set on 1
    foreach id [$_ck children {}] {
	$_ck item $id -tag [expr {$on ? "odd" : "even"}]
	set on [expr !$on]
    }
}


##########
# end public methods
##########


# All GeometryChecker stuff is in the GeometryChecker namespace
proc check {{filename ""} {parent ""}} {
    if {"$filename" != "" && ![file exists "$filename"]} {
	puts "ERROR: unable to open $filename"
	return
    }

    if {[winfo exists $parent.checker]} {
	destroy $parent.checker
    }

    set checkerWindow [toplevel $parent.checker]
    set checker [GeometryChecker $checkerWindow.ck]

    if {[file exists "$filename"]} {
	$checker loadOverlaps $filename
    }

    wm title $checkerWindow "Geometry Checker"
    pack $checker -expand true -fill both
}


# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
