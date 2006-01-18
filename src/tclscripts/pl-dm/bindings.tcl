#                    B I N D I N G S . T C L
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
proc sampler_bind_dm { name } {
bind $name <1> "zoom 0.5"
bind $name <2> "dm m %b 1 %x %y"
bind $name <3> "zoom 2.0"

bind $name <Control-ButtonRelease-1> "dm am t 0 %x %y"
bind $name <Control-ButtonPress-1> "dm am t 1 %x %y"
bind $name <Control-ButtonRelease-2> "dm am r 0 %x %y"
bind $name <Control-ButtonPress-2> "dm am r 1 %x %y"
bind $name <Control-ButtonRelease-3> "dm am z 0 %x %y"
bind $name <Control-ButtonPress-3> "dm am z 1 %x %y"
bind $name <Button1-KeyRelease-Control_L> "dm am t 0 %x %y"
bind $name <Button1-KeyRelease-Control_R> "dm am t 0 %x %y"
bind $name <Button2-KeyRelease-Control_L> "dm am r 0 %x %y"
bind $name <Button2-KeyRelease-Control_R> "dm am r 0 %x %y"
bind $name <Button3-KeyRelease-Control_L> "dm am z 0 %x %y"
bind $name <Button3-KeyRelease-Control_R> "dm am z 0 %x %y"

bind $name f "ae 0 0"
bind $name l "ae 90 0"
bind $name R "ae 180 0"
bind $name r "ae 270 0"
bind $name "ae -90 90"
bind $name "ae -90 -90"
bind $name "ae 35 25"
bind $name "ae 45 45"

bind $name <F1> "dm set depthcue !"
bind $name <F2> "dm set zclip !"
bind $name <F4> "dm set zbuffer !"
bind $name <F5> "dm set lighting !"

#bind $name <Left> "ae -i -1 0 0"
#bind $name <Right> "ae -i 1 0 0"
#bind $name <Down> "ae -i 0 -1 0"
#bind $name <Up> "ae -i 0 1 0"
}


# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
