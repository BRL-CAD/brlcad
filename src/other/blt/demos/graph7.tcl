#!../src/bltwish

set blt_library ../library
package require BLT
set blt_library ../library
set auto_path [linsert $auto_path 0 ../library]

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

image create photo bgTexture -file ./images/buckskin.gif

option add *Graph.Tile			bgTexture
option add *Label.Tile			bgTexture
option add *Frame.Tile			bgTexture
option add *Htext.Tile			bgTexture
option add *TileOffset			0
option add *HighlightThickness		0
option add *Element.ScaleSymbols	no
option add *Element.Smooth		linear
option add *activeLine.Color		yellow4
option add *activeLine.Fill		yellow
option add *activeLine.LineWidth	0
option add *Element.Pixels		3
option add *Graph.halo			7i

set visual [winfo screenvisual .] 
if { $visual != "staticgray" } {
    option add *print.background yellow
    option add *quit.background red
}

proc FormatLabel { w value } {
    return $value
}

set graph .graph

set length 250000
graph $graph -title "Scatter Plot\n$length points" 
$graph xaxis configure \
	-loose no \
	-title "X Axis Label"
$graph yaxis configure \
	-title "Y Axis Label" 
$graph legend configure \
	-activerelief sunken \
	-background ""

$graph element create line3 -symbol square -color green4 -fill green2 \
    -linewidth 0 -outlinewidth 1 -pixels 4
table . .graph 0,0  -fill both
update

vector x($length) y($length)
x expr random(x)
y expr random(y)
x sort y
$graph element configure line3 -x x -y y

wm min . 0 0

Blt_ZoomStack $graph
Blt_Crosshairs $graph
Blt_ActiveLegend $graph
Blt_ClosestPoint $graph

busy hold $graph
update
busy release $graph

