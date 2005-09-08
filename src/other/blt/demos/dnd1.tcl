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
    source scripts/send.tcl
    SendInit
    SendVerify
}

proc OnEnter { widget args } {
    array set info $args
    $widget configure -highlightbackground red
    return 1
}

proc OnMotion { widget args } {
    array set info $args
    set x1 [$widget cget -bd]
    set x1 20
    set y1 $x1
    set x2 [expr [winfo width $widget] - $x1]
    set y2 [expr [winfo height $widget] - $y1]
    if { ($info(x) >= $x1) && ($info(x) <= $x2) && 
	 ($info(y) >= $y1) && ($info(y) <= $y2) } {
	$widget configure -highlightbackground red
	return 1
    }
    $widget configure -highlightbackground grey
    return 0
}

proc OnLeave { widget args } {
    $widget configure -highlightbackground grey
    return 0
}

option add *OnEnter	OnEnter
option add *OnLeave	OnLeave
option add *OnMotion	OnMotion
	
# ----------------------------------------------------------------------
# This procedure is invoked each time a token is grabbed from the
# sample window.  It configures the token to display the current
# color, and returns the color value that is later passed to the
# target handler.
# ----------------------------------------------------------------------

proc PackageSample { widget args } {
    array set info $args
    set bg [.sample cget -background]
    set fg [.sample cget -foreground]
    $info(token).label configure -background $bg -foreground $fg
    return 1
}

proc ShowResult { widget args } {
    array set info $args
    puts "drop transaction($info(timestamp)) completed: result was $info(action)" 
} 


# ----------------------------------------------------------------------
# Main application window...
# ----------------------------------------------------------------------
image create photo openFolder -format gif -data {
R0lGODdhEAAOAPIAAP///wAAAH9/f9nZ2f//AAAAAAAAAAAAACwAAAAAEAAOAAADOwgqzPoQ
iDjjAoPkIZuTgCZykBCA2ziaXusRrFUGQ5zeRMCcE76xvJBPozuBVCmT0eUKGAHOqFQqqwIS
ADs=
    }
label .sample -text "Color" -height 12 -width 20 -bd 2 -relief raised  \
    -highlightthickness 2 

set cursors {
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


# Set up the color sample as a drag&drop source and target for "color" values:
dnd register .sample -source yes -target yes \
    -package PackageSample \
    -result ShowResult \
    -cursors $cursors

dnd getdata .sample color GetColor
dnd setdata .sample color SetColor

# Establish the appearance of the token window:
set token [dnd token window .sample]
label $token.label -text "Color" -bd 2 -highlightthickness 1  
pack $token.label
dnd token configure .sample -borderwidth 2 \
    -relief raised -activerelief raised  \
    -outline pink -fill red \
    -anchor s

if 1 {
scale .redScale -label "Red" -orient horizontal \
    -from 0 -to 255 -command adjust_color
frame .red -width 20 -height 20 -borderwidth 3 -relief sunken

scale .greenScale -label "Green" -orient horizontal \
    -from 0 -to 255 -command adjust_color
frame .green -width 20 -height 20 -borderwidth 3 -relief sunken

scale .blueScale -label "Blue" -orient horizontal \
    -from 0 -to 255 -command adjust_color
frame .blue -width 20 -height 20 -borderwidth 3 -relief sunken

# ----------------------------------------------------------------------
# This procedure loads a new color value into this editor.
# ----------------------------------------------------------------------
proc GetColor { widget args } {
    return [$widget cget -bg]
}

proc SetColor { widget args } {
    array set info $args 
    set rgb [winfo rgb . $info(value)]
    set r [lindex $rgb 0]
    set g [lindex $rgb 1]
    set b [lindex $rgb 2]
    
    .redScale set [expr round($r/65535.0 * 255)]
    .greenScale set [expr round($g/65535.0 * 255)]
    .blueScale set [expr round($b/65535.0 * 255)]
}

# ----------------------------------------------------------------------
# This procedure is invoked whenever an RGB slider changes to
# update the color samples in this display.
# ----------------------------------------------------------------------
proc adjust_color {args} {
    set rval [.redScale get]
    .red configure -background [format "#%.2x0000" $rval]
    set gval [.greenScale get]
    .green configure -background [format "#00%.2x00" $gval]
    set bval [.blueScale get]
    .blue configure -background [format "#0000%.2x" $bval]

    .sample configure -background \
        [format "#%.2x%.2x%.2x" $rval $gval $bval]
    if {$rval+$gval+$bval < 1.5*255} {
        .sample configure -foreground white
    } else {
        .sample configure -foreground black
    }
}
table . .redScale    1,0 -fill both
table . .red	     1,1 -fill both
table . .greenScale  2,0 -fill both
table . .green	     2,1 -fill both
table . .blueScale   3,0 -fill both
table . .blue	     3,1 -fill both

}
table . .sample      0,0 -columnspan 2 -fill both -pady {0 4}

proc random {{max 1.0} {min 0.0}} {
    global randomSeed

    set randomSeed [expr (7141*$randomSeed+54773) % 259200]
    set num  [expr $randomSeed/259200.0*($max-$min)+$min]
    return $num
}
set randomSeed [clock clicks]

.redScale set [expr round([random 255.0])]
.blueScale set [expr round([random 255.0])]
.greenScale set [expr round([random 255.0])]
bind .sample <KeyPress-Escape> { dnd cancel .sample }
focus .sample


