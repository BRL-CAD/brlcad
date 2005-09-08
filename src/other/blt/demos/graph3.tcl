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

set visual [winfo screenvisual .]
if { $visual != "staticgray" && $visual != "grayscale" } {
    option add *Button.Background		red
    option add *TextMarker.Foreground	black
    option add *TextMarker.Background	yellow
    option add *LineMarker.Foreground	black
    option add *LineMarker.Background	yellow
    option add *PolyMarker.Fill		yellow2
    option add *PolyMarker.Outline	""
    option add *PolyMarker.Stipple	fdiagonal1
    option add *activeLine.Color	red4
    option add *activeLine.Fill		red2
    option add *Element.Color		purple
}

image create photo bgTexture \
    -file ./images/chalk.gif

option add *Tile		bgTexture
option add *Button.Tile		""
option add *Text.font			-*-times*-bold-r-*-*-18-*-*
option add *header.font			-*-times*-medium-r-*-*-18-*-*
option add *footer.font			-*-times*-medium-r-*-*-18-*-*
option add *HighlightThickness		0

set graph [graph .g]
source scripts/graph3.tcl



text .header \
    -wrap word \
    -width 0 \
    -height 3

set text {
This is an example of a bitmap marker.  Try zooming in on 
a region by clicking the left button, moving the pointer, 
and clicking again.  Notice that the bitmap scales too. 
To restore the last view, click on the right button.  
}
regsub -all "\n" $text "" text
.header insert end "$text\n"
.header configure -state disabled

htext .footer -text {Hit the %%
    set im [image create photo -file ./images/stopsign.gif]
    button $htext(widget).quit -image $im -command { exit }
    $htext(widget) append $htext(widget).quit 
%% button when you've seen enough. %%
    label $htext(widget).logo -bitmap BLT
    $htext(widget) append $htext(widget).logo 
%%}

table . \
    .header 0,0 -fill x -padx 4 -pady 4\
    $graph 1,0 -fill both  \
    .footer 2,0 -fill x -padx 4 -pady 4

table configure . r0 r2 -resize none

source scripts/ps.tcl

bind $graph <Shift-ButtonPress-1> { 
    MakePsLayout $graph
}

if 0 {
set printer [printer open [lindex [printer names] 0]]
after 2000 {
	$graph print2 $printer
}
}
#after 2000 {
#    PsDialog $graph
#}
