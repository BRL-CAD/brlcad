#!/bin/sh
# the next line restarts using wish \
exec wish "$0" ${1+"$@"}

## maxsize.tcl
##
## This demo uses a really big table.  The big startup time is in
## filling the table's Tcl array var.
##
## jeff at hobbs org

source [file join [file dirname [info script]] loadtable.tcl]

array set table {
    rows	40000
    cols	10
    table	.t
    array	t
}

proc fill { array x y } {
    upvar $array f
    for {set row 0} {$row<$x} {incr row} {
	for {set col 0} {$col<$y} {incr col} {
	    set f($row,$col) "$row,$col"
	}
    }
}

## Test out the use of a procedure to define tags on rows and columns
proc colProc col { if {$col > 0 && $col % 2} { return OddCol } }

label .label -text "TkTable v2 Example"

fill $table(array) $table(rows) $table(cols)
table $table(table) \
	-rows $table(rows) -cols $table(cols) \
	-variable $table(array) \
	-width 6 -height 8 \
	-titlerows 1 -titlecols 1 \
	-yscrollcommand {.sy set} \
	-xscrollcommand {.sx set} \
	-coltagcommand colProc \
	-selectmode extended \
	-rowstretch unset \
	-colstretch unset \
	-selecttitles 0 \
	-drawmode slow

scrollbar .sy -command [list $table(table) yview]
scrollbar .sx -command [list $table(table) xview] -orient horizontal
button .exit -text "Exit" -command {exit}
grid .label - -sticky ew
grid $table(table) .sy -sticky news
grid .sx -sticky ew
grid .exit -sticky ew -columnspan 2
grid columnconfig . 0 -weight 1
grid rowconfig . 1 -weight 1

$table(table) tag config OddCol -bg brown -fg pink
$table(table) tag config title -bg red -fg blue -relief sunken
$table(table) tag config dis -state disabled

set i -1
set first [$table(table) cget -colorigin]
foreach anchor {n s e w nw ne sw se c} {
    $table(table) tag config $anchor -anchor $anchor
    $table(table) tag row $anchor [incr i]
    $table(table) set $i,$first $anchor
}
font create courier -family Courier -size 10
$table(table) tag config s -font courier -justify center

$table(table) width -2 8 -1 9 0 12 4 14

puts [list Table is $table(table) with array [$table(table) cget -var]]
