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

if { ![string match "*gray*" [winfo screenvisual .]] } {
    option add *Button.Background	red
    option add *TextMarker.Foreground	black
    option add *TextMarker.Background	yellow
    option add *LineMarker.Foreground	black
    option add *LineMarker.Background	yellow
    option add *PolyMarker.Fill		yellow2
    option add *PolyMarker.Outline	""
    option add *PolyMarker.Stipple	bdiagonal1
    option add *activeLine.Color	red4
    option add *activeLine.Fill		red2
    option add *Element.Color		purple
}

set data {
    R0lGODlhEAANAMIAAAAAAH9/f///////AL+/vwAA/wAAAAAAACH5BAEAAAUALAAAAAAQAA0A
    AAM8WBrM+rAEQWmIb5KxiWjNInCkV32AJHRlGQBgDA7vdN4vUa8tC78qlrCWmvRKsJTquHkp
    ZTKAsiCtWq0JADs=
}
set data {
    R0lGODlhEAANAMIAAAAAAH9/f///////AL+/vwAA/wAAAAAAACH5BAEAAAUALAAAAAAQAA0A
    AAM1WBrM+rAEMigJ8c3Kb3OSII6kGABhp1JnaK1VGwjwKwtvHqNzzd263M3H4n2OH1QBwGw6
    nQkAOw==
}
set image [image create photo -format gif -data $data]

set graph [graph .g]
table . \
    0,0 $graph -fill both 

source scripts/graph2.tcl

$graph postscript configure \
    -maxpect yes \
    -landscape yes 
$graph configure \
    -width 5i \
    -height 5i 
$graph axis configure x \
    -title "X Axis"

if 1 {
    $graph element configure line1 \
	-areapattern solid -areaforeground green
#	-areatile $image 
    $graph element configure line3 \
	-areapattern @bitmaps/sharky.xbm \
	-areaforeground red \
	-areabackground "" -areapattern solid
}

set fileName testImg.jpg
if { [file exists $fileName] } {
    set image [image create photo]
    winop readjpeg $fileName $image
    if 1 { 
	puts stderr [time { 
	    $graph marker create image -image $image \
		-coords "-360.0 -1.0 360.0 1.0" \
		-under yes \
		-mapx degrees \
		-name $fileName 
	}]
    }
} 


bind $graph <Control-ButtonPress-3> { MakeSnapshot }
bind $graph <Shift-ButtonPress-3> { 
    %W postscript output demo2.ps 
    %W snap -format emf demo2.emf
}

set unique 0
proc MakeSnapshot {} {
    update idletasks
    global unique
    set top ".snapshot[incr unique]"
    set im [image create photo]
    $graph snap $im 210 150

    toplevel $top
    wm title $top "Snapshot \#$unique of \"[$graph cget -title]\""
    label $top.lab -image $im 
    button $top.but -text "Dismiss" -command "DestroySnapshot $top"
    table $top $top.lab
    table $top $top.but -pady 4 
    focus $top.but
}

proc DestroySnapshot { win } {
    set im [$win.lab cget -image]
    $im write test.ppm
    image delete $im
    destroy $win
    exit
}

if { $tcl_platform(platform) == "windows" } {
    if 0 {
        set name [lindex [blt::printer names] 0]
        set printer {Lexmark Optra E310}
	blt::printer open $printer
	blt::printer getattrs $printer attrs
	puts $attrs(Orientation)
	set attrs(Orientation) Landscape
	set attrs(DocumentName) "This is my print job"
	blt::printer setattrs $printer attrs
	blt::printer getattrs $printer attrs
	puts $attrs(Orientation)
	after 5000 {
	    $graph print2 $printer
	    blt::printer close $printer
	}
    } else {
	after 5000 {
	    $graph print2 
	}
    }	
    if 1 {
	after 2000 {$graph snap -format emf CLIPBOARD}
    }
}
