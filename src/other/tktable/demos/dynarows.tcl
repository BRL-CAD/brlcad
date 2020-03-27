#!/bin/sh
# the next line restarts using wish \
exec wish "$0" ${1+"$@"}

## dynarows.tcl
##
## This demos shows the use of the validation mechanism of the table
## and uses the table's cache (no -command or -variable) with a cute
## dynamic row routine.
##
## jeff at hobbs org

source [file join [file dirname [info script]] loadtable.tcl]

proc table_validate {w idx} {
    if {[scan $idx %d,%d row col] != 2} return
    set val [$w get $idx]
 
    ## Entries in the last row are allowed to be empty
    set nrows [$w cget -rows]
    if {$row == ${nrows}-1 && [string match {} $val]} { return }
 
    if {[catch {clock scan $val} time]} {
        bell
        $w activate $idx
        $w selection clear all
        $w selection set active
        $w see active
    } else {
        set date {}
        foreach item [clock format $time -format "%m %d %Y"] {
            lappend date [string trimleft $item "0"]
        }
        $w set $idx [join $date "/"]
        if {$row == ${nrows}-1} {
	    ## if this is the last row and both cols 1 && 2 are not empty
	    ## then add a row and redo configs
            if {[string comp [$w get $row,1] {}] && \
                    [string comp [$w get $row,2] {}]} {
		$w tag row {} $row
		$w set $row,0 $row
                $w configure -rows [incr nrows]
		$w tag row unset [incr row]
		$w set $row,0 "*"
                $w see $row,1
		$w activate $row,1
            }
        }
    }
}

label .example -text "Dynamic Date Validated Rows"

set t .table
table $t -rows 2 -cols 3 -cache 1 -selecttype row \
	-titlerows 1 -titlecols 1 \
	-yscrollcommand { .sy set } \
	-xscrollcommand { .sx set } \
	-height 5 -colstretch unset -rowstretch unset \
	-autoclear 1 -browsecommand {table_validate %W %s}

$t set 0,1 "Begin" 0,2 "End" 1,0 "*"
$t tag config unset -fg \#008811
$t tag config title -fg red
$t tag row unset 1
$t width 0 3

scrollbar .sy -command [list $t yview]
scrollbar .sx -command [list $t xview] -orient horizontal
grid .example -     -sticky ew
grid $t       .sy   -sticky news
grid .sx            -sticky ew
grid columnconfig . 0 -weight 1
grid rowconfig . 1 -weight 1

bind $t <Return> {
    set r [%W index active row]
    set c [%W index active col]
    if {$c == 2} {
	%W activate [incr r],1
    } else {
	%W activate $r,[incr c]
    }
    %W see active
    break
}
bind $t <KP_Enter> [bind $t <Return>]
