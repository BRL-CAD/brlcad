#!../bltwish
#
#  Tk version of xcolors
#

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
set numCols 0
set numRows 0
set maxCols 15
set cellWidth 40
set cellHeight 20
set numCells 0
set lastCount 0
set beginInput(0) 0
set map 0
set entryCount 0
set lastTagId {}

scrollbar .xscroll -command { .canvas xview } -orient horizontal
scrollbar .yscroll -command { .canvas yview }

label .sample \
    -font -*-new*century*schoolbook*-bold-r-*-*-24-*-*-*-*-*-*-* \
    -text {"Bisque is Beautiful".}

button .name -font -*-helvetica-medium-r-*-*-18-*-*-*-*-*-*-* \
    -command "AddSelection name"
button .rgb -font -*-courier-medium-r-*-*-18-*-*-*-*-*-*-* \
    -command "AddSelection rgb"

canvas .canvas \
    -confine 1 \
    -yscrollcommand { .yscroll set } \
    -width [expr 16*$cellWidth] -height 400  \
    -scrollregion [list 0 0 [expr 16*$cellWidth] 800]

frame .border -bd 2 -relief raised

label .status \
    -anchor w \
    -font -*-helvetica-medium-r-*-*-14-*-*-*-*-*-*-* 

button .quit -text "Quit" -command "exit"
button .next -text "Next" -command "DisplayColors next"
button .prev -text "Previous" -command "DisplayColors last"

selection handle .name GetColor
selection handle .rgb GetValue

bind .name <Enter> { 
    .status config -text \
	"Press button to write color name into primary selection"
}

bind .rgb <Enter> { 
    .status config -text \
	"Press button to write RGB value into primary selection"
}
bind .name <Leave> { 
    .status config -text ""
}

bind .rgb <Leave> { 
    .status config -text ""
}

bind .canvas <Enter> { 
    .status config -text \
	"Press button 1 to change background; Button 2 changes foreground"
}


table . \
    .sample 0,0 -cspan 2 -fill both -reqheight 1i \
    .name 1,0 -fill both -anchor w \
    .rgb 1,1 -fill both -anchor w \
    .canvas 2,0 -cspan 2 -fill both \
    .yscroll 2,2 -fill y \
    .border 3,0 -cspan 2 -fill x -reqheight 8 \
    .status 4,0 -cspan 2 -fill both  \
    .quit 4,1 -anchor e -reqwidth 1i -fill y -padx 10 -pady 4 \
    .prev 5,0 -anchor e -reqwidth 1i -fill y -padx 10 -pady 4 \
    .next 5,1 -anchor e -reqwidth 1i -fill y -padx 10 -pady 4 

proc AddSelection { what } {
    selection own .$what
    if {$what == "name" } {
	set mesg "Color name written into primary selection"
    } else {
	set mesg "RGB value written into primary selection"
    }
    .status config -text $mesg
}

proc GetColor { args } {
    return [lindex [.name config -text] 4]
}

proc GetValue { args } {
    return [lindex [.rgb config -text] 4]
}

proc ShowInfo { tagId what info } {
    global lastTagId

    if { $lastTagId != {} } {
	.canvas itemconfig $lastTagId -width 1
    }
    .canvas itemconfig $tagId -width 3
    set lastTagId $tagId

    set name [lindex $info 3]
    .name config -text $name 
    set value [format "#%0.2x%0.2x%0.2x" \
	       [lindex $info 0] [lindex $info 1] [lindex $info 2]]
    .rgb config -text $value
    .sample config $what $name
    .status config -bg $name
}


proc MakeCell { info } {
    global numCols numRows maxCols cellWidth cellHeight numCells 

    set x [expr $numCols*$cellWidth]
    set y [expr $numRows*$cellHeight]
    set color [lindex $info 3]

    if [catch {winfo rgb . $color}] {
	return "ok"
    }
#    if { [tk colormodel .] != "color" } {
#	bind . <Leave> { 
#	    .status config -text "Color table full after $numCells entries."
#	}
#	.status config -text "Color table full after $numCells entries."
#	return "out of colors"
#    }
    set id [.canvas create rectangle \
	    $x $y [expr $x+$cellWidth] [expr $y+$cellHeight] \
		-fill $color -outline black]
    if { $color == "white" } {
	global whiteTagId
	set whiteTagId $id
    }

    .canvas bind $id <1> [list ShowInfo $id -bg $info]
    .canvas bind $id <2> [list ShowInfo $id -fg $info]
    
    incr numCols
    if { $numCols > $maxCols } {
	set numCols 0
	incr numRows
    }
    return "ok"
}

proc DisplayColors { how } {
    global lastCount numCells cellHeight numRows numCols rgbText 
    global map beginInput
    
#    tk colormodel . color
    set initialized no

    if { $how == "last" } {
	if { $map == 0 } {
	    return
	}
	set map [expr $map-1]
    } else {
	incr map
	if ![info exists beginInput($map)] {
	    set beginInput($map) $lastCount
	}
    }

    set start $beginInput($map)

    if { $numCells > 0 } {
	.canvas delete all
	set numRows 0
	set numCols 0
	set initialized yes
    }

    set input [lrange $rgbText $start end]
    set lineCount $start
    set entryCount 0
    foreach i $input {
	incr lineCount
	if { [llength $i] == 4 } {
	    if { [MakeCell $i] == "out of colors"  } {
		break
	    }
	    incr entryCount
	}
    }
    if { $entryCount == 0 } {
	bind . <Leave> { 
	    .status config -text "No more entries in RGB database"
	}
	.status config -text "No more entries in RGB database"
    } 
    set lastCount $lineCount
    proc tkerror {args} { 
	#dummy procedure
    }

    if { $initialized == "no" } {
	global cellWidth

	set height [expr $cellHeight*($numRows+1)]
	.canvas config -scrollregion [list 0 0 [expr 16*$cellWidth] $height]
	if { $height < 800 } {
	    .canvas config -height $height
	}
	global whiteTagId
	if [info exists whiteTagId] {
	    ShowInfo $whiteTagId -bg {255 255 255 white}
	}
    }
    update idletasks
    update
    rename tkerror {}
}

wm min . 0 0

foreach location {
	/usr/X11R6
	/util/X11R6
	/usr/openwin
	/usr/dt
} {
    set file [file join $location lib X11 rgb.txt]
    if { [file exists $file] } {
       break
    }
}
set in [open $file "r"]
set rgbText [read $in]
close $in
set rgbText [split $rgbText \n]
DisplayColors next
wm min . 0 0


