#                A T T R G R O U P S D I S P L A Y U T I L I T Y P . T C L
# BRL-CAD
#
# Copyright (c) 2002-2011 United States Government as represented by
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
#
# Description:
#	 This is an Archer plugin for the attribute groups display utility.
#

::itcl::class AttrGroupsDisplayUtilityP {
    inherit AttrGroupsDisplayUtility

    constructor {_archer args} {
	eval AttrGroupsDisplayUtility::constructor $_archer 1
    } {}
    destructor {}

    public {
	# Override's for the AttrGroupsDisplayUtility class
	common utilityClass AttrGroupsDisplayUtilityP
    }

    protected {
    }

    private {
    }
}

## - constructor
#
#
#
::itcl::body AttrGroupsDisplayUtilityP::constructor {_archer args} {
    # process options
    eval itk_initialize $args

    after idle [::itcl::code $this updateBgColor $mParentBg]
}

::itcl::body AttrGroupsDisplayUtilityP::destructor {} {
    # nothing for now
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
