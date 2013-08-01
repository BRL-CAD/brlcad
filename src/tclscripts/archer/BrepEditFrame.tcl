#               B R E P E D I T F R A M E . T C L
# BRL-CAD
#
# Copyright (c) 2013 United States Government as represented by
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

::itcl::class BrepEditFrame {
    inherit GeometryEditFrame

    constructor {args} {}
    destructor {}

    public {}

    protected {
	# GeometryEditFrame overrides
	method buildUpperPanel
	method buildLowerPanel
	method initEditState {}
    }

    private {}
}


# ------------------------------------------------------------
#                      CONSTRUCTOR
# ------------------------------------------------------------

::itcl::body BrepEditFrame::constructor {args} {
    eval itk_initialize $args
}


# ------------------------------------------------------------
#                      PUBLIC METHODS
# ------------------------------------------------------------



# ------------------------------------------------------------
#                      PROTECTED METHODS
# ------------------------------------------------------------

::itcl::body BrepEditFrame::buildUpperPanel {} {
}

::itcl::body BrepEditFrame::buildLowerPanel {} {
    set parent [$this childsite lower]

    itk_component add editCV {
	::ttk::checkbutton $parent.editCV \
	    -text "Edit Control Vertices"
    } {}
}

::itcl::body BrepEditFrame::initEditState {} {
    GeometryEditFrame::initEditState
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
