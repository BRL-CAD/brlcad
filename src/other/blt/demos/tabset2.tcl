#!../src/bltwish

package require BLT

# --------------------------------------------------------------------------
# Starting with Tcl 8.x, the BLT commands are stored in their own 
# namespace called "blt".  The idea is to prevent name clashes with
# Tcl commands and variables from other packages, such as a "table"
# command in two different packages.  
#
# You can access the BLT commands in a couple of ways.  You can prefix
# all the BLT commands with the namespace qualifier "blt::"
#  
#    blt::graph .g
#    blt::table . .g -resize both
# 
# or you can import all the command into the global namespace.
#
#    namespace import blt::*
#    graph .g
#    table . .g -resize both
#
# --------------------------------------------------------------------------

if { $tcl_version >= 8.0 } {
    namespace import blt::*
    namespace import -force blt::tile::*
}
source scripts/demo.tcl

image create photo bgTile -file ./images/smblue_rock.gif
image create photo label1 -file ./images/mini-book1.gif
image create photo label2 -file ./images/mini-book2.gif
image create photo testImage -file ./images/txtrflag.gif

scrollbar .s -command { .t view } -orient horizontal
tabset .t \
    -relief flat \
    -bd 2 \
    -textside right \
    -samewidth yes \
    -tiers 2 \
    -slant right \
    -scrollcommand { .s set } \
    -tile bgTile \
    -scrollincrement 1  -selectforeground green2

label .t.l -image testImage

set attributes {
    graph1 "Graph \#1" red	.t.graph1  
    graph2 "Graph \#2" green	.t.graph2  
    graph3 "Graph \#3" cyan	.t.graph3  
    graph5 "Graph \#5" yellow	.t.graph5  
    graph6 one		orange	.t.l       
}

foreach { entry label color window } $attributes {
    .t insert end $entry -text $label -fill both
}

.t insert end \
    there bunky another test of \
    a -image label2 widget

table . \
    .t 0,0 -fill both \
    .s 1,0 -fill x 

table configure . r1 -resize none

foreach file { graph1 graph2 graph3 graph5 } {
    namespace eval $file {
	set graph [graph .t.$file]
	source scripts/$file.tcl
	.t tab configure $file -window $graph
    }
}

