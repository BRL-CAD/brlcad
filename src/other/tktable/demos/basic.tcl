#!/bin/sh
# the next line restarts using wish \
exec wish "$0" ${1+"$@"}

## basic.tcl
##
## This demo shows the basic use of the table widget
##
## jeff at hobbs org

source [file join [file dirname [info script]] loadtable.tcl]

array set table {
    rows	8
    cols	8
    table	.t
    array	t
}

proc fill { array x y } {
    upvar $array f
    for {set i -$x} {$i<$x} {incr i} {
	for {set j -$y} {$j<$y} {incr j} { set f($i,$j) "r$i,c$j" }
    }
}

## Test out the use of a procedure to define tags on rows and columns
proc rowProc row { if {$row>0 && $row%2} { return OddRow } }
proc colProc col { if {$col>0 && $col%2} { return OddCol } }

label .label -text "TkTable v1 Example"

fill $table(array) $table(rows) $table(cols)
table $table(table) -rows $table(rows) -cols $table(cols) \
	-variable $table(array) \
	-width 6 -height 6 \
	-titlerows 1 -titlecols 2 \
	-roworigin -1 -colorigin -2 \
	-yscrollcommand {.sy set} -xscrollcommand {.sx set} \
	-rowtagcommand rowProc -coltagcommand colProc \
	-colstretchmode last -rowstretchmode last \
	-selectmode extended -sparsearray 0

scrollbar .sy -command [list $table(table) yview]
scrollbar .sx -command [list $table(table) xview] -orient horizontal
button .exit -text "Exit" -command {exit}

grid .label - -sticky ew
grid $table(table) .sy -sticky news
grid .sx -sticky ew
grid .exit -sticky ew -columnspan 2
grid columnconfig . 0 -weight 1
grid rowconfig . 1 -weight 1

$table(table) tag config OddRow -bg orange -fg purple
$table(table) tag config OddCol -bg brown -fg pink

$table(table) width -2 7 -1 7 1 5 2 8 4 14

puts [list Table is $table(table) with array [$table(table) cget -var]]

