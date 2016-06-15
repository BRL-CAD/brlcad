#             B O T _ V E R T E X _ F U S E _ A L L . T C L
# BRL-CAD
#
# Copyright (c) 2005-2014 United States Government as represented by
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

set fused_msg ""
set bot_global ""
set tmp_name ""

proc bot_vertex_fuse_all {} {

    global fused_msg
    global tmp_name

    set bot_list {}
    set bot_list [search -type bot]
    set cnt [llength $bot_list]

    foreach bot $bot_list {
	uplevel #0 set bot_global $bot
	set found_tmp 0
	while { !$found_tmp } {
	    uplevel #0 set tmp_name [expr rand()]
	    if { [search -name $tmp_name] == "" } {
		set found_tmp 1
	    }
	}
	set bot_desc [l -t $bot]
	set bot_vert_cnt [lindex $bot_desc 5]

	incr cnt -1

	puts stdout "$cnt remaining bots; processing $bot_vert_cnt vertices in bot \"$bot\" ..."

	after 1 {set fused_msg [bot_vertex_fuse $tmp_name $bot_global]}

	vwait fused_msg

	set fused_num [lindex $fused_msg 1]

	if { $fused_num == 0 } {
	    kill $tmp_name
	} else {
	    kill $bot
	    mv $tmp_name $bot
	}

	puts stdout "$fused_num vertex fused.\n"
    }
    puts stdout "Done."
}
