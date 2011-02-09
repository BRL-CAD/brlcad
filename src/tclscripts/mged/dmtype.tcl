#
#                        D M T Y P E . T C L
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
#       Without arguments, reports current display manager type.  With the
#   "set" argument, if display manager type supplied is different from
#   current type, destroy old display manager and start up new ones of the
#   new type.
#
# Usage:
#       dmtype
#       dmtype set {X, ogl, rtgl, ...}
#
proc dmtype {args} {
  global mged_gui
  global mged_default
  global mged_players
  global mged_display
  set id $mged_players
  
  set argc [llength $args]
  if {$argc == 0} {
    return $mged_gui($id,dtype)
  }
  if {$argc == 1} {
    return [help dmtype]
  }
  if {$argc > 2} {
    return [help dmtype]
  }
  set arg [lindex $args 0]
  if {$arg != "set"} {
    return [help dmtype]
  }
  set dtype [lindex $args 1]

  set oldaet [_mged_ae] 
 
  # New dm type is requested
  catch { release $mged_gui($id,top).ur }
  catch { release $mged_gui($id,top).ul }
  catch { release $mged_gui($id,top).lr }
  catch { release $mged_gui($id,top).ll }
  destroy $mged_gui($id,top).ulF 
  destroy $mged_gui($id,top).urF
  destroy $mged_gui($id,top).llF
  destroy $mged_gui($id,top).lrF

  openmv $id $mged_gui($id,top) $mged_gui($id,dmc) $mged_default(display) $dtype
 
  grid $mged_gui($id,dmc).$mged_gui($id,dm_loc)\F -in $mged_gui($id,dmc) -sticky "nsew" -row 0 -column 0

  set mged_gui($id,dtype) $dtype

  mged_apply_local $id "rset cs mode 0"
  rset cs mode 1

  ae [lindex $oldaet 0] [lindex $oldaet 1] [lindex $oldaet 2]
}


# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
