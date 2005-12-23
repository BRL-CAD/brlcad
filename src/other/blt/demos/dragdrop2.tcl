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

# ----------------------------------------------------------------------
# This procedure is invoked each time a token is grabbed from the
# sample window.  It configures the token to display the current
# color, and returns the color value that is later passed to the
# target handler.
# ----------------------------------------------------------------------

proc package_color {token} {
    set bg [.sample cget -background]
    set fg [.sample cget -foreground]

    $token.label configure -text "Color" -background $bg -foreground $fg
    return $bg
}

# ----------------------------------------------------------------------
# This procedure is invoked each time a token is grabbed from an
# entry widget.  It configures the token to display the current
# string, and returns the string that is later passed to the target
# handler.
# ----------------------------------------------------------------------
proc package_string {str token} {
    if {[string length $str] > 20} {
        set mesg "[string range $str 0 19]..."
    } else {
        set mesg $str
    }
    $token.label configure -text $mesg
    return $str
}

# ----------------------------------------------------------------------
# Main application window...
# ----------------------------------------------------------------------
label .sample -text "Color" -height 2 -borderwidth 3 -relief sunken

#
# Set up the color sample as a drag&drop source for "color" values
# and "string" values
#
drag&drop source .sample -packagecmd {package_color %t} 
drag&drop source .sample handler color
drag&drop source .sample handler string 

#
# Set up the color sample as a drag&drop target for "color" values:
#
drag&drop target .sample handler color {set_color %v}

#
# Establish the appearance of the token window:
#
set token [drag&drop token .sample -activebackground yellow ]
label $token.label -text "Color"
pack $token.label

scale .redScale -label "Red" -orient horizontal \
    -from 0 -to 255 -command adjust_color
frame .redSample -width 20 -height 20 -borderwidth 3 -relief sunken

scale .greenScale -label "Green" -orient horizontal \
    -from 0 -to 255 -command adjust_color
frame .greenSample -width 20 -height 20 -borderwidth 3 -relief sunken

scale .blueScale -label "Blue" -orient horizontal \
    -from 0 -to 255 -command adjust_color
frame .blueSample -width 20 -height 20 -borderwidth 3 -relief sunken

frame .color
label .color.label -text "Color:"
pack .color.label -side left
entry .color.value -width 10
pack .color.value -side left -expand yes -fill both

bind .color.value <KeyPress-Return> {set_color [%W get]}

#
# Set up the entry widget as a drag&drop source for "string" values:
#
drag&drop source .color.value \
	-packagecmd {package_string [%W get] %t} \
	-selftarget yes
drag&drop source .color.value handler string 

#
# Set up the entry widget as a drag&drop target for "string" values:
#
drag&drop target .color.value handler string {
    %W delete 0 end
    %W insert 0 "%v"
}

#
# Establish the appearance of the token window:
#
set token [drag&drop token .color.value]
label $token.label
pack $token.label

# ----------------------------------------------------------------------
# This procedure loads a new color value into this editor.
# ----------------------------------------------------------------------
proc set_color {cval} {
    set rgb [winfo rgb . $cval]

    set rval [expr round([lindex $rgb 0]/65535.0*255)]
    .redScale set $rval

    set gval [expr round([lindex $rgb 1]/65535.0*255)]
    .greenScale set $gval

    set bval [expr round([lindex $rgb 2]/65535.0*255)]
    .blueScale set $bval
}

# ----------------------------------------------------------------------
# This procedure is invoked whenever an RGB slider changes to
# update the color samples in this display.
# ----------------------------------------------------------------------
proc adjust_color {args} {
    set rval [.redScale get]
    .redSample configure -background [format "#%.2x0000" $rval]
    set gval [.greenScale get]
    .greenSample configure -background [format "#00%.2x00" $gval]
    set bval [.blueScale get]
    .blueSample configure -background [format "#0000%.2x" $bval]

    .sample configure -background \
        [format "#%.2x%.2x%.2x" $rval $gval $bval]
    if {$rval+$gval+$bval < 1.5*255} {
        .sample configure -foreground white
    } else {
        .sample configure -foreground black
    }
}

table . \
    0,0 .sample -columnspan 2 -pady {0 4} \
    1,0 .color  -columnspan 2 -padx 4 -pady 4 \
    2,0 .redScale \
    2,1 .redSample \
    3,0 .greenScale \
    3,1 .greenSample \
    4,0 .blueScale \
    4,1 .blueSample 

eval table configure . [winfo children .] -fill both
