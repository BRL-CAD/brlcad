#                       C O L O R . T C L
# BRL-CAD
#
# Copyright (c) 2004-2007 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# version 2.1 as published by the Free Software Foundation.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this file; see the file named COPYING for more
# information.
#
###
##
#			C O L O R . T C L
#
# Author -
#	Robert G. Parker
#
# Source -
#       The U. S. Army Research Laboratory
#       Aberdeen Proving Ground, Maryland  21005
#
#
# Description -
#	Color utilities.
#

## color_entry_build --
#
# Build a color entry widget. The color entry widget consists of
# an entry and a menubutton widget. The menu associated with the
# menubutton contains some canned color entries as well as an
# entry that invokes a user specified command for setting the color.
#
proc color_entry_build { top key vn user_color_cmd width icolor canned_colors } {
    frame $top.$key\F -relief sunken -bd 2
    entry $top.$key\E -relief flat -width $width -textvar $vn
    hoc_register_data $top.$key\E "Color"\
	    { { summary "Enter a color specification. The color
can be specified using three integers (i.e. r g b)
in the range 0 to 255. For example:

\tblack\t\t0 0 0
\twhite\t\t255 255 255
\tred\t\t255 0 0
\tgreen\t\t0 255 0
\tblue\t\t0 0 255
\tyellow\t\t255 255 0
\tcyan\t\t0 255 255
\tmagenta\t\t255 0 255

The color can also be specified using a six character
hexadecimal number (i.e. #ff0000 specifies red). And
of course the color can be specified by name. For
example, the name \"blue\" specifies a color with
an rgb value of 0 0 255.

Note - when entering colors directly,
pressing \"Enter\" will update the color
of the menubutton." } }

    menubutton $top.$key\MB -relief raised -bd 2\
	    -menu $top.$key\MB.m -indicatoron 1
    hoc_register_data $top.$key\MB "Color Menu"\
	    { { summary "Pop up a menu of colors. Also included
in the menu is an entry for a color tool." } }
    menu $top.$key\MB.m -title "Color" -tearoff 0

if {$canned_colors == "rt"} {
    $top.$key\MB.m add command -background #000032 -activebackground #000032 \
	    -command "set $vn \"0 0 50\"; \
	              setWidgetRGBColor $top.$key\MB $vn \$$vn"
    $top.$key\MB.m add command -background #84dfff -activebackground #84dfff \
	    -command "set $vn \"132 223 255\"; \
	              setWidgetRGBColor $top.$key\MB $vn \$$vn"
    $top.$key\MB.m add command -background #008000 -activebackground #008000 \
	    -command "set $vn \"0 128 0\"; \
	              setWidgetRGBColor $top.$key\MB $vn \$$vn"
    $top.$key\MB.m add command -background #f0f0a0 -activebackground #f0f0a0 \
	    -command "set $vn \"240 240 160\"; \
	              setWidgetRGBColor $top.$key\MB $vn \$$vn"
    $top.$key\MB.m add command -background #ffffff -activebackground #ffffff \
	    -command "set $vn \"255 255 255\"; \
	              setWidgetRGBColor $top.$key\MB $vn \$$vn"
    $top.$key\MB.m add command -background #808080 -activebackground #808080 \
	    -command "set $vn \"128 128 128\"; \
	              setWidgetRGBColor $top.$key\MB $vn \$$vn"
    $top.$key\MB.m add command -background #000000 -activebackground #000000 \
	    -command "set $vn \"0 0 0\"; \
	              setWidgetRGBColor $top.$key\MB $vn \$$vn"
} else {
    $top.$key\MB.m add command -background #000000 -activebackground #000000 \
	    -command "set $vn \"0 0 0\"; \
	              setWidgetRGBColor $top.$key\MB $vn \$$vn"
    $top.$key\MB.m add command -background #ffffff -activebackground #ffffff \
	    -command "set $vn \"255 255 255\"; \
	              setWidgetRGBColor $top.$key\MB $vn \$$vn"
    $top.$key\MB.m add command -background #ff0000 -activebackground #ff0000 \
	    -command "set $vn \"255 0 0\"; \
	              setWidgetRGBColor $top.$key\MB $vn \$$vn"
    $top.$key\MB.m add command -background #00ff00 -activebackground #00ff00 \
	    -command "set $vn \"0 255 0\"; \
	              setWidgetRGBColor $top.$key\MB $vn \$$vn"
    $top.$key\MB.m add command -background #0000ff -activebackground #0000ff \
	    -command "set $vn \"0 0 255\"; \
	              setWidgetRGBColor $top.$key\MB $vn \$$vn"
    $top.$key\MB.m add command -background #ffff00 -activebackground #ffff00 \
	    -command "set $vn \"255 255 0\"; \
	              setWidgetRGBColor $top.$key\MB $vn \$$vn"
    $top.$key\MB.m add command -background #00ffff -activebackground #00ffff \
	    -command "set $vn \"0 255 255\"; \
	              setWidgetRGBColor $top.$key\MB $vn \$$vn"
    $top.$key\MB.m add command -background #ff00ff -activebackground #ff00ff \
	    -command "set $vn \"255 0 255\"; \
	              setWidgetRGBColor $top.$key\MB $vn \$$vn"
}
    $top.$key\MB.m add separator
    $top.$key\MB.m add command -label "Color Tool..." -command $user_color_cmd
    hoc_register_menu_data "Color" "Color Tool..." "Color Tool"\
	    { { summary "The color tool allows the user to specify
a color using either RGB or HSV. The resulting
color is RGB." } }

    # initialize color
    catch {[list uplevel #0 [list set $vn $icolor]]}
    setWidgetRGBColor $top.$key\MB $vn $icolor

    grid $top.$key\E $top.$key\MB -sticky "nsew" -in $top.$key\F
    grid columnconfigure $top.$key\F 0 -weight 1
    grid rowconfigure $top.$key\F 0 -weight 1

    bind $top.$key\E <Return> "setWidgetColor $top.$key\MB $vn \$$vn"

    return $top.$key\F
}

proc color_entry_destroy { top key } {
    grid forget $top.$key\F
    destroy $top.$key\F $top.$key\E $top.$key\MB
}

proc color_entry_update { top key vn icolor } {
    if [winfo exists $top.$key\MB] {
	setWidgetColor $top.$key\MB $vn $icolor
    }
}

proc color_entry_chooser { id top key title vn vin } {
    set child color

    cadColorWidget dialog $top $child\
	    -title $title\
	    -initialcolor [$top.$key\MB cget -background]\
	    -ok "color_entry_ok $id $top $top.$child $key $vn $vin"\
	    -cancel "cadColorWidget_destroy $top.$child"
}

proc color_entry_ok { id top w key vn vin } {
    upvar #0 $w data $vn varname

    $top.$key\MB configure -bg $data(finalColor)
    $top.$key\MB configure -activebackground $data(finalColor)

    if {$vin == ""} {
	set varname "$data(red) $data(green) $data(blue)"
    } else {
	set varname($vin) "$data(red) $data(green) $data(blue)"
    }

    destroy $w
    unset data
}

## -- setWidgetRGBColor
#
# Set the widget color given an rgb string.
#
proc setWidgetRGBColor { w vn rgb } {

    if ![winfo exists $w] {
	return -code error "setWidgetRGBColor: bad Tk window name --> $w"
    }

    if {$rgb != ""} {
	set result [regexp "^(\[0-9\]+)\[ \]+(\[0-9\]+)\[ \]+(\[0-9\]+)$" \
		$rgb cmatch red green blue]
	if {!$result} {
	    # reset varname to properly reflect the current color of the widget
	    set varname [$w cget -bg]
	    return -code error "Improper color specification - $rgb"
	}
    } else {
	return
    }

    $w configure -bg [format "#%02x%02x%02x" $red $green $blue]
    $w configure -activebackground [format "#%02x%02x%02x" $red $green $blue]
}

## -- setWidgetColor
#
# Set the widget color given a color string.
#
proc setWidgetColor { w vn color } {
    set rgb [getRGBorReset $w $vn $color]
    setWidgetRGBColor $w $vn $rgb
}


## -- getRGBorReset
#
# Get the RGB value corresponding to the given color. Failing that
# reset the variable vn to the value provided by the widget w
#
proc getRGBorReset { w vn color } {
    upvar #0 $vn varname

    if ![winfo exists $w] {
	return -code error "getRGBorReset: bad Tk window name --> $w"
    }

    # convert to RGB
    set result [catch {getRGB $w $color} rgb]
    if {$result} {
	# reset varname to properly reflect the current color of the widget
	set varname [$w cget -bg]
	return -code error $rgb
    }

    return $rgb
}

## -- getRGB
#
# Get the RGB value corresponding to the given color.
#
proc getRGB { w color } {
    # convert to RGB
    set rgb [cadColorWidget_getRGB $w $color]

    # Check to make sure the color is valid
    if {[llength $rgb] == 3} {
	return $rgb
    }

    # not a color recognized by cadColorWidget_getRGB
    set result [regexp "^\[ \]*(\[0-9\]+)\[ \]+(\[0-9\]+)\[ \]+(\[0-9\]+)\[ \]*$" \
	    $color cmatch red green blue]
    if {!$result ||
        $red < 0 || $red > 255 ||
        $green < 0 || $green > 255 ||
        $blue < 0 || $blue > 255 } {
	return -code error "Improper color specification - $color"
    }

    return $color
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
