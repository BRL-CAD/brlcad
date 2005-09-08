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
bltdebug watch ResizeEpsItem

proc MoveEpsItem { canvas tagName x y } {
    global lastX lastY
    $canvas move $tagName [expr $x - $lastX] [expr $y - $lastY]
    set lastX $x; set lastY $y
}

proc GetEpsBBox { canvas tagName } {
    global left top right bottom
    set anchor [$canvas coords $tagName-image]
    set left [lindex $anchor 0]
    set top [lindex $anchor 1]
    set width [$canvas itemcget $tagName-image -width]
    set height [$canvas itemcget $tagName-image -height]
    set right [expr $left + $width]
    set bottom [expr $top + $height]
}
    
proc SaveImageCoords { canvas x y } {
    global lastX lastY 
    set lastX $x
    set lastY $y
    $canvas configure -cursor sb_h_double_arrow
}

array set cursors {
    sw bottom_left_corner
    ne top_right_corner
    se bottom_right_corner
    nw top_left_corner
}

proc StartResize { canvas tagName x y anchor } {
    global left top right bottom image

    GetEpsBBox $canvas $tagName
    $canvas itemconfigure $tagName-image -quick yes 
    $canvas itemconfigure $tagName-grip -fill red
    $canvas create line $left $top $right $bottom  \
	-tags "$tagName $tagName-cross $tagName-l1" \
	-fill red -width 2

    $canvas create line $left $bottom $right $top \
	-tags "$tagName $tagName-cross $tagName-l2" \
	-fill red  -width 2
    $canvas raise $tagName-grip
    global cursors
    $canvas configure -cursor $cursors($anchor)
    global lastX lastY 
    set lastX $x
    set lastY $y
}

proc EndResize { canvas tagName x y anchor } {
    $canvas itemconfigure $tagName-image -quick no \
        -showimage yes
    ResizeEpsItem $canvas $anchor $tagName $x $y
    $canvas itemconfigure $tagName-grip -fill green
    $canvas delete $tagName-cross
    $canvas configure -cursor ""
}

proc ResetGrips { canvas tagName } {
    global gripSize
    global left top right bottom

    GetEpsBBox $canvas $tagName
    $canvas coords $tagName-nw \
	$left $top [expr $left + $gripSize] [expr $top + $gripSize] 
    $canvas coords $tagName-se \
	[expr $right - $gripSize] [expr $bottom - $gripSize] $right $bottom 
    $canvas coords $tagName-ne \
	[expr $right - $gripSize] [expr $top + $gripSize] $right $top 
    $canvas coords $tagName-sw \
	$left $bottom [expr $left + $gripSize] [expr $bottom - $gripSize] 
    $canvas coords $tagName-l1 $left $top $right $bottom  
    $canvas coords $tagName-l2 $left $bottom $right $top 
}

proc ResizeEpsItem { canvas anchor tagName x y } {
    global lastX lastY left top right bottom 

    GetEpsBBox $canvas $tagName
    switch $anchor {
	sw {
	    set left $x ; set bottom $y
	    set cursor bottom_left_corner
	}
	ne {
	    set right $x ; set top $y
	    set cursor top_right_corner
	}
	se {
	    set right $x ; set bottom $y
	    set cursor bottom_right_corner
	}
	nw {
	    set left $x ; set top $y
	    set cursor top_left_corner
	}
	default {
	    error "anchor can't be $anchor"
	}
    }
    set w [expr $right - $left]
    set h [expr $bottom - $top]
    set options ""
    if { $w > 1 } {
	append options "-width $w "
    }
    if { $h > 1 } {
	append options "-height $h "
    }
    $canvas coords $tagName-image $left $top
    eval $canvas itemconfigure $tagName-image $options
    GetEpsBBox $canvas $tagName
    ResetGrips $canvas $tagName
}

set numGroups 0
set id 0

proc MakeEps { canvas {epsFile ""} {imageFile ""} } {
    global numGroups id gripSize image

#    set image [image create photo -width 200 -height 200]
#    if { $imageFile != "" } {
#	$image configure -file $imageFile
#    }
    set tagName "epsGroup[incr numGroups]"
    $canvas create eps 20 20 \
	-anchor nw \
	-borderwidth 4 \
	-tags "$tagName $tagName-image" \
	-titlecolor white \
	-titlerotate 90 \
	-titleanchor nw \
	-font *helvetica*24* \
	-stipple BLT \
	-outline orange4 \
	-fill orange \
	-file $epsFile \

#	-image $image 
    
    set gripSize 8
    GetEpsBBox $canvas $tagName
    global left top right bottom
    $canvas create rectangle \
	$left $top [expr $left + $gripSize] [expr $top + $gripSize] \
	-tags "$tagName $tagName-grip $tagName-nw" \
	-fill red -outline ""
    $canvas create rectangle \
	[expr $right - $gripSize] [expr $bottom - $gripSize] $right $bottom \
	-tags "$tagName $tagName-grip $tagName-se" \
	-fill red -outline ""
    $canvas create rectangle \
	[expr $right - $gripSize] [expr $top + $gripSize] $right $top \
	-tags "$tagName $tagName-grip $tagName-ne" \
	-fill red -outline ""
    $canvas create rectangle \
	$left $bottom [expr $left + $gripSize] [expr $bottom - $gripSize] \
	-tags "$tagName $tagName-grip $tagName-sw" \
	-fill red -outline ""

    $canvas bind $tagName <ButtonRelease-1> \
	"$canvas configure -cursor {}"
    $canvas bind $tagName-image <ButtonPress-1> \
	"SaveImageCoords $canvas %x %y"
    $canvas bind $tagName-image <B1-Motion> \
	"MoveEpsItem $canvas $tagName %x %y"

    foreach grip { sw ne se nw } {
	$canvas bind $tagName-$grip <ButtonPress-1> \
	    "StartResize $canvas $tagName %x %y $grip"
	$canvas bind $tagName-$grip <B1-Motion> \
	    "ResizeEpsItem $canvas $grip $tagName %x %y"
	$canvas bind $tagName-$grip <ButtonRelease-1> \
	    "EndResize $canvas $tagName %x %y $grip"
	$canvas raise $tagName-$grip
    }
}

source scripts/stipples.tcl

#
# Script to test the BLT "eps" canvas item.
# 

canvas .layout -bg white

button .print -text "Print" -command {
    wm iconify .
    update
    .layout postscript -file eps.ps 
    wm deiconify .
    update
}
button .quit -text "Quit" -command {
    exit 0
}

table . \
    0,0 .layout -fill both -cspan 2 \
    1,0 .print \
    1,1 .quit \

table configure . r1 -resize none

foreach file { ./images/out.ps xy.ps test.ps } {
    if { [file exists $file] } {
        MakeEps .layout $file
    }
}

.layout create rectangle 10 10 50 50 -fill blue -outline white

.layout create text 200 200 \
    -text "This is a text item" \
    -fill yellow \
    -anchor w \
    -font *helvetica*24*

.layout create rectangle 50 50 150 150 -fill green -outline red

wm colormapwindows . .layout

.layout configure -scrollregion [.layout bbox all]
