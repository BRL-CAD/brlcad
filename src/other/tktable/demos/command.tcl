#!/bin/sh
# the next line restarts using wish \
	exec wish "$0" ${1+"$@"}

## command.tcl
##
## This demo shows the use of the table widget's -command options
##
## jeff at hobbs org

source [file join [file dirname [info script]] loadtable.tcl]

array set table {
    rows	10
    cols	10
    table	.table
    array	DATA
}

proc fill { array x y } {
    upvar $array f
    for {set i -$x} {$i<$x} {incr i} {
	for {set j -$y} {$j<$y} {incr j} { set f($i,$j) "$i x $j" }
    }
}

## Test out the use of a procedure to define tags on rows and columns
proc rowProc row { if {$row>0 && $row%2} { return OddRow } }
proc colProc col { if {$col>0 && $col%2} { return OddCol } }

proc tblCmd { arrayName set cell val } {
    upvar \#0 $arrayName data

    if {$set} {
	#echo set $cell $val
	set data($cell) $val
    } else {
	#echo get $cell
	if {[info exists data($cell)]} {
	    return $data($cell)
	} else {
	    return
	}
    }
}

label .label -text "TkTable -command Example"
label .current -textvar CURRENT -width 5
entry .active -textvar ACTIVE

bind .active <Return> "$table(table) curvalue \[%W get\]"

fill $table(array) $table(rows) $table(cols)
set t $table(table)
table $table(table) -rows $table(rows) -cols $table(cols) \
	-command [list tblCmd $table(array) %i %C %s] -cache 1 \
	-width 6 -height 6 \
	-titlerows 1 -titlecols 1 \
	-yscrollcommand {.sy set} -xscrollcommand {.sx set} \
	-roworigin -1 -colorigin -1 \
	-rowtagcommand rowProc -coltagcommand colProc \
	-selectmode extended \
	-rowstretch unset -colstretch unset \
	-flashmode on -browsecommand {
    set CURRENT %S
    set ACTIVE [%W get %S]
} -validate 1 -validatecommand {
    set ACTIVE %S
    return 1
}

scrollbar .sy -command [list $table(table) yview] -orient v
scrollbar .sx -command [list $table(table) xview] -orient h
grid .label - - -sticky ew
grid .current .active - -sticky ew
grid $table(table) - .sy -sticky nsew
grid .sx - -sticky ew
grid columnconfig . 1 -weight 1
grid rowconfig . 2 -weight 1

$table(table) tag config OddRow -bg orange -fg purple
$table(table) tag config OddCol -bg brown -fg pink

puts [list Table is $table(table)]

