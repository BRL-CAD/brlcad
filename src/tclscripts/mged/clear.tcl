#
#                        C L E A R . T C L
# BRL-CAD
#
# Copyright (c) 2009-2011 United States Government as represented by
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
#       This clears MGED's command window.
#
# Usage:
#       clear
#
proc clear {} {
  set clearid [cmd_win get]
  if {$clearid == "" || $clearid == "mged"} {
    puts [exec clear]
  } else { 
    .$clearid.t delete 1.0 end
    .$clearid.t insert insert " "
    beginning_of_line .$clearid.t
    .$clearid.t  edit reset
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
