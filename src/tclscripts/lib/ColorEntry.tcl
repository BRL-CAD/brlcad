#                  C O L O R E N T R Y . T C L
# BRL-CAD
#
# Copyright (C) 1998-2005 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# as published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this file; see the file named COPYING for more
# information.
#
###
#
# Author -
#	Bob Parker
#
# Source -
#	The U. S. Army Research Laboratory
#	Aberdeen Proving Ground, Maryland  21005
#
#
#
# Description -
#	ColorEntry instances are used to specify colors.
#

::itk::usual ColorEntry {
    keep -tearoff
}

::itcl::class cadwidgets::ColorEntry {
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

::itcl::body cadwidgets::ColorEntry::constructor {args} {
    $itk_component(menu) add command -label black \
	    -command [::itcl::code $this setColor 0 0 0]
    $itk_component(menu) add command -label white \
	    -command [::itcl::code $this setColor 255 255 255]
    $itk_component(menu) add command -label red \
	    -command [::itcl::code $this setColor 255 0 0]
    $itk_component(menu) add command -label green \
	    -command [::itcl::code $this setColor 0 255 0]
    $itk_component(menu) add command -label blue\
	    -command [::itcl::code $this setColor 0 0 255]
    $itk_component(menu) add command -label yellow \
	    -command [::itcl::code $this setColor 255 255 0]
    $itk_component(menu) add command -label cyan \
	    -command [::itcl::code $this setColor 0 255 255]
    $itk_component(menu) add command -label magenta \
	    -command [::itcl::code $this setColor 255 0 255]
    $itk_component(menu) add separator
    $itk_component(menu) add command -label "Color Tool..." \
	    -command [::itcl::code $this chooser]

    eval itk_initialize $args
    bind $itk_component(entry) <Return> [::itcl::code $this updateColor]
}

::itcl::body cadwidgets::ColorEntry::chooser {} {
    cadColorWidget dialog $itk_interior color \
	    -title "Color Chooser" \
	    -initialcolor [$itk_component(menubutton) cget -background] \
	    -ok [::itcl::code $this colorOK] \
	    -cancel "cadColorWidget_destroy $itk_interior.color"
}

::itcl::body cadwidgets::ColorEntry::getColor {} {
    return [getText]
}

::itcl::body cadwidgets::ColorEntry::setColor {r g b} {
    if {![rgbValid $r $g $b]} {
	error "Improper color specification - $r $g $b"
    }

    setText "$r $g $b"
    $itk_component(menubutton) configure \
	    -bg [format "#%02x%02x%02x" $r $g $b]
}

::itcl::body cadwidgets::ColorEntry::updateColor {} {
    eval setColor [getText]
}

::itcl::body cadwidgets::ColorEntry::colorOK {} {
    upvar #0 $itk_interior.color data
    setColor $data(red) $data(green) $data(blue)
    cadColorWidget_destroy $itk_interior.color
}

::itcl::body cadwidgets::ColorEntry::rgbValid {r g b} {
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

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
