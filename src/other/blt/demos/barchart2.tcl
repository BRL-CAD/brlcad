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

proc FormatXTicks { w value } {

    # Determine the element name from the value

    set index [expr round($value)]
    if { $index != $value } {
	return $value 
    }
    incr index -1

    set name [lindex { A1 B1 A2 B2 C1 D1 C2 A3 E1 } $index]
    return $name
}

source scripts/stipples.tcl

#image create photo bgTexture -file ./images/chalk.gif
image create photo bgTexture -file ./images/rain.gif

option add *Button.padX			5

option add *tile			bgTexture

option add *Radiobutton.font		-*-courier*-medium-r-*-*-14-*-*
option add *Radiobutton.relief		flat
option add *Radiobutton.borderWidth     2
option add *Radiobutton.highlightThickness 0

option add *Htext.font			-*-times*-bold-r-*-*-14-*-*
option add *Htext.tileOffset		no
option add *header.font			-*-times*-medium-r-*-*-14-*-*

option add *Barchart.font		 -*-helvetica-bold-r-*-*-14-*-*
option add *Barchart.title		"Comparison of Simulators"
option add *Axis.tickFont		-*-helvetica-medium-r-*-*-12-*-*
option add *Axis.titleFont		-*-helvetica-bold-r-*-*-12-*-*
option add *x.Command			FormatXTicks
option add *x.Title			"Simulator"
option add *y.Title			"Time (hrs)"

option add *activeBar.Foreground	pink
option add *activeBar.stipple		dot3
option add *Element.Background		red
option add *Element.Relief		solid

option add *Grid.dashes			{ 2 4 }
option add *Grid.hide			no
option add *Grid.mapX			""

option add *Legend.Font			"-*-helvetica*-bold-r-*-*-12-*-*"
option add *Legend.activeBorderWidth	2 
option add *Legend.activeRelief		raised 
option add *Legend.anchor		ne 
option add *Legend.borderWidth		0 
option add *Legend.position		right

option add *TextMarker.Font		*Helvetica-Bold-R*14*

set visual [winfo screenvisual .] 
if { $visual != "staticgray" && $visual != "grayscale" } {
    option add *print.background	yellow
    option add *quit.background		red
    option add *quit.activeBackground	red2
}

htext .title -text {
    Data points with like x-coordinates, can have their bar segments displayed     
    in one of the following modes (using the -barmode option):
}
htext .header -text {
    %%
        radiobutton .header.stacked -text stacked -variable barMode \
            -anchor w -value "stacked" -selectcolor red -command {
            .graph configure -barmode $barMode
        } 
        .header append .header.stacked -width 1.5i -anchor w
    %%      Bars are stacked on top of each other. The overall height is the     
                                                   sum of the y-coordinates. 
    %% 
        radiobutton .header.aligned -text aligned -variable barMode \
          -anchor w -value "aligned" -selectcolor yellow -command {
            .graph configure -barmode $barMode
        }
        .header append .header.aligned -width 1.5i -fill x
    %%      Bars are drawn side-by-side at a fraction of their normal width. 
    %%
        radiobutton .header.overlap -text "overlap" -variable barMode \
            -anchor w -value "overlap" -selectcolor green -command {
            .graph configure -barmode $barMode
        } 
        .header append .header.overlap -width 1.5i -fill x
    %%      Bars overlap slightly. 
    %%
        radiobutton .header.normal -text "normal" -variable barMode \
            -anchor w -value "normal" -selectcolor blue -command {
            .graph configure -barmode $barMode
        } 
        .header append .header.normal -width 1.5i -fill x
    %%      Bars are overlayed one on top of the next. 
}

htext .footer -text {    Hit the %%
    set im [image create photo -file ./images/stopsign.gif]
    button $htext(widget).quit -image $im -command { exit }
    $htext(widget) append $htext(widget).quit -pady 2
%% button when you've seen enough. %%
    label $htext(widget).logo -bitmap BLT
    $htext(widget) append $htext(widget).logo 
%%}

barchart .graph -tile bgTexture 

vector X Y0 Y1 Y2 Y3 Y4

X set { 1 2 3 4 5 6 7 8 9 }
Y0 set { 
    0.729111111  0.002250000  0.09108333  0.006416667  0.026509167 
    0.007027778  0.1628611    0.06405278  0.08786667  
}
Y1 set {
    0.003120278	 0.004638889  0.01113889  0.048888889  0.001814722
    0.291388889  0.0503500    0.13876389  0.04513333 
}
Y2 set {
    11.534444444 3.879722222  4.54444444  4.460277778  2.334055556 
    1.262194444  1.8009444    4.12194444  3.24527778  
}
Y3 set {
    1.015750000  0.462888889  0.49394444  0.429166667  1.053694444
    0.466111111  1.4152500    2.17538889  2.55294444 
}
Y4 set {
    0.022018611  0.516333333  0.54772222  0.177638889  0.021703889 
    0.134305556  0.5189278    0.07957222  0.41155556  
}


#
# Element attributes:  
#
#    Label     yData	Foreground	Background	Stipple	    Borderwidth
set attributes { 
    "Setup"	Y1	lightyellow3	lightyellow1	fdiagonal1	1
    "Read In"	Y0	lightgoldenrod3	lightgoldenrod1	bdiagonal1	1
    "Other"	Y4	lightpink3	lightpink1	fdiagonal1	1
    "Solve"	Y3	cyan3		cyan1		bdiagonal1	1
    "Load"	Y2	lightblue3	""		fdiagonal1	1
}

foreach {label yData fg bg stipple bd} $attributes {
    .graph element create $yData -label $label -bd $bd \
	-y $yData -x X -fg "" -bg $bg -stipple $stipple
}
.header.stacked invoke

table . \
    0,0 .title -fill x \
    1,0 .header -fill x  \
    2,0 .graph -fill both \
    3,0 .footer -fill x

table configure . r0 r1 r3 -resize none

Blt_ZoomStack .graph
Blt_Crosshairs .graph
Blt_ActiveLegend .graph
Blt_ClosestPoint .graph

.graph marker bind all <B2-Motion> {
    set coords [%W invtransform %x %y]
    catch { %W marker configure [%W marker get current] -coords $coords }
}

.graph marker bind all <Enter> {
    set marker [%W marker get current]
    catch { %W marker configure $marker -bg green}
}

.graph marker bind all <Leave> {
    set marker [%W marker get current]
    catch { %W marker configure $marker -bg ""}
}

.graph element bind all <Enter> {
    .graph element closest %x %y info
    puts stderr "$info(x) $info(y)"
}
