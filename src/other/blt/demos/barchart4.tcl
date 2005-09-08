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

proc random {{max 1.0} {min 0.0}} {
    global randomSeed

    set randomSeed [expr (7141*$randomSeed+54773) % 259200]
    set num  [expr $randomSeed/259200.0*($max-$min)+$min]
    return $num
}
set randomSeed 14823


set graph .graph

source scripts/stipples.tcl
source scripts/patterns.tcl

option add *Barchart.title		"A Simple Barchart"
option add *Barchart.relief 		raised
option add *Barchart.borderWidth 	2
option add *Barchart.plotBackground 	white
option add *Barchart.baseline   	57.299

option add *Element.borderWidth		2
option add *Element.Background		white
option add *Element.Relief		raised

option add *x.Title			"X Axis"
option add *x.Font			*Times-Medium-R*10*
option add *y.Title			"Y Axis"
option add *LineMarker.Foreground	yellow

set visual [winfo screenvisual .] 
if { $visual != "staticgray" && $visual != "grayscale" } {
    option add *print.background yellow
    option add *quit.background red
    option add *graph.background palegreen
}

htext .header -text \
{   This is an example of the barchart widget.  The barchart has 
    many components; x and y axis, legend, crosshairs, elements, etc.  
    To create a postscript file "bar.ps", press the %%
    set w $htext(widget)
    button $w.print -text {Print} -command {
	$graph postscript output bar.ps 
    } 
    $w append $w.print

%% button.  
}
barchart $graph 
$graph xaxis configure -rotate 90 -stepsize 0

htext .footer -text {    Hit the %%
    set im [image create photo -file ./images/stopsign.gif]
    button $htext(widget).quit -image $im -command { exit }
    $htext(widget) append $htext(widget).quit -pady 2
%% button when you've seen enough. %%
    label $htext(widget).logo -bitmap BLT
    $htext(widget) append $htext(widget).logo 
%%}

set attributes { 
    red		bdiagonal1
    orange	bdiagonal2
    yellow	fdiagonal1
    green	fdiagonal2
    blue	hline1 
    cyan	hline2
    magenta	vline1 
    violetred	vline2
    purple	crossdiag
    lightblue 	hobbes	
}

set count 0
foreach { color stipple } $attributes {
    $graph pen create pen$count -fg ${color}1 -bg ${color}4 -stipple $stipple
    lappend styles [list pen$count $count $count]
    incr count
}

vector x y w

x seq 0 1000
y expr random(x)*90.0
w expr round(y/10.0)%$count
y expr y+10.0

$graph element create data -label {} \
    -x x -y y -weight w -styles $styles

table . \
    0,0 .header -fill x  \
    1,0 .graph -fill both \
    2,0 .footer -fill x

table configure . r0 r2 -resize none
	
wm min . 0 0

Blt_ZoomStack $graph
Blt_Crosshairs $graph
Blt_ActiveLegend $graph
Blt_ClosestPoint $graph

