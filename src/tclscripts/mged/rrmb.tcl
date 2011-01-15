#
#                        R R M B . T C L
# BRL-CAD
#
# Copyright (c) 2010-2011 United States Government as represented by
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
# Description -
#
# Replace Region Members with Bot
#
# This proc looks for bots created by the facetall.sh script
# and substitutes them in as the sole members of the respective
# regions that were used to create them.
#

set glob_compat_mode 0

proc rrmb {{_unclaimed unclaimed}} {
    foreach bot [expand *.bot] {
	regexp {(.*)\.bot} $bot match rname

	if {[catch {get $rname id} rid]} {
	    set rid ""
	}

	if {$rid != ""} {
	    # Replace tree with a bot leaf
	    adjust $rname tree [list l $bot]
	} else {
	    catch {
		r $rname u $bot
		g $_unclaimed $rname
	    }
	}
    }
}
