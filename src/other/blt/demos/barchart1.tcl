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

set graph .bc

proc random {{max 1.0} {min 0.0}} {
    global randomSeed

    set randomSeed [expr (7141*$randomSeed+54773) % 259200]
    set num  [expr $randomSeed/259200.0*($max-$min)+$min]
    return $num
}
set randomSeed 148230

proc FormatLabel { w value } {

    # Determine the element name from the value

    set names [$w element show]
    set index [expr round($value)]
    if { $index != $value } {
	return $value 
    }
    global elemLabels
    if { [info exists elemLabels($index)] } {
	return $elemLabels($index)
    }
    return $value
}

source scripts/stipples.tcl

image create photo bgTexture -file ./images/rain.gif

option add *tile			bgTexture

option add *Button.tile			""

option add *Htext.tileOffset		no
option add *Htext.font			{ Times 12 }

option add *Barchart.title		"A Simple Barchart"

option add *Axis.tickFont		{ Courier 10 }
option add *Axis.titleFont		{ Helvetica 12 bold }

option add *x.Title			"X Axis Label"
option add *x.Rotate			90
option add *x.Command			FormatLabel
option add *y.Title			"Y Axis Label"

option add *Element.Background		white
option add *Element.Relief		solid
option add *Element.BorderWidth		1

option add *Legend.hide			yes

option add *Grid.hide			no
option add *Grid.dashes			{ 2 4 }
option add *Grid.mapX			""

set visual [winfo screenvisual .] 
if { $visual != "staticgray" && $visual != "grayscale" } {
    option add *print.background yellow
    option add *quit.background red
    option add *graph.background palegreen
}

htext .header -text {
    The barchart has several components: coordinate axes, data 
    elements, legend, crosshairs, grid,  postscript, and markers.  
    They each control various aspects of the barchart.  For example,
    the postscript component lets you generate PostScript output.  
    Pressing the %%

    set w $htext(widget)
    button $w.print -text {Print} -command {
	.bc postscript output bar.ps
    } 
    $w append $w.print

%% button will create a file "bar.ps" 
}

htext .footer -text {
    Hit the %%

    set w $htext(widget)
    button $w.quit -text quit -command exit 
    $w append $w.quit 

%% button when you've seen enough.%%

    label $w.logo -bitmap BLT
    $w append $w.logo -padx 20

%% }

barchart .bc

#
# Element attributes:  
#
#    Label	Foreground	Background	Stipple Pattern

source scripts/stipples.tcl

set bitmaps { 
    bdiagonal1 bdiagonal2 checker2 checker3 cross1 cross2 cross3 crossdiag
    dot1 dot2 dot3 dot4 fdiagonal1 fdiagonal2 hline1 hline2 lbottom ltop
    rbottom rtop vline1 vline2
}
set count 1
foreach stipple $bitmaps {
    set label [file tail $stipple]
    set label [file root $label] 
    set y [random -2 10]
    set yhigh [expr $y + 0.5]
    set ylow [expr $y - 0.5]
    .bc element create $label -y $y -x $count \
	-fg brown -bg orange -stipple $stipple -yhigh $yhigh -ylow $ylow 
    set elemLabels($count) $label
    incr count
}

table . \
    0,0 .header -fill x \
    1,0 .bc -fill both \
    2,0	.footer -fill x	
	
table configure . r0 r2 -resize none

Blt_ZoomStack .bc
Blt_Crosshairs .bc
Blt_ActiveLegend .bc
Blt_ClosestPoint .bc

if 0 {
set printer [printer open [lindex [printer names] 0]]
printer getattr $printer attrs
set attrs(Orientation) Portrait
printer setattr $printer attrs
after 2000 {
	$graph print2 $printer
	printer close $printer
}
}

.bc axis bind x <Enter> {
    set axis [%W axis get current]
    %W axis configure $axis -color blue3 -titlecolor blue3
}
.bc axis bind x <Leave> {
    set axis [%W axis get current]
    %W axis configure $axis -color black -titlecolor black
}


