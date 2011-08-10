#                   H E L P D E V E L . T C L
# BRL-CAD
#
# Copyright (c) 2004-2011 United States Government as represented by
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
#==============================================================================
#
# TCL versions of MGED "helpdevel", "?devel", and "aproposdevel" commands
#
#==============================================================================

set mged_helpdevel_data(aip)		{{[fb]}	{advance illumination pointer or path position forward or backward}}
set mged_helpdevel_data(cmd_win)	{{cmd}	{routine for maintaining command windows}}
set mged_helpdevel_data(collaborate)	{{join id | quit id | show}	{routine for maintaining the collaborative session}}
set mged_helpdevel_data(get_comb)	{{combination}	{get information about combination}}
set mged_helpdevel_data(get_dm_list)	{{}	{returns a list of all display managers}}
set mged_helpdevel_data(get_autoview)	{{}	{get view size and center such that all displayed solids would be in view}}
set mged_helpdevel_data(get_regions)	{{combination}	{returns the names of all regions at or under a given combination}}
set mged_helpdevel_data(get_sed)	{{[-c]}	{get the solid parameters for the solid currently
    being edited}}
set mged_helpdevel_data(get_solid_keypoint) {{} {set the solid keypoint using the default rules}}
set mged_helpdevel_data(get_more_default)	{{}	{get the current default input value}}
set mged_helpdevel_data(grid2model_lu)	{{gx gy}	{given a point in grid coordinates (local units),
    convert it to model coordinates (local units).}}
set mged_helpdevel_data(grid2view_lu)	{{gx gy}	{given a point in grid coordinates (local units),
    convert it to view coordinates (local units).}}
set mged_helpdevel_data(gui_destroy)	{{id}	{destroy display/command window pair}}
set mged_helpdevel_data(hist)		{{command}	{routine for maintaining command history}}
set mged_helpdevel_data(make_name)	{{template | -s [num]}	{make an object name not occuring in the database}}
set mged_helpdevel_data(mged_update)	{{non_blocking}	{handle outstanding events and refresh}}
set mged_helpdevel_data(mged_wait)	{{}	{see tkwait}}
set mged_helpdevel_data(mmenu_get)	{{[index]}	{get menu corresponding to index}}
set mged_helpdevel_data(mmenu_set)	{{w id i menu}	{this Tcl proc is used to set/install menu "i"}}
set mged_helpdevel_data(model2grid_lu)	{{mx my mz}	{convert point in model coords (local units)
    to grid coords (local units)}}
set mged_helpdevel_data(model2view)	{{mx my mz}	{convert point in model coords (mm) to view coords}}
set mged_helpdevel_data(model2view_lu)	{{mx my mz}	{convert point in model coords (local units) to view coords (local units)}}
set mged_helpdevel_data(oed_reset)	{{}	{reset the parameters for the currently edited matrix}}
set mged_helpdevel_data(output_hook)	{{[hook_cmd]}	{set up to have output from bu_log sent to hook_cmd}}
set mged_helpdevel_data(put_comb)	{{comb_name is_Region id air material los color shader inherit boolean_expr} {set combination}}
set mged_helpdevel_data(put_sed)	{{solid parameters}	{put the solid parameters into the in-memory (i.e. es_int)
    solid currently being edited.}}
set mged_helpdevel_data(sed_reset)	{{}	{reset the parameters for the currently edited solid (i.e. es_int)}}
set mged_helpdevel_data(rset)		{{[res_type [res [vals]]]}	{provides a mechanism to get/set resource values.}}
set mged_helpdevel_data(set_more_default)	{{more_default}	{set the current default input value}}
set mged_helpdevel_data(share)		{{[-u] resource dm1 [dm2]}	{Provides a mechanism to (un)share resources among display managers.}}
set mged_helpdevel_data(solids_on_ray)	{{h v}	{list all displayed solids along a ray}}
set mged_helpdevel_data(stuff_str)	{{string}	{sends a string to stdout while leaving the current command-line alone}}
set mged_helpdevel_data(svb)		{{}	{set view reference base}}
set mged_helpdevel_data(tie)		{{[[-u] cw [dm]]}	{ties/associates a command window (cw) with a display manager (dm)}}
set mged_helpdevel_data(view_ring)	{{cmd [i]}	{manipulates the view_ring for the current display manager}}
set mged_helpdevel_data(view2grid_lu)	{{vx vy vz}	{given a point in view coordinates (local units),
    convert it to grid coordinates (local units).}}
set mged_helpdevel_data(view2model)	{{vx vy vz}	{given a point in view (screen) space coordinates,
    convert it to model coordinates (in mm).}}
set mged_helpdevel_data(view2model_vec)	{{vx vy vz}	{given a vector in view coordinates,
    convert it to model coordinates.}}
set mged_helpdevel_data(view2model_lu)	{{vx vy vz}	{given a point in view coordinates (local units),
    convert it to model coordinates (local units).}}
set mged_helpdevel_data(winset)		{{[pathname]}	{sets the current display manager to pathname}}

proc helpdevel {args} {
    global mged_helpdevel_data

    if {[llength $args] > 0} {
	return [help_comm mged_helpdevel_data $args]
    } else {
	return [help_comm mged_helpdevel_data]
    }
}

proc ?devel {} {
    global mged_helpdevel_data

    return [?_comm mged_helpdevel_data 20 4]
}

proc aproposdevel key {
    global mged_helpdevel_data

    return [apropos_comm mged_helpdevel_data $key]
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
