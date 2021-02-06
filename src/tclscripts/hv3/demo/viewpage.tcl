#!/usr/brlcad/bin/bwish
#                    V I E W P A G E . T C L
# BRL-CAD
#
# Copyright (c) 2010-2021 United States Government as represented by
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
package require hv3

#source snit.tcl
#source hv3.tcl

proc mkTkImage {file} {
     set name [image create photo -file $file]
     return [list $name [list image delete $name]]
}

set fd [open index.html]
set data [read $fd]
close $fd

set myHv3 [::hv3::hv3 .hv3]
#$myHv3 Subscribe motion [list .hv3 motion]
bind .hv3 <3> [list .hv3 rightclick %x %y %X %Y]
bind .hv3 <2> [list .hv3 middleclick %x %y]
set html [$myHv3 html]
$html configure -parsemode html
$html configure -imagecmd mkTkImage
$html parse $data
pack .hv3 -side left -expand yes -fill y

#scrollbar .helpviewer_yscroll -command {.helpviewer yview} -orient vertical
#html .helpviewer -yscrollcommand {.helpviewer_yscroll set}
#grid .helpviewer .helpviewer_yscroll -sticky news
#grid rowconfigure . 0 -weight 1
#grid columnconfigure . 0 -weight 1
#.helpviewer configure -parsemode html
#.helpviewer configure -imagecmd mkTkImage
#.helpviewer parse $data
