##                 C O L O R E N T R Y . T C L
#
# Author -
#	Bob Parker
#
# Source -
#	The U. S. Army Research Laboratory
#	Aberdeen Proving Ground, Maryland  21005
#
# Distribution Notice -
#	Re-distribution of this software is restricted, as described in
#       your "Statement of Terms and Conditions for the Release of
#       The BRL-CAD Package" agreement.
#
# Copyright Notice -
#       This software is Copyright (C) 1998 by the United States Army
#       in all countries except the USA.  All rights reserved.
#
# Description -
#	ColorEntry instances are used to specify colors.
#

itk::usual ColorEntry {
    keep -tearoff
}

class cadwidgets::ColorEntry {
    inherit cadwidgets::ComboBox

    constructor {args} {}
    destructor {}

    public method rgbValid {r g b}
    public method getColor {}
    public method setColor {r g b}

    private method chooser {}
    private method updateColor {}
    private method colorOK {}
}

body cadwidgets::ColorEntry::constructor {args} {
    $itk_component(menu) add command -label black \
	    -command [code $this setColor 0 0 0]
    $itk_component(menu) add command -label white \
	    -command [code $this setColor 255 255 255]
    $itk_component(menu) add command -label red \
	    -command [code $this setColor 255 0 0]
    $itk_component(menu) add command -label green \
	    -command [code $this setColor 0 255 0]
    $itk_component(menu) add command -label blue\
	    -command [code $this setColor 0 0 255]
    $itk_component(menu) add command -label yellow \
	    -command [code $this setColor 255 255 0]
    $itk_component(menu) add command -label cyan \
	    -command [code $this setColor 0 255 255]
    $itk_component(menu) add command -label magenta \
	    -command [code $this setColor 255 0 255]
    $itk_component(menu) add separator
    $itk_component(menu) add command -label "Color Tool..." \
	    -command [code $this chooser]

    eval itk_initialize $args
    bind $itk_component(entry) <Return> [code $this updateColor]
}

body cadwidgets::ColorEntry::chooser {} {
    cadColorWidget dialog $itk_interior color \
	    -title "Color Chooser" \
	    -initialcolor [$itk_component(menubutton) cget -background] \
	    -ok [code $this colorOK] \
	    -cancel "cadColorWidget_destroy $itk_interior.color"
}

body cadwidgets::ColorEntry::getColor {} {
    return [getText]
}

body cadwidgets::ColorEntry::setColor {r g b} {
    if {![rgbValid $r $g $b]} {
	error "Improper color specification - $r $g $b"
    }

    setText "$r $g $b"
    $itk_component(menubutton) configure \
	    -bg [format "#%02x%02x%02x" $r $g $b]
}

body cadwidgets::ColorEntry::updateColor {} {
    eval setColor [getText]
}

body cadwidgets::ColorEntry::colorOK {} {
    upvar #0 $itk_interior.color data
    setColor $data(red) $data(green) $data(blue)
    cadColorWidget_destroy $itk_interior.color
}

body cadwidgets::ColorEntry::rgbValid {r g b} {
    if {![string is integer $r]} {
	    return 0
    }
    if {![string is integer $g]} {
	    return 0
    }
    if {![string is integer $b]} {
	    return 0
    }
    if {$r < 0 || 255 < $r} {
	return 0
    }
    if {$g < 0 || 255 < $g} {
	return 0
    }
    if {$b < 0 || 255 < $b} {
	return 0
    }

    return 1
}
