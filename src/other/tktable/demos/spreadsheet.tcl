#!/bin/sh
# the next line restarts using wish \
	exec wish "$0" ${1+"$@"}

## spreadsheet.tcl
##
## This demos shows how you can simulate a 3D table
## and has other basic features to begin a basic spreadsheet
##
## jeff at hobbs org

source [file join [file dirname [info script]] loadtable.tcl]

array set table {
    rows	10
    cols	10
    page	AA
    table	.table
    default	pink
    AA		orange
    BB		blue
    CC		green
}

proc colorize num { if {$num>0 && $num%2} { return colored } }

proc fill {array {r 10} {c 10}} {
    upvar \#0 $array ary
    for {set i 0} {$i < $r} {incr i} {
	for {set j 0} {$j < $c} {incr j} {
	    if {$j && $i} {
		set ary($i,$j) "$array $i,$j"
	    } elseif {$i} {
		set ary($i,$j) "$i"
	    } elseif {$j} {
		set ary($i,$j) [format %c [expr 64+$j]]
	    }
	}
    }
}

proc changepage {w e name el op} {
    global $name table
    if {[string comp {} $el]} { set name [list $name\($el\)] }
    set i [set $name]
    if {[string comp $i [$w cget -var]]} {
	$w sel clear all
	$w config -variable $i
	$e config -textvar ${i}(active)
	$w activate origin
	if {[info exists table($i)]} {
	    $w tag config colored -bg $table($i)
	} else {
	    $w tag config colored -bg $table(default)
	}
	$w see active
    }
}

label .example -text "TkTable v1 Spreadsheet Example"

label .current -textvar table(current) -width 5
entry .active -textvar $table(page)(active)
label .lpage -text "PAGE:" -width 6 -anchor e
tk_optionMenu .page table(page) AA BB CC DD

fill $table(page)
fill BB [expr {$table(rows)/2}] [expr {$table(cols)/2}]

trace var table(page) w [list changepage $table(table) .active]

set t $table(table)
table $t \
	-rows $table(rows) \
	-cols $table(cols) \
	-variable $table(page) \
	-titlerows 1 \
	-titlecols 1 \
	-yscrollcommand { .sy set } \
	-xscrollcommand { .sx set } \
	-coltagcommand colorize \
	-flashmode on \
	-selectmode extended \
	-colstretch unset \
	-rowstretch unset \
	-width 5 -height 5 \
	-browsecommand {set table(current) %S}

$t tag config colored -bg $table($table(page))
$t tag config title -fg red -relief groove
$t tag config blue -bg blue
$t tag config green -bg green
$t tag cell green 6,3 5,7 4,9
$t tag cell blue 8,8
$t tag row blue 7
$t tag col blue 6 8
$t width 0 3 2 7

scrollbar .sy -command [list $t yview]
scrollbar .sx -command [list $t xview] -orient horizontal
button .exit -text "Exit" -command exit

grid .example -       -      -     -   -sticky ew
grid .current .active .lpage .page -   -sticky ew
grid $t       -       -      -     .sy -sticky ns
grid .sx      -       -      -         -sticky ew
grid .exit - - - - -sticky ew
grid columnconfig . 1 -weight 1
grid rowconfig . 2 -weight 1
grid config $t -sticky news

bind .active <Return> [list tkTableMoveCell $t 1 0]

menu .menu
menu .menu.file
. config -menu .menu
.menu add cascade -label "File" -underline 0 -menu .menu.file
.menu.file add command -label "Fill Array" -command { fill $table(page) }
.menu.file add command -label "Quit" -command exit

puts [list Table is $table(table) with array [$table(table) cget -var]]

