#                    X C L O N E . T C L
# BRL-CAD
#
# Copyright (c) 2008-2011 United States Government as represented by
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
#       This clones an object and xpushes the clone.
#
# Note -
#       At the moment, clone does not work properly for some of it operations
#       when there are matrices hanging above a combinations members. To get
#       around this the object is cloned without transforms and xpushed. The
#       result of this is clone again, if necessary, applying any specfied
#       transformations.
#

proc xclone {args} {
    # Begin hack section
    set remaining [lrange $args 0 end-1]
    set last [lindex $args end]

    if {[catch {clone $last} iclone]} {
	return $iclone
    }

    if {[catch {xpush $iclone} msg]} {
	killtree $iclone
	return $msg
    }

    if {[llength $remaining] < 1} {
	return $iclone
    }

    set args [lreplace $args end end $iclone]
    catch {eval clone $args} clone
    killtree $iclone
    # End hack section


    # Use the following commented-out lines when clone gets fixed
    #if {[catch {eval clone $args} clone]} {
    #	return $clone
    #}
    #
    #catch {xpush $clone} msg
    #puts $msg

    return $clone
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
