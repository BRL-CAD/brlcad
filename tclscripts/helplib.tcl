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
set helplib_data(dm_drawGeom)		{{obj(s)} {draw the specified geometry object(s)}}
set helplib_data(dm_fg)			{{["r g b"]} {Get/set foreground color}}
set helplib_data(dm_linewidth)		{{[width]} {Set/get line width}}
set helplib_data(dm_linestyle)		{{[0|1]} {Set/get line style}}
set helplib_data(dm_zclip)		{{[flag]} {Set/get zclip flag}}
set helplib_data(dm_zbuffer)		{{[flag]} {Set/get zbuffer flag}}
set helplib_data(dm_light)		{{[flag]} {Set/get light flag}}
set helplib_data(dm_perspective)	{{[flag]} {Set/get perspective flag}}
set helplib_data(dm_listen)		{{[port]} {Set/get the port used to listen for framebuffer clients}}
set helplib_data(dm_size)		{{[width [height]]} {Set/get the window size}}
set helplib_data(dm_getaspect)		{{} {Get window's aspect ratio }}
set helplib_data(dm_observer)		{{cmd [args]} {Attach/detach observer to/from list}}
set helplib_data(wdb_close)		{{} {close/destroy this database object}}
set helplib_data(wdb_reopen)		{{[filename]} {open a database}}
set helplib_data(wdb_dbip)		{{} {get dbip}}
set helplib_data(wdb_ls)		{{[args]} {list objects in this database object}}
set helplib_data(wdb_list)		{{[-r] arg(s)} {list object information, verbose}}
set helplib_data(wdb_kill)		{{object(s)} {kill/delete database object(s)}}
set helplib_data(wdb_killall)		{{object(s)} {kill/delete database object(s), removing all references}}
set helplib_data(wdb_killtree)		{{object(s)} {kill all paths belonging to object(s)}}
set helplib_data(wdb_copy)		{{from to} {copy a database object}}
set helplib_data(wdb_move)		{{from to} {rename a database object}}
set helplib_data(wdb_moveall)		{{from to} {rename all occurences of object}}
set helplib_data(wdb_concat)		{{file.g prefix} {concatenate another GED file into the current database}}
set helplib_data(wdb_dup)		{{file.g prefix} {check for duplicate names}}
set helplib_data(wdb_group)		{{gname object(s)} {create or append object(s) to a group}}
set helplib_data(wdb_remove)		{{comb object(s)} {remove members from a combination}}
set helplib_data(wdb_region)		{{object(s)} {create or append objects to a region}}
set helplib_data(wdb_comb)		{{cname op1 s1 op2 s2 ...} {create or append objects to a combination}}
set helplib_data(wdb_find)		{{object(s)} {find combinations that reference object(s)}}
set helplib_data(wdb_whichair)		{{code(s)} {find regions with the specified air code(s)}}
set helplib_data(wdb_whichid)		{{id(s)} {find regions with the specified id(s)}}
set helplib_data(wdb_title)		{{description} {Set/get database title}}
set helplib_data(wdb_tree)		{{object(s)} {print out a tree of all members of an object}}
set helplib_data(wdb_color)		{{low high r g b} {make color entry}}
set helplib_data(wdb_prcolor)		{{} {print color table}}
set helplib_data(wdb_tol)		{{[abs|rel|norm|dist|perp [#]} {Set/get tessellation and calculation tolerances}}
set helplib_data(wdb_push)		{{object(s)} {push object(s) path transformations to solids}}
set helplib_data(wdb_whatid)		{{region} {return the specified region's id}}
set helplib_data(wdb_keep)		{{file object(s)} {save named objects in the specified file}}
set helplib_data(wdb_cat)		{{object(s)} {list attributes (brief)}}
set helplib_data(wdb_instance)		{{obj comb [op]} {add instance of obj to comb}}
set helplib_data(wdb_observer)		{{cmd [args]} {Attach/detach observer to/from list}}
set helplib_data(wdb_make_bb)		{{bbname object(s)} {make a bounding box (rpp) around the specified objects}}
set helplib_data(wdb_make_name)		{{template | -s [num]}	{make an object name not occuring in the database}}
set helplib_data(vo_close)		{{} {close/destroy this view object}}
set helplib_data(vo_size)		{{vsize} {set/get the view size}}
set helplib_data(vo_invSize)		{{} {get the inverse view size}}
set helplib_data(vo_aet)		{{["az el tw"]} {set/get the azimuth, elevation and twist}}
set helplib_data(vo_rmat)		{{[mat]} {set/get the rotation matrix}}
set helplib_data(vo_center)		{{["x y z"]} {set/get the view center}}
set helplib_data(vo_model2view)		{{} {get the model2view matrix}}
set helplib_data(vo_pmodel2view)	{{} {get the pmodel2view matrix}}
set helplib_data(vo_view2model)		{{} {get the view2model matrix}}
set helplib_data(vo_pmat)		{{[mat]} {set/get the perspective matrix}}
set helplib_data(vo_perspective)	{{[angle]} {set/get the perspective angle}}
set helplib_data(vo_eye)		{{"x y z"} {set the eyepoint}}
set helplib_data(vo_eye_pos)		{{"x y z"} {set the eye position}}
set helplib_data(vo_lookat)		{{"x y z"} {set the look-at point}}
set helplib_data(vo_orient)		{{quat} {set the orientation from quaternion}}
set helplib_data(vo_pov)		{{args} {center quat scale eye_pos perspective}}
set helplib_data(vo_zoom)		{{sf} {zoom view by specified scale factor}}
set helplib_data(vo_units)		{{unit_spec} {set/get units}}
set helplib_data(vo_base2local)		{{} {get base2local conversion factor}}
set helplib_data(vo_local2base)		{{} {get local2base conversion factor}}
set helplib_data(vo_rot)		{{"x y z"} {rotate the view}}
set helplib_data(vo_tra)		{{"x y z"} {translate the view}}
set helplib_data(vo_slew)		{{"x y"} {slew the view}}
set helplib_data(vo_observer)		{{cmd [args]} {Attach/detach observer to/from list}}
set helplib_data(vo_coord)		{{[m|v]} {set/get the coodinate system}}
set helplib_data(vo_keypoint)		{{[point]} {set/get the keypoint}}
set helplib_data(vo_rotate_about)	{{[e|k|m|v]} {set/get the rotate about point}}
set helplib_data(dgo_close)		{{} {close/destroy this drawable geometry object}}
set helplib_data(dgo_open)		{{name wdb_obj} {open/create a new drawable geometry object}}
set helplib_data(dgo_headSolid)		{{} {return pointer to solid list}}
set helplib_data(dgo_illum)		{{[-n] obj} {illuminate/highlight obj}}
set helplib_data(dgo_draw)		{{args} {prepare object(s) for display}}
set helplib_data(dgo_erase)		{{object(s)} {erase object(s) from display}}
set helplib_data(dgo_zap)		{{} {erase all objects from the display}}
set helplib_data(dgo_clear)		{{} {erase all objects from the display}}
set helplib_data(dgo_blast)		{{object(s)} {erase all currently displayed geometry and draw the specified object(s)}}
set helplib_data(dgo_rtcheck)		{{view_obj [args]} {}}
set helplib_data(dgo_assoc)		{{[wdb_obj]} {set/get the associated database object}}
set helplib_data(dgo_observer)		{{cmd [args]} {Attach/detach observer to/from list}}
set helplib_data(dgo_report)		{{[lvl]} {print solid table & vector list}}
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

proc helplib args {
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


proc aproposlib key {
	global helplib_data

	return [apropos_comm helplib_data $key]
}
