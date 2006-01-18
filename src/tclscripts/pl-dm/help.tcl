#                        H E L P . T C L
# BRL-CAD
#
# Copyright (c) 2004-2006 United States Government as represented by
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
#==============================================================================
#
# Help facility for pl-dm
#
#==============================================================================

set help_data(?)	{{} {summary of available commands}}
set help_data(ae)	{{[-i] azim elev [twist]} {set view using azim, elev and twist angles}}
set help_data(clear)	{{} {clear screen}}
set help_data(closepl)	{{plot file(s)} {close one or more plot files}}
set help_data(dm)	{{set var [val]} {Do display-manager specific command}}
set help_data(draw)	{{object(s)} {draw object(s)}}
set help_data(erase)	{{object(s)} {erase object(s)}}
set help_data(exit)	{{} {quit}}
set help_data(help)	{{[commands]} {give usage message for given commands}}
set help_data(ls)	{{} {list objects}}
set help_data(openpl)	{{plotfile(s)} {open one or more plot files}}
set help_data(q)	{{} {quit}}
set help_data(reset)    {{reset} {reset view}}
set help_data(sv)	{{x y [z]} {Move view center to (x, y, z)}}
set help_data(t)	{{} {list object(s)}}
set help_data(vrot)	{{xdeg ydeg zdeg} {rotate viewpoint}}
set help_data(zoom)	{{scale_factor} {zoom view in or out}}

proc help {args} {
	global help_data

        if {[llength $args] > 0} {
                return [help_comm help_data $args]
        } else {
                return [help_comm help_data]
        }
}

proc ? {} {
        global help_data

        return [?_comm help_data 20 4]
}

proc apropos key {
        global help_data

        return [apropos_comm help_data $key]
}


# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
