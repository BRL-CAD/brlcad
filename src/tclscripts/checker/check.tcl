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

	method goPrev {} {}
	method goNext {} {}

	method subLeft {} {}
	method subRight {} {}

	method registerSubtractionCallback { callback } {}
    }

    private {
	variable _ck
	variable _status

	variable _count

	variable _arrowUp
	variable _arrowDown
	variable _arrowOff

	variable _colorOdd
	variable _colorEven

	variable _subLeftCommand
	variable _subRightCommand
	variable _drawLeftCommand
	variable _drawRightCommand

	variable _subtractionCallback
    }
}


###########
# begin constructor/destructor
###########

::itcl::body GeometryChecker::constructor {args} {

    set _subLeftCommand "puts subtract the left"
    set _subRightCommand "puts subtract the right"

    set _drawLeftCommand "puts draw -C255/0/0"
    set _drawRightCommand "puts draw -C0/0/255"

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
	ttk::button $itk_component(checkButtonFrame).buttonLeft -text "Subtract Left" -padding 10 -command [ list $this subLeft ]
    } {}
    itk_component add buttonRight {
	ttk::button $itk_component(checkButtonFrame).buttonRight -text "Subtract Right" -padding 10 -command [ list $this subRight ]
    } {}
    itk_component add buttonBoth {
	ttk::button $itk_component(checkButtonFrame).buttonBoth -text "Subtract Both" -padding 10 -state disabled
    } {}
    itk_component add buttonPrev {
	ttk::button $itk_component(checkButtonFrame).buttonPrev -text "Prev" -padding 10 -command [ list $this goPrev ]
    } {}
    itk_component add buttonNext {
	ttk::button $itk_component(checkButtonFrame).buttonNext -text "Next" -padding 10 -command [ list $this goNext ]
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
    pack $itk_component(checkButtonFrame).buttonLeft -side right -padx 20 -pady 10
    pack $itk_component(checkButtonFrame).buttonPrev -side left -padx 20 -pady 10
    pack $itk_component(checkButtonFrame).buttonNext -side left -pady 10

    pack $itk_component(checkFrame).checkScroll -side right -fill y 
    pack $itk_component(checkFrame).checkList -expand 1 -fill both -padx {20 0}

    bind $itk_component(checkButtonFrame).buttonPrev <Up> [list $this goPrev]
    bind $itk_component(checkButtonFrame).buttonPrev <Down> [list $this goNext]
    bind $itk_component(checkButtonFrame).buttonNext <Up> [list $this goPrev]
    bind $itk_component(checkButtonFrame).buttonNext <Down> [list $this goNext]
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
    set _count $count
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


# goPrev
#
# select the previous node
#
body GeometryChecker::goPrev {} {
    set sset [$_ck selection]
    set slen [llength $sset]
    set alln [$_ck children {}]
    set num0 [lindex $alln 0]

    if { $slen == 0 } {
	set curr $num0
	set prev -1
    } else {
	set curr [lindex $alln [lsearch $alln [lindex $sset 0]]]
	set prev [lindex $alln [expr [lsearch $alln [lindex $sset 0]] - 1]]
    }

    if {$curr != $num0} {
	$_status configure -text "$_count overlaps, drawing #$prev"
	$_ck see $prev
	$_ck selection set $prev
    } else {
	$_status configure -text "$_count overlaps, at the beginning of the list"
	$_ck see $num0
	$_ck selection set {}
    }
}


# goNext
#
# select the next node
#
body GeometryChecker::goNext {} {
    set sset [$_ck selection]
    set slen [llength $sset]
    set alln [$_ck children {}]
    set num0 [lindex $alln 0]

    if { $slen == 0 } {
	set curr -1
	set next $num0
    } else {
	set curr [lindex $alln [lsearch $alln [lindex $sset end]]]
	set next [lindex $alln [expr [lsearch $alln [lindex $sset end]] + 1]]
    }

    set last [lindex $alln end]
    if {$curr != $last} {
	$_status configure -text "$_count overlaps, drawing #$next"
	$_ck see $next
	$_ck selection set $next
    } else {
	$_status configure -text "$_count overlaps, at the end of the list"
	$_ck see $last
    }
}


# subLeft
#
# subtract the currently selected left nodes from the right ones
#
body GeometryChecker::subLeft {} {
    set sset [$_ck selection]
    foreach item $sset {
	foreach {id_lbl id left_lbl left right_lbl right size_lbl size} [$_ck set $item] {
	    $_subtractionCallback $left $right
	}
    }
}

# subRight
#
# subtract the currently selected right nodes from the left ones
#
body GeometryChecker::subRight {} {
    set sset [$_ck selection]
    foreach item $sset {
	foreach {id_lbl id left_lbl left right_lbl right size_lbl size} [$_ck set $item] {
	    $_subtractionCallback $right $left
	}
    }
}


# registerSubtractionCallback
#
# 
#
body GeometryChecker::registerSubtractionCallback {callback} {
    set _subtractionCallback $callback
}


##########
# end public methods
##########

proc subtract {left right} {
    puts "subtracting $left from $right"
}


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

    $checker registerSubtractionCallback [code subtract]

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
