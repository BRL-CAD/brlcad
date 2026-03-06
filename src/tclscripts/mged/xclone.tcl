#                    X C L O N E . T C L
# BRL-CAD
#
# Copyright (c) 2008-2025 United States Government as represented by
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
# xclone
#
# Description -
#       This clones an object and xpushes the clone so that any matrices
#       hanging above combination members are absorbed into the primitives,
#       giving a flat hierarchy suitable for further cloning or editing.
#
# Usage -
#       xclone [clone options] object
#
# The --xpush flag is forwarded to libged clone which performs the
# clone-then-xpush sequence atomically and correctly.
#

proc xclone {args} {
    if {[catch {eval clone --xpush $args} result]} {
	return $result
    }
    return $result
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
