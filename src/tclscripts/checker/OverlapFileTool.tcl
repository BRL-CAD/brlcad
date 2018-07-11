#             O V E R L A P F I L E T O O L . T C L
# BRL-CAD
#
# Copyright (c) 2018 United States Government as represented by
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
# This is the Overlaps File tool. Which creates a new overlaps file
# for the checker tool.
# 

package require Tk
package require Itcl
package require Itk

# go ahead and blow away the class if we are reloading
# catch {delete class OverlapFileTool} error

package provide OverlapFileTool 1.0

::itcl::class OverlapFileTool {
    inherit ::itk::Widget

    constructor { args } {}
    public {
	variable pairsList
	variable overlapsList
	variable firstFlag
	variable runCheckCallback

	method runTools {} {}
    }
    private {
	variable _objs
	variable _statusText
	variable _progressValue

	method runCheckOverlaps { obj } {}
	method runGqa { obj } {}

	method sortPairs {} {}
	method rmDupPairs {} {}

	method addToList { new_list } {}
    }
}

::itcl::body OverlapFileTool::constructor { args } {
    set firstFlag false
    set _objs [who]
    set _statusText "Ready"
    set _progressValue 0

    itk_component add ovFrame {
	ttk::frame $itk_interior.ovFrame -padding 4
    } {}
    itk_component add objectsLabel {
	ttk::label $itk_component(ovFrame).objectsLabel -text "Object(s)"
    } {}
    itk_component add objectsEntry {
	tk::entry $itk_component(ovFrame).objectsEntry -width 40 -textvariable [scope _objs]
    } {}

    itk_component add progressFrame {
	ttk::frame $itk_interior.progressFrame -padding 4
    } {}
    itk_component add statusLabel {
	ttk::label $itk_component(progressFrame).statusLabel \
	    -textvariable [scope _statusText]
    } {}
    itk_component add progressBar {
	ttk::progressbar $itk_component(progressFrame).progressBar -variable [scope _progressValue]
    } {}

    itk_component add ovButtonFrame {
    	ttk::frame $itk_interior.ovButtonFrame -padding 4
    } {}
    itk_component add buttonGo {
	ttk::button $itk_component(ovButtonFrame).buttonGo \
	-text "Check For Overlaps" -padding 5 -command [code $this runTools ]
    } {}

    eval itk_initialize $args

    pack $itk_component(ovFrame) -side top -fill x -padx 8
    pack $itk_component(objectsLabel) -side top -expand true -anchor nw
    pack $itk_component(objectsEntry) -side left -expand true -fill x 
    focus $itk_component(objectsEntry)
    bind $itk_component(objectsEntry) <Return> [code $itk_component(buttonGo) invoke]

    pack $itk_component(ovButtonFrame) -side top -fill both
    pack $itk_component(buttonGo) -side left -expand true

    pack $itk_component(progressFrame) -side top -expand true -fill both
    pack $itk_component(statusLabel)
    pack $itk_component(progressBar) -side left -expand true -fill x -anchor nw -pady 8 -padx 8

}

# addToList
#
# averages the size values for common pairs and
# inserts to overlaps list
#
body OverlapFileTool::addToList { new_list } {
    set avg 0
    foreach pair $new_list {
	set size [lindex $pair 2]
	set avg [expr $avg + [expr $size/16]]
    }
    set common_pair [lindex $new_list 0]
    set common_pair [lreplace $common_pair 2 2 [format "%.4f" $avg]]
    lappend overlapsList $common_pair
}

# rmDupPairs
#
# removes any duplicate pairs by calling addToList
# by aggregating a list of pairs with same names
#
body OverlapFileTool::rmDupPairs { } {
    set _statusText "Removing duplicates from overlaps list"
    update
    set new_list {}

    #take first pair
    set prev_pair [lindex $pairsList 0]
    lappend new_list $prev_pair

    foreach pair [lrange $pairsList 1 end] {
	set prev_reg "[lindex $prev_pair 0] [lindex $prev_pair 1]"
	set reg "[lindex $pair 0] [lindex $pair 1]"

	# check if pairs have same left and right region names
	if {[string compare $reg $prev_reg] == 0} {
	    lappend new_list $pair
	} else {
	    $this addToList $new_list
	    set new_list {}
	    lappend new_list $pair
	    set prev_pair $pair
	}
    }
    set _progressValue 100
    set _statusText "Done"
    update

    #take last pairlist
    $this addToList $new_list
    puts "Unique Overlaps: [llength $overlapsList]\n"
}

# sortPairs
#
# sorts the pairs before removing duplicates
#
body OverlapFileTool::sortPairs { } {
    set _progressValue 95
    set _statusText "Sorting overlaps list"
    update
    set pairsList [lsort $pairsList]
}

# runGqa
#
# runs the gqa command for the passed object
#
body OverlapFileTool::runGqa { obj } {
    set cmd "gqa -Ao -q -g1mm,1mm $obj"
    set _statusText "Running $cmd"
    update
    catch {set gqa_list [eval $cmd]}
    set lines [split $gqa_list \n]
    foreach line $lines {
	regexp {(.*) (.*) count:([0-9]*) dist:(.*)mm} $line full left right count depth
	if { [info exists full] == 0 } {
	    continue
	}
	set size [expr $count * $depth]
	# swaps the region names by comparing lexicographically 
	if { [string compare $left $right] > 0 } {
	    lappend pairsList [list $right $left $size]
	} else {
	    lappend pairsList [list $left $right $size]
	}
	# unset $full for next line
	unset full
    }
    set _progressValue 90
    update
}

# runCheckOverlaps
#
# runs the check_overlaps command for the passed object
# 16 times for different combinations of az/el values
#
body OverlapFileTool::runCheckOverlaps { obj } {
    for { set az 0}  {$az < 180} {incr az 45} {
	for { set el 0}  {$el < 180} {incr el 45} {
	    set cmd "check_overlaps -s1024 -a$az -e$el $obj"
	    set _statusText "Running $cmd"
	    incr _progressValue 4
	    update
	    catch {set chk_list [eval $cmd]}
	    set lines [split $chk_list \n]
	    foreach line $lines {
		regexp {<(.*),.(.*)>: ([0-9]*).* (.*)mm} $line full left right count depth
		if { [info exists full] == 0 } {
		    continue
		}
		set size [expr $count * $depth]
		# swaps the region names by comparing lexicographically 
		if { [string compare $left $right] > 0 } {
		    lappend pairsList [list $right $left $size]
		} else {
		    lappend pairsList [list $left $right $size]
		}
		# unset $full for next line
		unset full
	    }
	}
    }
}

# runTools
#
# main driver that calls the commands and
# creates the overlaps file
#
body OverlapFileTool::runTools { } {
    # check if user passed the objects list
    if { [llength $_objs] == 0 } {
	tk_messageBox -icon info -type ok -title "No Objects Specified" -message "Please input objects names in the objects field"
	return
    }
    # check if the specified objects exist in the database
    set bad_objs ""
    foreach obj $_objs {
	catch {set ret [t $obj]}
	if { [string first $obj $ret] != 0 } {
	    lappend bad_objs $obj
	}
    }

    if { [llength $bad_objs] > 0 } {
	tk_messageBox -icon error -type ok -title "Bad Object Names" -message "Unrecognized object names:\n$bad_objs"
	return
    }

    # disable the go button and entry before proceeding to prevent any re-runs
    $itk_component(buttonGo) configure -state disabled
    $itk_component(objectsEntry) configure -state disabled
    $this configure -cursor watch
    update

    # run checkoverlaps and gqa for all the specified objects
    if { [string length $_objs] > 0 } {
	foreach obj $_objs {
	    set _progressValue 0
	    update
	    $this runCheckOverlaps $obj
	    $this runGqa $obj
	}
    }
    # check for the count of overlaps detected
    set ov_count [llength $pairsList]
    if { $ov_count == 0 } {
	tk_messageBox -type ok -title "No Overlaps Found" -message "No Overlaps Found"
	$itk_component(buttonGo) configure -state normal
	$itk_component(objectsEntry) configure -state normal
	$this configure -cursor ""
	set _progressValue 0
	update
	return
    }

    puts "\nCount of overlaps: $ov_count\n"

    # process the overlap pairs
    $this sortPairs
    $this rmDupPairs

    # delete any previous overlaps files in the db directory
    set db_path [eval opendb]
    set dir [file dirname $db_path]
    set name [file tail $db_path]
    set ol_dir [file join $dir "${name}.ck"]
    set filename [file join $dir "${name}.ck" "ck.${name}.overlaps"]
    file delete -force -- $ol_dir

    # create new folder
    file mkdir $ol_dir
    # write the overlaps file
    set fp [open $filename w+]
    foreach pair [lsort -decreasing -real -index 2 $overlapsList] {
	puts $pair
	puts $fp $pair
    }
    close $fp
    puts "\nOverlaps file saved: $filename"

    # run checker tool
    eval $runCheckCallback
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
