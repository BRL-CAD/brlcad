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

class cadwidgets::ColorEntry {
    inherit cadwidgets::ComboBox

    constructor {args} {}
    destructor {}

    public method rgbvalid {r g b}

    private method chooser {}
    private method setcolor {r g b}
    private method setMBcolor {}
    private method colorok {}
}

body cadwidgets::ColorEntry::constructor {args} {
    $itk_component(menu) add command -label black \
	    -command [code $this setcolor 0 0 0]
    $itk_component(menu) add command -label white \
	    -command [code $this setcolor 255 255 255]
    $itk_component(menu) add command -label red \
	    -command [code $this setcolor 255 0 0]
    $itk_component(menu) add command -label green \
	    -command [code $this setcolor 0 255 0]
    $itk_component(menu) add command -label blue\
	    -command [code $this setcolor 0 0 255]
    $itk_component(menu) add command -label yellow \
	    -command [code $this setcolor 255 255 0]
    $itk_component(menu) add command -label cyan \
	    -command [code $this setcolor 0 255 255]
    $itk_component(menu) add command -label magenta \
	    -command [code $this setcolor 255 0 255]
    $itk_component(menu) add separator
    $itk_component(menu) add command -label "Color Tool..." \
	    -command [code $this chooser]

    eval itk_initialize $args
    bind $itk_component(entry) <Return> [code $this setMBcolor]
}

body cadwidgets::ColorEntry::chooser {} {
    cadColorWidget dialog $itk_interior color \
	    -title "Color Chooser" \
	    -initialcolor [$itk_component(menubutton) cget -background] \
	    -ok [code $this colorok] \
	    -cancel "cadColorWidget_destroy $itk_interior.color"
}

body cadwidgets::ColorEntry::setcolor {r g b} {
    if {![rgbvalid $r $g $b]} {
	error "Improper color specification - $r $g $b"
    }

    settext "$r $g $b"
    $itk_component(menubutton) configure \
	    -bg [format "#%02x%02x%02x" $r $g $b]
}

body cadwidgets::ColorEntry::setMBcolor {} {
    eval setcolor [gettext]
}

body cadwidgets::ColorEntry::colorok {} {
    upvar #0 $itk_interior.color data
    setcolor $data(red) $data(green) $data(blue)
    cadColorWidget_destroy $itk_interior.color
}

body cadwidgets::ColorEntry::rgbvalid {r g b} {
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
