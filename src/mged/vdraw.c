/*                         V D R A W . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file vdraw.c
 *
 */

/*******************************************************************

CMD_VDRAW - edit vector lists and display them as pseudosolids

OPEN COMMAND
vdraw	open			- with no argument, asks if there is
	  			  an open vlist (1 yes, 0 no)

		name		- opens the specified vlist
				  returns 1 if creating new vlist
				          0 if opening an existing vlist

EDITING COMMANDS - no return value

vdraw	write	i	c x y z	- write params into i-th vector
		next	c x y z	- write params to end of vector list
		rpp	x y z x y z - write RPP outline at end of vector list

vdraw	insert	i	c x y z	- insert params in front of i-th vector

vdraw	delete 	i		- delete i-th vector
		last		- delete last vector on list
		all		- delete all vectors on list

PARAMETER SETTING COMMAND - no return value
vdraw	params	color		- set the current color with 6 hex digits
				  representing rrggbb
		name		- change the name of the current vlist

QUERY COMMAND
vdraw	read	i		- returns contents of i-th vector "c x y z"
		color		- return the current color in hex
		length		- return number of vectors in list
		name		- return name of current vlist

DISPLAY COMMAND -
vdraw	send			- send the current vlist to the display
				  returns 0 on success, -1 if the name
				  conflicts with an existing true solid

CURVE COMMANDS
vdraw	vlist	list		- return list of all existing vlists
vdraw	vlist	delete	name	- delete the named vlist

All textual arguments can be replaced by their first letter.
(e.g. "vdraw d a" instead of "vdraw delete all"

In the above listing:

"i" refers to an integer
"c" is an integer representing one of the following rt_vlist commands:
	 RT_VLIST_LINE_MOVE	0	/ begin new line /
	 RT_VLIST_LINE_DRAW	1	/ draw line /
	 RT_VLIST_POLY_START	2	/ pt[] has surface normal /
	 RT_VLIST_POLY_MOVE	3	/ move to first poly vertex /
	 RT_VLIST_POLY_DRAW	4	/ subsequent poly vertex /
	 RT_VLIST_POLY_END	5	/ last vert (repeats 1st), draw poly /
	 RT_VLIST_POLY_VERTNORM	6	/ per-vertex normal, for interpoloation /

"x y z" refer to floating point values which represent a point or normal
	vector. For commands 0,1,3,4, and 5, they represent a point, while
	for commands 2 and 6 they represent normal vectors

author - Carl Nuzman

Example Use -
	vdraw open rays
	vdraw delete all
	foreach partition $ray {
		...stuff...
		vdraw write next 0 $inpt
		vdraw write next 1 $outpt
	}
	vdraw send


********************************************************************/
#include "common.h"



#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <math.h>
#include <signal.h>
#include "tcl.h"

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "mater.h"
#include "nmg.h"
#include "raytrace.h"
#include "./sedit.h"
#include "./ged.h"
#include "./mged_solid.h"
#include "./mged_dm.h"

#include "../librt/debug.h"	/* XXX */

#if 0
#ifndef M_SQRT2
#define M_SQRT2		1.41421356237309504880
#endif


#define VDRW_PREFIX		"_VDRW"
#define VDRW_PREFIX_LEN	6
#define VDRW_MAXNAME	31
#define VDRW_DEF_COLOR	0xffff00
#define REV_BU_LIST_FOR(p,structure,hp)	\
	(p)=BU_LIST_LAST(structure,hp);	\
	BU_LIST_NOT_HEAD(p,hp);		\
	(p)=BU_LIST_PLAST(structure,p)

static struct bu_list vdraw_head;
struct rt_curve {
	struct bu_list	l;
	char		name[VDRW_MAXNAME+1]; 	/* name array */
	long		rgb;	/* color */
	struct bu_list	vhd;	/* head of list of vertices */
};


/*XXX Not being called. */
int my_final_check(hp)
struct bu_list *hp;
{
	struct rt_vlist *vp;

	for ( BU_LIST_FOR( vp, rt_vlist, hp) ) {
		RT_CK_VLIST( vp );
		printf("num_used = %d\n", vp->nused);
	}
}
#endif

#if 0
int
cmd_read_center(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	char result_string[90];
	point_t pos;

	if(argc < 1 || 1 < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help read_center");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	MAT_DELTAS_GET_NEG(pos, view_state->vs_toViewcenter);
	sprintf(result_string,"%.12e %.12e %.12e", pos[0], pos[1], pos[2]);
	Tcl_AppendResult(interp, result_string, (char *)NULL);
	return TCL_OK;

}

int
cmd_read_scale(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	char result_string[90];

	if(argc < 1 || 1 < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help read_scale");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	sprintf(result_string,"%.12e", view_state->vs_Viewscale);
	Tcl_AppendResult(interp, result_string, (char *)NULL);
	return TCL_OK;

}

int
cmd_viewget(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	char result_string[90];
	point_t pos, temp;
	quat_t quat;
	mat_t mymat;
	char c;

	if(argc < 2 || 2 < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "helpdevel viewget");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	/* center, size, eye, ypr */
	c = argv[1][0];
	switch(	c ) {
	case 'c': 	/*center*/
		MAT_DELTAS_GET_NEG(pos, view_state->vs_toViewcenter);
		sprintf(result_string,"%.12g %.12g %.12g", pos[0], pos[1], pos[2]);
		Tcl_AppendResult(interp, result_string, (char *)NULL);
		return TCL_OK;
	case 's':	/*size*/
		/* don't use base2local, because rt doesn't */
		sprintf(result_string,"%.12g", view_state->vs_Viewscale * 2.0);
		Tcl_AppendResult(interp, result_string, (char *)NULL);
		return TCL_OK;
	case 'e':	/*eye*/
		VSET(temp, 0.0, 0.0, 1.0);
		MAT4X3PNT(pos, view_state->vs_view2model, temp);
		sprintf(result_string,"%.12g %.12g %.12g",pos[0],pos[1],pos[2]);
		Tcl_AppendResult(interp, result_string, (char *)NULL);
		return TCL_OK;
	case 'y':	/*ypr*/
		bn_mat_trn( mymat, view_state->vs_Viewrot);
		anim_v_unpermute(mymat);
		c = anim_mat2ypr(temp, mymat);
		if (c==2) {
			Tcl_AppendResult(interp, "mat2ypr - matrix is not a rotation matrix", (char *)NULL);
			return TCL_ERROR;
		}
		VSCALE(temp, temp, bn_radtodeg);
		sprintf(result_string,"%.12g %.12g %.12g",temp[0],temp[1],temp[2]);
		Tcl_AppendResult(interp, result_string, (char *)NULL);
		return TCL_OK;
	case 'a': 	/* aet*/
		bn_mat_trn(mymat,view_state->vs_Viewrot);
		anim_v_unpermute(mymat);
		c = anim_mat2ypr(temp, mymat);
		if (c==2) {
			Tcl_AppendResult(interp, "mat2ypr - matrix is not a rotation matrix", (char *)NULL);
			return TCL_ERROR;
		}
		VSCALE(temp, temp, bn_radtodeg);
		if (temp[0] >= 180.0 ) temp[0] -= 180;
		if (temp[0] < 180.0 ) temp[0] += 180;
		temp[1] = -temp[1];
		temp[2] = -temp[2];
		sprintf(result_string,"%.12g %.12g %.12g",temp[0],temp[1],temp[2]);
		Tcl_AppendResult(interp, result_string, (char *)NULL);
		return TCL_OK;
	case 'q':	/*quat*/
		quat_mat2quat(quat,view_state->vs_Viewrot);
		sprintf(result_string,"%.12g %.12g %.12g %.12g", quat[0],quat[1],quat[2],quat[3]);
		Tcl_AppendResult(interp, result_string, (char *)NULL);
		return TCL_OK;
	default:
		Tcl_AppendResult(interp,
			"cmd_viewget: invalid argument. Must be one of center,size,eye,ypr.",
			(char *)NULL);
		return TCL_ERROR;

	}
}

int
cmd_viewset(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	char result_string[90];
	quat_t quat;
	point_t center, eye;
	vect_t ypr, aet;
	fastf_t size;
	int in_quat, in_center, in_eye, in_ypr, in_aet, in_size;
	int i, res;
	vect_t dir, norm, temp;
	mat_t mymat;

	if(argc < 3){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "helpdevel viewset");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	in_quat = in_center = in_eye = in_ypr = in_aet = in_size = 0.0;
	i = 1;
	while(i < argc) {
		switch( argv[i][0] ) {
		case 'q':	/* quaternion */
			if (i+4 >= argc) {
				Tcl_AppendResult(interp, "viewset: quat options requires four parameters", (char *)NULL);
				return TCL_ERROR;
			}
			res = sscanf(argv[i+1],"%lf",quat);
			res += sscanf(argv[i+2],"%lf",quat+1);
			res += sscanf(argv[i+3],"%lf",quat+2);
			res += sscanf(argv[i+4],"%lf",quat+3);
			if (res < 4) {
				Tcl_AppendResult(interp, "viewset: quat option requires four parameters", (char *)NULL);
				return TCL_ERROR;
			}
			in_quat = 1;
			i += 5;
			break;
		case 'y':	/* yaw,pitch,roll */
			if (i+3 >= argc) {
				Tcl_AppendResult(interp, "viewset: ypr option requires three parameters", (char *)NULL);
				return TCL_ERROR;
			}
			res = sscanf(argv[i+1],"%lf",ypr);
			res += sscanf(argv[i+2],"%lf",ypr+1);
			res += sscanf(argv[i+3],"%lf",ypr+2);
			if (res < 3) {
				Tcl_AppendResult(interp, "viewset: ypr option requires three parameters", (char *)NULL);
				return TCL_ERROR;
			}
			in_ypr = 1;
			i += 4;
			break;
		case 'a':	/* azimuth,elevation,twist */
			if (i+3 >= argc) {
				Tcl_AppendResult(interp, "viewset: aet option requires three parameters", (char *)NULL);
				return TCL_ERROR;
			}
			res = sscanf(argv[i+1],"%lf",aet);
			res += sscanf(argv[i+2],"%lf",aet+1);
			res += sscanf(argv[i+3],"%lf",aet+2);
			if (res < 3) {
				Tcl_AppendResult(interp, "viewset: aet option requires three parameters", (char *)NULL);
				return TCL_ERROR;
			}
			in_aet = 1;
			i += 4;
			break;
		case 'c':	/* center point */
			if (i+3 >= argc) {
				Tcl_AppendResult(interp, "viewset: center option requires three parameters", (char *)NULL);
				return TCL_ERROR;
			}
			res = sscanf(argv[i+1],"%lf",center);
			res += sscanf(argv[i+2],"%lf",center+1);
			res += sscanf(argv[i+3],"%lf",center+2);
			if (res < 3) {
				Tcl_AppendResult(interp, "viewset: center option requires three parameters", (char *)NULL);
				return TCL_ERROR;
			}
			in_center = 1;
			i += 4;
			break;
		case 'e':	/* eye_point */
			if (i+3 >= argc) {
				Tcl_AppendResult(interp, "viewset: eye option requires three parameters", (char *)NULL);
				return TCL_ERROR;
			}
			res = sscanf(argv[i+1],"%lf",eye);
			res += sscanf(argv[i+2],"%lf",eye+1);
			res += sscanf(argv[i+3],"%lf",eye+2);
			if (res < 3) {
				Tcl_AppendResult(interp, "viewset: eye option requires three parameters", (char *)NULL);
				return TCL_ERROR;
			}
			in_eye = 1;
			i += 4;
			break;
		case 's': 	/* view size */
			if (i+1 >= argc) {
				Tcl_AppendResult(interp, "viewset: size option requires a parameter", (char *)NULL);
				return TCL_ERROR;
			}
			res = sscanf(argv[i+1],"%lf",&size);
			if (res<1) {
				Tcl_AppendResult(interp, "viewset: size option requires a parameter", (char *)NULL);
				return TCL_ERROR;
			}
			in_size = 1;
			i += 2;
			break;
		default:
			sprintf(result_string,"viewset: Unknown option %.40s.", argv[i]);
			Tcl_AppendResult(interp, result_string, (char *)NULL);
			return TCL_ERROR;
		}
	}

	/* do size set - don't use units (local2base) because rt doesn't */
	if (in_size) {
		if (size < 0.0001) size = 0.0001;
		view_state->vs_Viewscale = size * 0.5;
	}


	/* overrides */
	if (in_center&&in_eye) {
		in_ypr = in_aet = in_quat = 0;
	}

	if (in_quat) {
		quat_quat2mat( view_state->vs_Viewrot, quat);
	} else if (in_ypr) {
		anim_dy_p_r2mat(mymat, ypr[0], ypr[1], ypr[2]);
		anim_v_permute(mymat);
		bn_mat_trn(view_state->vs_Viewrot, mymat);
	} else if (in_aet) {
		anim_dy_p_r2mat(mymat, aet[0]+180.0, -aet[1], -aet[2]);
		anim_v_permute(mymat);
		bn_mat_trn(view_state->vs_Viewrot, mymat);
	} else if (in_center && in_eye) {
		VSUB2( dir, center, eye);
		view_state->vs_Viewscale = MAGNITUDE(dir);
		if (view_state->vs_Viewscale < 0.00005) view_state->vs_Viewscale = 0.00005;
		/* use current eye norm as backup if dir vertical*/
		VSET(norm, -view_state->vs_Viewrot[0], -view_state->vs_Viewrot[1], 0.0);
		anim_dirn2mat(mymat, dir, norm);
		anim_v_permute(mymat);
		bn_mat_trn(view_state->vs_Viewrot, mymat);
	}

	if (in_center) {
		MAT_DELTAS_VEC_NEG( view_state->vs_toViewcenter, center);
	} else if (in_eye) {
		VSET(temp, 0.0, 0.0, view_state->vs_Viewscale);
		bn_mat_trn(mymat, view_state->vs_Viewrot);
		MAT4X3PNT( dir, mymat, temp);
		VSUB2(temp, dir, eye);
		MAT_DELTAS_VEC( view_state->vs_toViewcenter, temp);
	}

	new_mats();

	return TCL_OK;

}
#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
