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
source scripts/patterns.tcl


option add *graph.xTitle "X Axis Label"
option add *graph.yTitle "Y Axis Label"
option add *graph.title "A Simple Barchart"
option add *graph.xFont *Times-Medium-R*12*
option add *graph.elemBackground white
option add *graph.elemRelief raised

set visual [winfo screenvisual .] 
if { $visual != "staticgray" && $visual != "grayscale" } {
    option add *print.background yellow
    option add *quit.background red
}

htext .header -text {
This is an example of the barchart widget.  To create a postscript 
file "bar.ps", press the %% 
button $htext(widget).print -text {Print} -command {
  $graph postscript output bar.ps  -maxpect 1
} 
$htext(widget) append $htext(widget).print
%% button.}

set graph [barchart .b]
$graph configure \
    -invert true \
    -baseline 1.2
$graph xaxis configure \
    -command FormatLabel \
    -descending true
$graph legend configure \
    -hide yes

htext .footer -text {Hit the %%
button $htext(widget).quit -text quit -command exit
$htext(widget) append $htext(widget).quit 
%% button when you've seen enough.%%
label $htext(widget).logo -bitmap BLT
$htext(widget) append $htext(widget).logo -padx 20
%%}

set names { One Two Three Four Five Six Seven Eight }
if { $visual == "staticgray" || $visual == "grayscale" } {
    set fgcolors { white white white white white white white white }
    set bgcolors { black black black black black black black black }
} else {
    set fgcolors { red green blue purple orange brown cyan navy }
    set bgcolors { green blue purple orange brown cyan navy red }
}
set bitmaps { 
    bdiagonal1 bdiagonal2 checker2 checker3 cross1 cross2 cross3 crossdiag
    dot1 dot2 dot3 dot4 fdiagonal1 fdiagonal2 hline1 hline2 lbottom ltop
    rbottom rtop vline1 vline2
}
set numColors [llength $names]

for { set i 0} { $i < $numColors } { incr i } {
    $graph element create [lindex $names $i] \
	-data { $i+1 $i+1 } \
	-fg [lindex $fgcolors $i] \
	-bg [lindex $bgcolors $i] \
	-stipple [lindex $bitmaps $i]  \
	-relief raised \
	-bd 2 
}

$graph element create Nine \
    -data { 9 -1.0 } \
    -fg red  \
    -relief sunken 
$graph element create Ten \
    -data { 10 2 } \
    -fg seagreen \
    -stipple hobbes \
    -background palegreen 
$graph element create Eleven \
    -data { 11 3.3 } \
    -fg blue  

#    -coords { -Inf Inf  } 

$graph marker create bitmap \
    -coords { 11 3.3 } -anchor center \
    -bitmap @bitmaps/sharky.xbm \
    -name bitmap \
    -fill "" 

$graph marker create polygon \
    -coords { 5 0 7 2  10 10  10 2 } \
    -name poly -linewidth 0 -fill ""

table . \
    .header 0,0 -padx .25i \
    $graph 1,0 -fill both \
    .footer 2,0 -padx .25i  

table configure . r0 r2 -resize none

wm min . 0 0

proc FormatLabel { w value } {
    # Determine the element name from the value
    set displaylist [$w element show]
    set index [expr round($value)-1]
    set name [lindex $displaylist $index]
    if { $name == "" } { 
	return $name
    }
    # Return the element label
    set info [$w element configure $name -label]
    return [lindex $info 4]
}

Blt_ZoomStack $graph
Blt_Crosshairs $graph
Blt_ActiveLegend $graph
Blt_ClosestPoint $graph


$graph marker bind all <B3-Motion> {
    set coords [%W invtransform %x %y]
    catch { %W marker configure [%W marker get current] -coords $coords }
}

$graph marker bind all <Enter> {
    set marker [%W marker get current]
    catch { %W marker configure $marker -fill green -outline black}
}

$graph marker bind all <Leave> {
    set marker [%W marker get current]
    catch { 
	set default [lindex [%W marker configure $marker -fill] 3]
	%W marker configure $marker -fill "$default"
    }
}

