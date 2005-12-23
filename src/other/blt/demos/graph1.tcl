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

if { [winfo screenvisual .] != "staticgray" } {
    option add *print.background yellow
    option add *quit.background red
    set image [image create photo -file ./images/rain.gif]
    option add *Graph.Tile $image
    option add *Label.Tile $image
    option add *Frame.Tile $image
    option add *Htext.Tile $image
    option add *TileOffset 0
}

set graph [graph .g]
htext .header \
    -text {\
This is an example of the graph widget.  It displays two-variable data 
with assorted line attributes and symbols.  To create a postscript file 
"xy.ps", press the %%
    button $htext(widget).print -text print -command {
        puts stderr [time {
	   blt::busy hold .
	   update
	   .g postscript output demo1.eps 
	   update
	   blt::busy release .
	   update
        }]
    } 
    $htext(widget) append $htext(widget).print
%% button.}

source scripts/graph1.tcl

htext .footer \
    -text {Hit the %%
button $htext(widget).quit -text quit -command { exit } 
$htext(widget) append $htext(widget).quit 
%% button when you've seen enough.%%
label $htext(widget).logo -bitmap BLT
$htext(widget) append $htext(widget).logo -padx 20
%%}

proc MultiplexView { args } { 
    eval .g axis view y $args
    eval .g axis view y2 $args
}

scrollbar .xbar \
    -command { .g axis view x } \
    -orient horizontal -relief flat \
    -highlightthickness 0 -elementborderwidth 2 -bd 0
scrollbar .ybar \
    -command MultiplexView \
    -orient vertical -relief flat  -highlightthickness 0 -elementborderwidth 2
table . \
    0,0 .header -cspan 3 -fill x \
    1,0 .g  -fill both -cspan 3 -rspan 3 \
    2,3 .ybar -fill y  -padx 0 -pady 0 \
    4,1 .xbar -fill x \
    5,0 .footer -cspan 3 -fill x

table configure . c3 r0 r4 r5 -resize none

.g postscript configure \
    -center yes \
    -maxpect yes \
    -landscape no \
    -preview yes

.g axis configure x \
    -scrollcommand { .xbar set } \
    -scrollmax 10 \
    -scrollmin 2 

.g axis configure y \
    -scrollcommand { .ybar set }

.g axis configure y2 \
    -scrollmin 0.0 -scrollmax 1.0 \
    -hide no \
    -title "Y2" 

.g legend configure \
    -activerelief flat \
    -activeborderwidth 1  \
    -position top -anchor ne

.g pen configure "activeLine" \
    -showvalues y
.g element bind all <Enter> {
    %W legend activate [%W element get current]
}
.g configure -plotpady { 1i 0 } 
.g element bind all <Leave> {
    %W legend deactivate [%W element get current]
}
.g axis bind all <Enter> {
    set axis [%W axis get current]
    %W axis configure $axis -background lightblue2
}
.g axis bind all <Leave> {
    set axis [%W axis get current]
    %W axis configure $axis -background "" 
}
.g configure -leftvariable left 
trace variable left w "UpdateTable .g"
proc UpdateTable { graph p1 p2 how } {
    table configure . c0 -width [$graph extents leftmargin]
    table configure . c2 -width [$graph extents rightmargin]
    table configure . r1 -height [$graph extents topmargin]
    table configure . r3 -height [$graph extents bottommargin]
}

set image2 [image create photo -file images/blt98.gif]
.g element configure line2 -areapattern @bitmaps/sharky.xbm \

#	-areaforeground blue -areabackground ""
.g element configure line3 -areatile $image2
.g configure -title [pwd]
