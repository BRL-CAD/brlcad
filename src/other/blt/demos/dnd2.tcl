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

if { ([info exists tcl_platform]) && ($tcl_platform(platform) == "windows") } {
    error "This script works only under X11"
}

canvas .c -width 320 -height 320 -background white

blt::table . .c -fill both 

set lastCell ""
set cellWidth 1
set cellHeight 1
proc RedrawWorld { canvas } {
    global cells cellWidth cellHeight

    $canvas delete all

    set width [winfo width $canvas]
    set height [winfo height $canvas]

    set cellWidth [expr $width / 8]
    set cellHeight [expr $height / 8]

    for { set row 0 } { $row < 8 } { incr row } {
	set y [expr $row * $cellHeight]
	set h [expr $y + $cellHeight]
	for { set column 0 } { $column < 8 } { incr column } {
	    set x [expr $column * $cellWidth]
	    set w [expr $x + $cellWidth]
	    $canvas create rectangle $x $y $w $h -fill white -outline "" \
		-tags "$row,$column"
	}
    }

    for { set row 0 } { $row < 8 } { incr row } {
	set y [expr $row * $cellHeight]
	$canvas create line 0 $y $width $y 
    }
    
    for { set column 0 } { $column < 8 } { incr column } {
	set x [expr $column * $cellWidth]
	$canvas create line $x 0 $x $height 
    }
    foreach name [array names cells] {
	set rc [split $name ,]
	set row [lindex $rc 0]
	set column [lindex $rc 1]
	set x [expr ($column * $cellWidth) + 5]
	set y [expr ($row * $cellHeight) + 5]
	set w [expr $cellWidth - 10]
	set h [expr $cellHeight - 10]
	set color [lindex $cells($name) 0]
	set type [lindex $cells($name) 1]
	set pi1_2 [expr 3.14159265358979323846/180.0]
	set points {}
	switch $type {
	    hexagon { 
		lappend points $x [expr $y + $h/2] [expr $x + $w * 1/3] \
		    $y [expr $x + $w * 2/3] $y [expr $x + $w] [expr $y + $h/2] \
		    [expr $x + $w * 2/3] [expr $y + $h] \
		    [expr $x + $w * 1/3] [expr $y + $h] 
	    }
	    parallelogram   { 
		lappend points $x [expr $y + $h * 2/3] \
		    [expr $x + $w * 2/3] $y \
		    [expr $x + $w] [expr $y + $h * 1/3] \
		    [expr $x + $w * 1/3] [expr $y + $h]
	    }
	    triangle { 
		lappend points \
		    $x [expr $y + $h] \
		    [expr $x + $w * 1/2] $y \
		    [expr $x + $w] [expr $y + $h] 
	    }
	}
	eval .c create polygon $points -fill $color -outline black 
    }
}

bind .c <Configure> { RedrawWorld %W }

# ----------------------------------------------------------------------
#  USAGE:  random ?<max>? ?<min>?
#
#  Returns a random number in the range <min> to <max>.
#  If <min> is not specified, the default is 0; if max is not
#  specified, the default is 1.
# ----------------------------------------------------------------------

proc random {{max 1.0} {min 0.0}} {
    global randomSeed

    set randomSeed [expr (7141*$randomSeed+54773) % 259200]
    set num  [expr $randomSeed/259200.0*($max-$min)+$min]
    return $num
}
set randomSeed [clock clicks]

set itemTypes { parallelogram hexagon triangle }
set itemTypes { hexagon triangle parallelogram }

for { set i 0 } { $i < 20 } { incr i } {
    while { 1 } {
	set row [expr int([random 8])]
	set column [expr int([random 8])]
	set type [expr int([random 3])]
	set type [lindex $itemTypes $type]
	if { ![info exists cells($row,$column)] } {
	    set r [expr int([random 256 128])]
	    set g [expr int([random 256 128])]
	    set b [expr int([random 256 128])]
	    set cells($row,$column) [format "#%.2x%.2x%.2x %s" $r $g $b $type]
	    break
	}
    }
}

proc ScreenToCell { widget x y }  {
    global cellWidth cellHeight 
    set column [expr $x / $cellWidth]
    set row [expr $y / $cellHeight]
    return $row,$column
}


set count 0
foreach i [winfo interps] {
    puts $i
    if { [string match "dnd2.tcl*" $i] } {
	incr count
    }
}

if { $count == 1 }  {
    toplevel .info
    raise .info
    text .info.text -width 65 -height 12 -font { Helvetica 10 } -bg white \
	-tabs { 0.25i } 
    .info.text insert end {
	This is a more involved example of the new "dnd" command.  
	Run this script again to get another window. You can then drag
	and drop symbols between the windows by clicking with the left 
	mouse button on a symbol.  

	It demonstates how to 
		o Drag-and-drop on specific areas (canvas items) of a widget.
		o How to receive and handle Enter/Leave/Motion events in the target.
		o How to send drag feedback to the source.
		o Use a drag threshold. 
    }
    button .info.quit -text "Dismiss" -command { destroy .info }
    blt::table .info \
	0,0 .info.text -fill both \
	1,0 .info.quit
}


# -----------------------------------------------------------------
# 
#  Setup finished.  Start of drag-and-drop code here.
# 

# Set up the entire canvas as a drag&drop source.

dnd register .c -source yes  -dragthreshold 5 -button 1

# Register code to pick up the information about a canvas item

dnd getdata .c color GetColor

proc GetColor { widget args } {
    array set info $args
    global itemInfo
    set id $itemInfo($info(timestamp))
    set color [$widget itemcget $id -fill]
    set ncoords [llength [$widget coords $id]]
    if { $ncoords == 6 } {
	set type triangle
    } elseif { $ncoords == 8 } {
	set type parallelogram
    } elseif { $ncoords ==  12 } {
        set type hexagon
    } else {
	error "unknown type n=$ncoords"
    }
    return [list $color $type]
}

dnd configure .c -package PackageSample 

proc PackageSample { widget args } {
    array set info $args
    
    # Check if we're over a canvas item
    set items [$widget find overlapping $info(x) $info(y) $info(x) $info(y)]
    set pickedItem ""
    foreach i $items {
	if { [$widget type $i] == "polygon" } {
	    set pickedItem $i
	    break
	}
    }
    if { $pickedItem == "" } {
	# Cancel the drag
	puts "Cancel the drag x=$info(x) y=$info(y)"
	return 0
    }
    set fill [$widget itemcget $pickedItem -fill]
    set outline [$widget itemcget $pickedItem -outline]

    set ncoords [llength [$widget coords $pickedItem]]
    if { $ncoords == 6 } {
	set type triangle
    } elseif { $ncoords == 8 } {
	set type parallelogram
    } elseif { $ncoords ==  12 } {
        set type hexagon
    } else {
	error "unknown type n=$ncoords"
    }
    set tag [ScreenToCell $widget $info(x) $info(y)]
    $info(token).label configure -background $fill -foreground $outline \
	-text $type 
    update idletasks
    update
    global itemInfo
    set itemInfo($info(timestamp)) $pickedItem 
    return 1
}

# Configure a set of animated cursors.

dnd configure .c -cursors {
    { @bitmaps/hand/hand01.xbm bitmaps/hand/hand01m.xbm  black white }
    { @bitmaps/hand/hand02.xbm bitmaps/hand/hand02m.xbm  black white }
    { @bitmaps/hand/hand03.xbm bitmaps/hand/hand03m.xbm  black white }
    { @bitmaps/hand/hand04.xbm bitmaps/hand/hand04m.xbm  black white }
    { @bitmaps/hand/hand05.xbm bitmaps/hand/hand05m.xbm  black white }
    { @bitmaps/hand/hand06.xbm bitmaps/hand/hand06m.xbm  black white } 
    { @bitmaps/hand/hand07.xbm bitmaps/hand/hand07m.xbm  black white }
    { @bitmaps/hand/hand08.xbm bitmaps/hand/hand08m.xbm  black white }
    { @bitmaps/hand/hand09.xbm bitmaps/hand/hand09m.xbm  black white }
    { @bitmaps/hand/hand10.xbm bitmaps/hand/hand10m.xbm  black white }
    { @bitmaps/hand/hand11.xbm bitmaps/hand/hand11m.xbm  black white }
    { @bitmaps/hand/hand12.xbm bitmaps/hand/hand12m.xbm  black white }
    { @bitmaps/hand/hand13.xbm bitmaps/hand/hand13m.xbm  black white }
    { @bitmaps/hand/hand14.xbm bitmaps/hand/hand14m.xbm  black white }
}

# Create a widget to place in the drag-and-drop token

set token [dnd token window .c]

label $token.label -bd 2 -highlightthickness 1  
pack $token.label
dnd token configure .c \
    -borderwidth 2 \
    -relief raised -activerelief raised  \
    -outline pink -fill red \
    -anchor s


dnd configure .c -target yes

dnd setdata .c color { 
    NewObject 
}

proc NewObject { widget args } {
    array set info $args
    set tag [ScreenToCell $widget $info(x) $info(y)]
    global cells
    if { [info exists cells($tag)] } {
	error "Cell already exists"
    }
    set cells($tag) $info(value)
    RedrawWorld $widget

}

dnd configure .c -onmotion OnMotion -onenter OnMotion -onleave OnMotion 

proc OnMotion { widget args } {
    global cells lastCell

    array set info $args
    set tag [ScreenToCell $widget $info(x) $info(y)]
    if { $lastCell != "" } {
	$widget itemconfigure $lastCell -fill white -outline "" -width 1 \
	    -stipple ""
    }
    # Check that we're not over a canvas item
    if { ![info exists cells($tag)] } {
	$widget itemconfigure $tag -outline lightblue -fill lightblue \
	    -width 2 -stipple BLT
	set lastCell $tag
	return 1
    }
    return 0
}

