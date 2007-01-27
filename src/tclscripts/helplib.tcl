#                     H E L P L I B . T C L
# BRL-CAD
#
# Copyright (c) 2004-2007 United States Government as represented by
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
# Help facility for Tclized library routines.
#
#==============================================================================

set helplib_data(dm_bg)			{{["r g b"]} {Get/set background color}}
set helplib_data(dm_bounds)		{{["xmin xmax ymin ymax zmin zmax"]} {Set/get window bounds}}
set helplib_data(dm_close)		{{} {close/destroy this display manager object}}
set helplib_data(dm_open)		{{[name type [args]]} {Open/create a display manager object}}
set helplib_data(dm_configure)		{{} {configure window parameters}}
set helplib_data(dm_debug)		{{[level]} {Set/get debug level}}
set helplib_data(dm_loadmat)		{{mat} {load viewing matrix}}
set helplib_data(dm_drawString)		{{str x y size use_aspect} {draw string at (x,y)}}
set helplib_data(dm_drawPoint)		{{x y} {draw point at (x,y)}}
set helplib_data(dm_drawLine)		{{x1 y1 x2 y2} {draw line from (x1,y1) to (x2,y2)}}
set helplib_data(dm_drawVList)		{{vlp} {draw vlist represented by the given vlist pointer string}}
set helplib_data(dm_drawSList)		{{slp} {draw solid list represented by the given solid list pointer string}}
set helplib_data(dm_drawModelAxes)	{{vsize rmat apos asize acolor lcolor lw v2m_mat doticks tlen ti tcolor} {draw model axes with labels}}
set helplib_data(dm_drawViewAxes)	{{vsize rmat apos asize acolor lcolor lw} {draw view axes with labels}}
set helplib_data(dm_drawCenterDot)	{{color} {draw center dot using specified color}}
set helplib_data(dm_drawGeom)		{{obj(s)} {draw the specified geometry object(s)}}
set helplib_data(dm_fg)			{{["r g b"]} {Get/set foreground color}}
set helplib_data(dm_linewidth)		{{[width]} {Set/get line width}}
set helplib_data(dm_linestyle)		{{[0|1]} {Set/get line style}}
set helplib_data(dm_zclip)		{{[flag]} {Set/get zclip flag}}
set helplib_data(dm_zbuffer)		{{[flag]} {Set/get zbuffer flag}}
set helplib_data(dm_light)		{{[flag]} {Set/get light flag}}
set helplib_data(dm_transparency)	{{[val]} {Set/get transparency value}}
set helplib_data(dm_depthMask)		{{[flag]} {Set/get depth mask flag}}
set helplib_data(dm_perspective)	{{[flag]} {Set/get perspective flag}}
set helplib_data(dm_png)		{{file} {Dump contents of window to a png file}}
set helplib_data(dm_listen)		{{[port]} {Set/get the port used to listen for framebuffer clients}}
set helplib_data(dm_size)		{{[width [height]]} {Set/get the window size}}
set helplib_data(dm_getaspect)		{{} {Get window's aspect ratio }}
set helplib_data(dm_observer)		{{cmd [args]} {Attach/detach observer to/from list}}
set helplib_data(dm_clearBufferAfter)	{{[flag]} {Get/set the clearBufferAfter flag}}

set helplib_data(wdb_adjust)		{{object attr value ?attr value?} {adjust object's attribute(s)}}
set helplib_data(wdb_attr)        {{ {set|get|show|rm|append} object [args]}
	      {set, get, show, remove or append to attribute values for the specified object.
	 The arguments for "set" and "append" subcommands are attribute name/value pairs.
	 The arguments for "get", "rm", and "show" subcommands are attribute names.
	 The "set" subcommand sets the specified attributes for the object.
	 The "append" subcommand appends the provided value to an existing attribute,
		 or creates a new attribute if it does not already exist.
	 The "get" subcommand retrieves and displays the specified attributes.
	 The "rm" subcommand deletes the specified attributes.
	 The "show" subcommand does a "get" and displays the results in a user readable format.}   }
set helplib_data(wdb_binary)		{{(-i|-o) major_type minor_type dest source}
		{manipulate opaque objects.
		 Must specify one of -i (for creating or adjusting objects (input))
		 or -o for extracting objects (output).
		 If the major type is "u" the minor type must be one of:
		      "f" -> float
		      "d" -> double
		      "c" -> char (8 bit)
		      "s" -> short (16 bit)
		      "i" -> int (32 bit)
		      "l" -> long (64 bit)
		      "C" -> unsigned char (8 bit)
		      "S" -> unsigned short (16 bit)
		      "I" -> unsigned int (32 bit)
		      "L" -> unsigned long (64 bit)
		 For input, source is a file name and dest is an object name.
		 For output source is an object name and dest is a file name.
		 Only uniform array binary objects (major_type=u) are currently supported}}
set helplib_data(wdb_bot_face_sort)     {{triangles_per_piece bot_solid1 [bot_solid2 bot_solid3 ...]} {sort the facelist of BOT solids to optimize ray trace performance for a particular number of triangles per raytrace piece }}
set helplib_data(wdb_bot_decimate)      {{ -c maximum_chord_error -n maximum_normal_error -e minimum_edge_length new_bot_name current_bot_name} {Uses edge decimation to reduce the number of triangles in the specified BOT while keeping within the specified constraints}}
set helplib_data(wdb_cat)		{{<objects>} {list attributes (brief)}}
set helplib_data(wdb_color)		{{low high r g b} {make color entry}}
set helplib_data(wdb_comb)		{{comb_name <operation solid>}	{create or extend combination w/booleans}}
set helplib_data(wdb_comb_std)		{{[-cr] comb_name <boolean_expr>}	{create or extend a combination using standard notation}}
set helplib_data(wdb_concat)		{{[-s|-p] file.g [prefix]} {concatenate another GED file into the current database}}
set helplib_data(wdb_copy)		{{from to} {copy a database object}}
set helplib_data(wdb_copyeval)		{{new_solid path_to_old_solid}	{copy an 'evaluated' path solid}}
set helplib_data(wdb_dbip)		{{} {get dbip}}
set helplib_data(wdb_dump)		{{file.g} {dump a full copy of the database into file.g}}
set helplib_data(wdb_dup)		{{file.g prefix} {check for duplicate names in file}}
set helplib_data(wdb_expand)		{{expression}	{globs expression against database objects}}
set helplib_data(wdb_find)		{{<objects>} {find combinations that reference objects}}
set helplib_data(wdb_form)		{{type} {returns form for objects of type "type"}}
set helplib_data(wdb_get)		{{object ?attr?} {get object attributes}}
set helplib_data(wdb_get_type)		{{object} {get the object's type}}
set helplib_data(wdb_group)		{{gname object(s)} {create or append object(s) to a group}}
set helplib_data(wdb_hide)              {{<objects>} {set the "hidden" flag for the specified objects so they do not appear in an "ls" command output}}
set helplib_data(wdb_instance)		{{obj comb [op]} {add instance of obj to comb}}
set helplib_data(wdb_keep)		{{file object(s)} {save named objects in the specified file}}
set helplib_data(wdb_kill)		{{<objects>} {kill/delete database objects}}
set helplib_data(wdb_killall)		{{<objects>} {kill/delete database objects, removing all references}}
set helplib_data(wdb_killtree)		{{<objects>} {kill all paths belonging to objects}}
set helplib_data(wdb_list)		{{[-r] <objects>} {list object information, verbose}}
set helplib_data(wdb_listeval)		{{}	{lists 'evaluated' path solids}}
set helplib_data(wdb_ls)		{{[-A -o -a -c -r -s -p -l] [object(s) | name/value pairs]} {list objects in this database}}
set helplib_data(wdb_lt)		{{object} {list object's tree as a tcl list of {operator object} pairs}}
set helplib_data(wdb_make_bb)		{{bbname object(s)} {make a bounding box (rpp) around the specified objects}}
set helplib_data(wdb_make_name)		{{template | -s [num]}	{make an object name not occuring in the database}}
set helplib_data(wdb_match)		{{expression}	{globs expression against database objects, does not return tokens that match nothing}}
set helplib_data(wdb_move_arb_edge)	{{arb edge pt} {move an arb's edge through pt}}
set helplib_data(wdb_move_arb_face)	{{arb face pt} {move an arb's face through pt}}
set helplib_data(wdb_rotate_arb_face)	{{arb face pt} {rotate an arb's face through pt}}
set helplib_data(wdb_move)		{{from to} {rename a database object}}
set helplib_data(wdb_moveall)		{{from to} {rename all occurences of object}}
set helplib_data(wdb_nmg_collapse)	{{nmg_solid new_solid maximum_error_distance [minimum_allowed_angle]}	{decimate NMG solid via edge collapse}}
set helplib_data(wdb_nmg_simplify)	{{[arb|tgc|ell|poly] new_solid nmg_solid}	{simplify nmg_solid, if possible}}
set helplib_data(wdb_observer)		{{cmd [args]} {Attach/detach observer to/from list}}
set helplib_data(wdb_pathlist)		{{name(s)}	{list all paths from name(s) to leaves}}
set helplib_data(wdb_paths)		{{pattern}	{lists all paths matching input path}}
set helplib_data(wdb_prcolor)		{{} {print color table}}
set helplib_data(wdb_push)		{{object(s)} {push object(s) path transformations to solids}}
set helplib_data(wdb_put)		{{object type attrs} {create a database object}}
set helplib_data(wdb_region)		{{object(s)} {create or append objects to a region}}
set helplib_data(wdb_remove)		{{comb object(s)} {remove members from a combination}}
set helplib_data(wdb_reopen)		{{[filename]} {open a database}}
set helplib_data(wdb_rt_gettrees)	{{procname [-i] [-u] treetops...} {create an rt instance object}}
set helplib_data(wdb_shells)		{{nmg_model}	{breaks model into seperate shells}}
set helplib_data(wdb_showmats)		{{path}	{show xform matrices along path}}
set helplib_data(wdb_bot_smooth)        {{[-t norm_tolerance_degrees] new_bot_name old_bot_name} {calculate vertex normals for BOT primitive}}
set helplib_data(wdb_summary)		{{[p r g]}	{count/list primitives/regions/groups}}
set helplib_data(wdb_title)		{{description} {Set/get database title}}
set helplib_data(wdb_tol)		{{[abs|rel|norm|dist|perp [#]} {Set/get tessellation and calculation tolerances}}
set helplib_data(wdb_tops)		{{[-n] [-u] [-g]}	{find all top level objects}}
set helplib_data(wdb_track)		{{args} {create a track}}
set helplib_data(wdb_tree)		{{[-c] [-o outfile] [-i indentSize] object(s)} {print out a tree of all members of an object}}
set helplib_data(wdb_unhide)            {{[objects]} {unset the "hidden" flag for the specified objects so they will appear in a "t" or "ls" command output}}
set helplib_data(wdb_units)		{{[mm|cm|m|in|ft|...]}	{change units}}
set helplib_data(wdb_version)		{{} {returns the database version}}
set helplib_data(wdb_whatid)		{{region} {return the specified region's id}}
set helplib_data(wdb_whichair)		{{code(s)} {find regions with the specified air code(s)}}
set helplib_data(wdb_whichid)		{{[-s] id(s)} {find regions with the specified id(s)}}
set helplib_data(wdb_xpush)		{{object} {push object path transformations to solids, creating solids if necessary}}

set helplib_data(vo_aet)		{{[-i] ["az el tw"]} {set/get the azimuth, elevation and twist}}
set helplib_data(vo_arot)		{{x y z angle} {rotate angle degrees about the axis specified by xyz}}
set helplib_data(vo_base2local)		{{} {get base2local conversion factor}}
set helplib_data(vo_center)		{{["x y z"]} {set/get the view center}}
set helplib_data(vo_coord)		{{[m|v]} {set/get the coodinate system}}
set helplib_data(vo_eye)		{{"x y z"} {set the eyepoint}}
set helplib_data(vo_eye_pos)		{{"x y z"} {set the eye position}}
set helplib_data(vo_invSize)		{{} {get the inverse view size}}
set helplib_data(vo_keypoint)		{{[point]} {set/get the keypoint}}
set helplib_data(vo_local2base)		{{} {get local2base conversion factor}}
set helplib_data(vo_lookat)		{{"x y z"} {set the look-at point}}
set helplib_data(vo_model2view)		{{} {get the model2view matrix}}
set helplib_data(vo_mrot)		{{x y z} {rotate view using model x,y,z}}
set helplib_data(vo_observer)		{{cmd [args]} {Attach/detach observer to/from list}}
set helplib_data(vo_orient)		{{quat} {set the orientation from quaternion}}
set helplib_data(vo_perspective)	{{[angle]} {set/get the perspective angle}}
set helplib_data(vo_pmat)		{{[mat]} {set/get the perspective matrix}}
set helplib_data(vo_pmodel2view)	{{} {get the pmodel2view matrix}}
set helplib_data(vo_pov)		{{center quat scale eye_pos perspective} {set point of view}}
set helplib_data(vo_rmat)		{{[mat]} {set/get the rotation matrix}}
set helplib_data(vo_rot)		{{"x y z"} {rotate the view}}
set helplib_data(vo_rotate_about)	{{[e|k|m|v]} {set/get the rotate about point}}
set helplib_data(vo_sca)		{{sfactor} {scale by sfactor}}
set helplib_data(vo_setview)		{{x y z} {set the view given angles x, y, and z in degrees}}
set helplib_data(vo_size)		{{vsize} {set/get the view size}}
set helplib_data(vo_slew)		{{x y [z]} {move view center}}
set helplib_data(vo_tra)		{{dx dy dz} {translate by (dx,dy,dz)}}
set helplib_data(vo_units)		{{unit_spec} {set/get units}}
set helplib_data(vo_view2model)		{{} {get the view2model matrix}}
set helplib_data(vo_viewDir)		{{[-i]} {return the view direction}}
set helplib_data(vo_vrot)		{{xdeg ydeg zdeg} {rotate viewpoint}}
set helplib_data(vo_zoom)		{{sf} {zoom view by specified scale factor}}

set helplib_data(dgo_assoc)		{{[wdb_obj]} {set/get the associated database object}}
set helplib_data(dgo_autoview)		{{view_obj} {calculate an appropriate view size and center for view_obj}}
set helplib_data(dgo_blast)		{{[-A -o -C#/#/# -s] <object(s) | attribute name/value pairs>} {erase all currently displayed geometry and draw the specified object(s)}}
set helplib_data(dgo_clear)		{{} {erase all objects from the display}}
set helplib_data(dgo_draw)		{{[-A -o -C#/#/# -s] <objects | attribute name/value pairs>} {prepare object(s) for display}}
set helplib_data(dgo_E)			{{[-s] <objects>} {evaluated display of objects}}
set helplib_data(dgo_erase)		{{<objects>} {erase objects from the display}}
set helplib_data(dgo_erase_all)		{{<objects>} {erase all occurrences of objects from the display}}
set helplib_data(dgo_ev)		{{[-dfnstuvwT] [-P #] <objects>}	{evaluate objects via NMG tessellation}}
set helplib_data(dgo_get_autoview)	{{}	{get view size and center such that all displayed solids would be in view}}
set helplib_data(dgo_headSolid)		{{} {return pointer to solid list}}
set helplib_data(dgo_how)		{{[-b] obj}	{returns how an object is being displayed}}
set helplib_data(dgo_illum)		{{[-n] obj} {illuminate/highlight obj}}
set helplib_data(dgo_nirt)		{{[nirt(1) options] [x y z]}	{trace a single ray from current view}}
set helplib_data(dgo_observer)		{{cmd [args]} {Attach/detach observer to/from list}}
set helplib_data(dgo_open)		{{name wdb_obj} {open/create a new drawable geometry object}}
set helplib_data(dgo_overlay)		{{file.pl [name]}	{overlay the specified 2D/3D UNIX plot file}}
set helplib_data(dgo_qray)		{{subcommand}	{get/set query_ray characteristics}}
set helplib_data(dgo_report)		{{[lvl]} {print solid table & vector list}}
set helplib_data(dgo_rt)		{{[options] [-- objects]}	{do raytrace of view or specified objects}}
set helplib_data(dgo_rtabort)		{{} {abort the associated raytraces}}
set helplib_data(dgo_rtcheck)		{{[options]}	{check for overlaps in current view}}
set helplib_data(dgo_rtarea)		{{[options] [-- objects]}	{report the exposed and presented areas in the current view}}
set helplib_data(dgo_rtedge)		{{[options] [-- objects]}	{do edge rendering of view or specified objects}}
set helplib_data(dgo_rtweight)		{{[options] [-- objects]}	{report the approximate weight and centroid of displayed geometry}}
set helplib_data(dgo_set_outputHandler)	{{[script]}	{get/set output handler script}}
set helplib_data(dgo_set_plOutputMode)	{{[binary|text]}	{get/set the plot output mode}}
set helplib_data(dgo_set_transparency)	{{obj transparency}	{set transparency of the specified object}}
set helplib_data(dgo_shaded_mode)	{{[0|1|2]}	{get/set shaded mode}}
set helplib_data(dgo_vdraw)		{{write|insert|delete|read|send|params|open|vlist [args]}	{Expermental drawing (cnuzman)}}
set helplib_data(dgo_vnirt)		{{[vnirt(1) options] viewX viewY}	{trace a single ray from current view}}
set helplib_data(dgo_who)		{{[r(eal)|p(hony)|b(oth)]}	{list the top-level objects currently being displayed}}
set helplib_data(dgo_zap)		{{} {erase all objects from the display}}

set helplib_data(cho_close)		{{} {close/destroy this command history object}}
set helplib_data(cho_open)		{{name} {open/create a new command history object}}

set helplib_data(dm_best_type)          {{} {return the best available display manager type}}
set helplib_data(dm_best_name)          {{} {return the best available display manager name}}
set helplib_data(dm_name2type)          {{name} {return the display manager type that corresponds to name}}
set helplib_data(dm_names)              {{} {return a list of available display manager names}}
set helplib_data(dm_type2name)          {{type} {return the display manager name that corresponds to type}}
set helplib_data(dm_types)              {{} {return a list of available display manager types}}
set helplib_data(wdb_open)		{{widget_command file filename}	{}}
set helplib_data(rt_wdb_inmem_rgb)	{{$wdbp comb r g b}	{}}
set helplib_data(rt_wdb_inmem_shader)	{{$wdbp comb shader [params]}	{}}
set helplib_data(mat_mul)		{{matA matB}	{multiply matrix matA by matB}}
set helplib_data(mat_inv)		{{mat}	{invert a 4x4 matrix}}
set helplib_data(mat_trn)		{{mat}	{transpose a 4x4 matrix}}
set helplib_data(mat_ae)		{{azimuth elevation}	{compute a 4x4 rotation matrix given azimuth and elevation}}
set helplib_data(mat_ae_vec)		{{vect}	{find azimuth and elevation angles that correspond to the
	direction given by vect}}
set helplib_data(mat_aet_vec)		{{vec_ae vec_twist}	{find azimuth, elevation and twist angles}}
set helplib_data(mat_angles)		{{alpha beta gamma}	{build a homogeneous rotation matrix given 3 angles of rotation}}
set helplib_data(mat_eigen2x2)		{{a b c}	{find eigenvalues and eigenvectors of a symmetric 2x2 matrix}}
set helplib_data(mat_fromto)		{{vecFrom vecTo}	{compute a rotation matrix that will transform space by the
	angle between the two given vectors}}
set helplib_data(mat_xrot)		{{sinAngle cosAngle}	{find the rotation matrix given the sin and cos of the X rotation angle}}
set helplib_data(mat_yrot)		{{sinAngle cosAngle}	{find the rotation matrix given the sin and cos of the Y rotation angle}}
set helplib_data(mat_zrot)		{{sinAngle cosAngle}	{find the rotation matrix given the sin and cos of the Z rotation angle}}
set helplib_data(mat_lookat)		{{dir yflip}	{compute a matrix which rotates vector dir onto the -Z axis}}
set helplib_data(mat_vec_ortho)		{{vec}	{find a vector which is perpendicular to vec and with unit length}}
set helplib_data(mat_vec_perp)		{{vec}	{find a vector which is perpendicular to vec and with unit length
	if vec was of unit length}}
set helplib_data(mat_scale_about_pt)	{{pt scale}	{build a matrix to scale uniformly around a given point}}
set helplib_data(mat_xform_about_pt)	{{xform pt}	{build a matrix to apply an arbitrary 4x4 transformation around
	a given point}}
set helplib_data(mat_arb_rot)		{{pt dir angle}	{build a matrix to rotate about an arbitrary axis}}
set helplib_data(quat_mat2quat)		{{mat}	{convert matrix to quaternion}}
set helplib_data(quat_quat2mat)		{{quat}	{convert quaternion to matrix}}
set helplib_data(quat_distance)		{{quatA quatB}	{finds the euclidean distance between two quaternions}}
set helplib_data(quat_double)		{{quatA quatB}	{finds the quaternion point representing twice the rotation from
	quatA to quatB}}
set helplib_data(quat_bisect)		{{quatA quatB}	{finds the bisector of quaternions quatA and quatB}}
set helplib_data(quat_sberp)		{{quat1 quat2 quat3 quat4 factor}	{do sperical bezier interpolation between four quaternions
	by the given factor}}
set helplib_data(quat_slerp)		{{quat1 quat2 factor}	{do spherical linear interpolation between two unit quaternions
	by the given factor}}
set helplib_data(quat_make_nearest)	{{quatA quatB}	{set quaternion quatA to the quaternion which yields the smallest
	rotation from quatB}}
set helplib_data(quat_exp)		{{quat}	{exponentiate a quaternion}}
set helplib_data(quat_log)		{{quat}	{take the natural logarithm of a unit quaternion}}

proc helplib {args} {
	global helplib_data

	if {[llength $args] > 0} {
		return [help_comm helplib_data $args]
	} else {
		return [help_comm helplib_data]
	}
}

proc ?lib {} {
	global helplib_data

	return [?_comm helplib_data 20 4]
}


proc aproposlib {key} {
	global helplib_data

	return [apropos_comm helplib_data $key]
}

## - helplib_alias
#
# This routine replaces the command name in the string
# returned by helplib with the alias. Each of the
# tcl_object's methods in the libraries should use
# this instead of helplib. That way applications and/or
# widgets that use these methods can get the same
# help using their respective names.
#
proc helplib_alias {libcmd alias} {
    set info [helplib $libcmd]

    set pattern "Usage: $libcmd"
    regsub $pattern $info "Usage: $alias" info

    return $info
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
