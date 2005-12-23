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


source scripts/stipples.tcl

option add *graph.x.Title "X Axis Label"
option add *graph.y.Title "Y Axis Label"
option add *graph.title "A Simple Barchart"
option add *graph.x.Font { Times 10 } 
option add *graph.Element.Relief raised

set visual [winfo screenvisual .] 
if { $visual != "staticgray" && $visual != "grayscale" } {
    option add *graph.LineMarker.color yellow
    option add *graph.Element.Background white
    option add *graph.Legend.activeForeground pink
    option add *print.background yellow
    option add *quit.background red
    option add *graph.background palegreen
    option add *graph.plotBackground lightblue
}

htext .htext -text \
{   This is an example of the barchart widget.  The barchart has 
    many components; x and y axis, legend, crosshairs, elements, etc.  
    To create a postscript file "bar.ps", press the %%
    set w $htext(widget)
    button $w.print -text {Print} -command {
	$graph postscript output bar.ps
    } 
    $w append $w.print

%% button.  
%%

    set graph [barchart .htext.graph]
    $graph configure \
	-relief raised \
	-bd 2
    $graph xaxis configure \
	-rotate 90 \
	-stepsize 0 
    $w append $graph -fill both -padx 4

%%
    Hit the %%

    button $w.quit -text quit -command exit
    $w append $w.quit 

%% button when you've seen enough.%%

    label $w.logo -bitmap BLT
    $w append $w.logo -padx 20

%% }

set names { One Two Three Four Five Six Seven Eight }
if { $visual == "staticgray" || $visual == "grayscale" } {
    set fgcolors { white white white white white white white white }
    set bgcolors { black black black black black black black black }
} else {
    set fgcolors { yellow orange red magenta purple blue cyan green }
    set bgcolors { yellow4 orange4 red4 magenta4 purple4 blue4 cyan4 green4 }
}

set numColors [llength $names]

set tcl_precision 15
vector create x
vector create y
x seq -5.0 5.0 0.2 
y expr sin(x)
set barWidth 0.19

$graph element create sin -relief raised -bd 1 -x x -y y  -barwidth $barWidth
table . .htext -fill both
	
wm min . 0 0

Blt_ZoomStack $graph
Blt_Crosshairs $graph
Blt_ActiveLegend $graph
Blt_ClosestPoint $graph
