#!/bin/sh
# next line is a comment in tcl \
exec wish "$0" ${1+"$@"}

## buttons.tcl
##
## demonstrates the simulation of a button array
##
## ellson@lucent.com
## modifications made by jeff at hobbs org

source [file join [file dirname [info script]] loadtable.tcl]

array set table {
    rows	20
    cols	20
    table	.table
}

# create the table
set t $table(table)
table $t -rows [expr {$table(rows)+1}] -cols [expr {$table(cols)+1}] \
	-titlerows  1 -titlecols  1 \
	-roworigin -1 -colorigin -1 \
	-colwidth 4 \
	-width 8 -height 8 \
	-variable tab \
	-flashmode off \
	-cursor top_left_arrow \
	-borderwidth 2 \
	-state disabled \
	-xscrollcommand ".sx set" -yscrollcommand ".sy set"

scrollbar .sx -orient h -command "$t xview"
scrollbar .sy -orient v -command "$t yview"

grid $t .sy -sticky nsew
grid .sx -sticky ew
grid columnconfig . 0 -weight 1
grid rowconfig . 0 -weight 1

# set up tags for the various states of the buttons
$t tag configure OFF -bg red -relief raised
$t tag configure ON  -bg green -relief sunken
$t tag configure sel -bg gray75 -relief flat

# clean up if mouse leaves the widget
bind $t <Leave> {
    %W selection clear all
}

# highlight the cell under the mouse
bind $t <Motion> {
    if {[%W selection includes @%x,%y]} break
    %W selection clear all
    %W selection set @%x,%y
    break
    ## "break" prevents the call to tkTableCheckBorder
}

# mousebutton 1 toggles the value of the cell
# use of "selection includes" would work here
bind $t <1> {
    set rc [%W cursel]
    if {[string match ON $tab($rc)]} {
	set tab($rc) OFF
        %W tag celltag OFF $rc
    } {
	set tab($rc) ON
        %W tag celltag ON $rc
    }
}

# inititialize the array, titles, and celltags
for {set i 0} {$i < $table(rows)} {incr i} {
    set tab($i,-1) $i
    for {set j 0} {$j < $table(cols)} {incr j} {
        if {! $i} {set tab(-1,$j) $j}
	set tab($i,$j) "OFF"
        $t tag celltag OFF $i,$j
    }
}
