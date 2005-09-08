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
    #namespace import -force blt::tile::*
}
source scripts/demo.tcl

option add *Element.ScaleSymbols	true
option add *Axis.loose			true
option add *Pixels			.8c
option add *Element.lineWidth		0
option add *Legend.ActiveRelief		raised
option add *Legend.padY			0
option add *Button*Font			{ Courier 14 } widgetDefault
option add *Legend*Font			{ Courier 14 bold } widgetDefault
option add *Graph.Font			{ Courier 18 bold } widgetDefault
option add *Graph.title			"Element Symbol Types"
option add *Graph.width			8i
option add *Graph.height		6i
option add *Graph.plotPadY		.25i
option add *Graph.plotPadX		.25i

set graph .graph

graph $graph

vector x -variable ""
x set { 0.0 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0 }

for { set i 0 } { $i < 11 } { incr i } {
    set vecName "y${i}"
    vector ${vecName}
    $vecName length 11
    $vecName variable y
    set y(:) [expr $i*100.0]
}

set attributes {
    none	"None"		red	red4		y0
    arrow	"Arrow"		brown	brown4		y10
    circle	"Circle"	yellow	yellow4		y2
    cross	"Cross"		cyan	cyan4		y6
    diamond	"Diamond"	green	green4		y3
    plus	"Plus"		magenta	magenta4	y9
    splus	"Splus"		Purple	purple4		y7
    scross	"Scross"	red	red4		y8
    square	"Square"	orange	orange4		y1
    triangle	"Triangle"	blue	blue4		y4
    "@bitmaps/hobbes.xbm @bitmaps/hobbes_mask.xbm"
		"Bitmap"	yellow	black		y5
}

set count 0
foreach {symbol label fill color yVec} $attributes {
    $graph element create line${count} \
	-label $label -symbol $symbol -color $color -fill $fill -x x -y $yVec 
    incr count
}
$graph element configure line0 -dashes  { 2 4 2 } -linewidth 2
button .quit -text Quit -command exit
table . \
  $graph 0,0 -fill both \
  .quit  1,0 -fill x
Blt_ZoomStack $graph
Blt_Crosshairs $graph
Blt_ActiveLegend $graph
Blt_ClosestPoint $graph
Blt_PrintKey $graph
