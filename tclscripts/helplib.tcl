#==============================================================================
#
# Help facility for Tclized library routines.
#
#==============================================================================

set helplib_data(wdb_open)		{{widget_command file filename}	{}}
set helplib_data(rt_wdb_inmem_rgb)	{{$wdbp comb r g b}	{}}
set helplib_data(rt_wdb_inmem_shader)	{{$wdbp comb shader [params]}	{}}
set helplib_data(mat_mul)		{{matA matB}	{multiply matrix matA by matB}}
set helplib_data(mat_inv)		{{mat}	{invert a 4x4 matrix}}
set helplib_data(mat_trn)		{{mat}	{transpose a 4x4 matrix}}
set helplib_data(mat_ae)		{{azimuth elevation}	{compute a 4x4 rotation matrix given azimuth and elevation}}
set helplib_data(mat_ae_vec)		{{vect}	{find azimuth and elevation angles that correspond to the direction given by vect}}
set helplib_data(mat_aet_vec)		{{vec_ae vec_twist}	{find azimuth, elevation and twist angles}}
set helplib_data(mat_angles)		{{alpha beta gamma}	{build a homogeneous rotation matrix given 3 angles of rotation}}
set helplib_data(mat_eigen2x2)		{{a b c}	{find eigenvalues and eigenvectors of a symmetric 2x2 matrix}}
set helplib_data(mat_fromto)		{{vecFrom vecTo}	{compute a rotation matrix that will transform space by the angle between the two given vectors}}
set helplib_data(mat_xrot)		{{sinAngle cosAngle}	{find the rotation matrix given the sin and cos of the X rotation angle}}
set helplib_data(mat_yrot)		{{sinAngle cosAngle}	{find the rotation matrix given the sin and cos of the Y rotation angle}}
set helplib_data(mat_zrot)		{{sinAngle cosAngle}	{find the rotation matrix given the sin and cos of the Z rotation angle}}
set helplib_data(mat_lookat)		{{dir yflip}	{compute a matrix which rotates vector dir onto the -Z axis}}
set helplib_data(mat_vec_ortho)		{{vec}	{find a vector which is perpendicular to vec and with unit length}}
set helplib_data(mat_vec_perp)		{{vec}	{find a vector which is perpendicular to vec and with unit length if vec was of unit length}}
set helplib_data(mat_scale_about_pt)	{{pt scale}	{build a matrix to scale uniformly around a given point}}
set helplib_data(mat_xform_about_pt)	{{xform pt}	{build a matrix to apply an arbitrary 4x4 transformation around a given point}}
set helplib_data(mat_arb_rot)		{{pt dir angle}	{build a matrix to rotate about an arbitrary axis}}
set helplib_data(quat_mat2quat)		{{mat}	{convert matrix to quaternion}}
set helplib_data(quat_quat2mat)		{{quat}	{convert quaternion to matrix}}
set helplib_data(quat_distance)		{{quatA quatB}	{finds the euclidean distance between two quaternions}}
set helplib_data(quat_double)		{{quatA quatB}	{finds the quaternion point representing twice the rotation from quatA to quatB}}
set helplib_data(quat_bisect)		{{quatA quatB}	{finds the bisector of quaternions quatA and quatB}}
set helplib_data(quat_slerp)		{{quat1 quat2 factor}	{do spherical linear interpolation between two unit quaternions by the given facotr}}
set helplib_data(quat_sberp)		{{quat1 quat2 quat3 quat4 factor}	{do sperical bezier interpolation between four quaternions by the given factor}}
set helplib_data(quat_make_nearest)	{{quatA quatB}	{set quaternion quatA to the quaternion which yields the smallest roation from quatB}}
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
