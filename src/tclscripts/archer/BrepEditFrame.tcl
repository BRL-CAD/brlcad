#               B R E P E D I T F R A M E . T C L
# BRL-CAD
#
# Copyright (c) 2013-2014 United States Government as represented by
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

    public {
	method CVEditMode {_dname _obj _x _y}
	method clearEditState {{_clearModeOnly 0} {_inifFlag 0}}
    }

    protected {
	method initCVEdit {}
	# GeometryEditFrame overrides
	method buildUpperPanel
	method buildLowerPanel
    }

    private {
	method toggleCVEdit {}
    }
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

::itcl::body BrepEditFrame::CVEditMode {_dname _obj _x _y} {
}

::itcl::body BrepEditFrame::clearEditState {{_clearModeOnly 0} {_initFlag 0}} {
    set mEditCommand ""
    chain $_clearModeOnly $_initFlag
}

# ------------------------------------------------------------
#                      PROTECTED METHODS
# ------------------------------------------------------------
::itcl::body BrepEditFrame::initCVEdit {} {
    set mEditCommand CVEditMode
    set mEditClass $EDIT_CLASS_TRANS
    set mEditLastTransMode $::ArcherCore::OBJECT_TRANSLATE_MODE
    GeometryEditFrame::initEditState
}

::itcl::body BrepEditFrame::buildUpperPanel {} {
}

::itcl::body BrepEditFrame::buildLowerPanel {} {
    set parent [$this childsite lower]

    itk_component add editCV {
	::ttk::checkbutton $parent.editCV \
	    -text "Edit Control Vertices" \
	    -variable [::itcl::scope mEditMode] \
	    -command [::itcl::code $this toggleCVEdit]
    } {}

    pack $itk_component(editCV)
}

# ------------------------------------------------------------
#                      PRIVATE METHODS
# ------------------------------------------------------------
::itcl::body BrepEditFrame::toggleCVEdit {} {
    if {$mEditMode} {
	initCVEdit
    } else {
	clearEditState 0 1
    }
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
