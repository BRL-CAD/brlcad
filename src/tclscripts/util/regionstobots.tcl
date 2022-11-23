#                 R E G I O N S T O B O T S . T C L
# BRL-CAD
#
# Copyright (c) 2004-2022 United States Government as represented by
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

proc regions_to_bots {} {
    set old_globcompat $::glob_compat_mode
    set glob_compat_mode 0
    set regions [search -type region]
    foreach region $regions {
	puts "facetizing $region"
	if { [catch {facetize $region.bot $region} ] } {
	  puts "facetizing $region failed... skipping"
        }
	set have_bot [exists $region.bot]
	if { $have_bot } {
	    puts "facetized $region to $region.bot"
	    kill $region
	    g $region $region.bot
        }
    }
    set ::glob_compat_mode $old_globcompat
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
