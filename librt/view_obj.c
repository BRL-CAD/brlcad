/*
 *				V I E W _ O B J . C
 *
 * A view object contains the attributes and methods for
 * controlling viewing transformations. Much of this code
 * was extracted from MGED and modified to work herein.
 * 
 * Source -
 *	SLAD CAD Team
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 * Author -
 *	Robert G. Parker
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Package" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1997 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#include "conf.h"
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <math.h>
#include "tcl.h"
#include "machine.h"
#include "externs.h"
#include "cmd.h"		/* includes bu.h */
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"

int Vo_Init();

static int vo_open_tcl();
static int vo_close_tcl();
#if 0
static int vo_scale_tcl();
#endif
static int vo_size_tcl();
static int vo_invSize_tcl();
static int vo_aet_tcl();
static int vo_rmat_tcl();
static int vo_center_tcl();
static int vo_model2view_tcl();
static int vo_pmodel2view_tcl();
static int vo_view2model_tcl();
static int vo_perspective_tcl();
static int vo_pmat_tcl();
static int vo_rot_tcl();
static int vo_tra_tcl();
static int vo_slew_tcl();

static int vo_eye_tcl();
static int vo_eye_pos_tcl();
static int vo_lookat_tcl();
static int vo_orientation_tcl();
static int vo_pov_tcl();
static int vo_units_tcl();
static int vo_zoom_tcl();
static int vo_local2base_tcl();
static int vo_base2local_tcl();
static int vo_observer_tcl();
static int vo_coord_tcl();
static int vo_rotate_about_tcl();
static int vo_keypoint_tcl();

static int vo_cmd();
static void vo_update();
static void vo_mat_aet();
static void vo_persp_mat();
static void vo_mike_persp_mat();

struct view_obj HeadViewObj;		/* head of view object list */

static struct bu_cmdtab vo_cmds[] = 
{
	{"aet",			vo_aet_tcl},
	{"base2local",		vo_base2local_tcl},
	{"center",		vo_center_tcl},
	{"close",		vo_close_tcl},
	{"coord",		vo_coord_tcl},
	{"eye",			vo_eye_tcl},
	{"eye_pos",		vo_eye_pos_tcl},
	{"invSize",		vo_invSize_tcl},
	{"keypoint",		vo_keypoint_tcl},
	{"local2base",		vo_local2base_tcl},
	{"lookat",		vo_lookat_tcl},
	{"model2view",		vo_model2view_tcl},
	{"observer",		vo_observer_tcl},
	{"orientation",		vo_orientation_tcl},
	{"perspective",		vo_perspective_tcl},
	{"pmat",		vo_pmat_tcl},
	{"pmodel2view",		vo_pmodel2view_tcl},
	{"pov",			vo_pov_tcl},
	{"rmat",		vo_rmat_tcl},
	{"rot",			vo_rot_tcl},
	{"rotate_about",	vo_rotate_about_tcl},
#if 0
	{"scale",		vo_scale_tcl},
#endif
	{"size",		vo_size_tcl},
	{"slew",		vo_slew_tcl},
	{"tra",			vo_tra_tcl},
	{"units",		vo_units_tcl},
	{"view2model",		vo_view2model_tcl},
	{"zoom",		vo_zoom_tcl},
	{(char *)0,		(int (*)())0}
};

static int
vo_cmd(clientData, interp, argc, argv)
     ClientData	clientData;
     Tcl_Interp	*interp;
     int		argc;
     char		**argv;
{
	return bu_cmd(clientData, interp, argc, argv, vo_cmds, 1);
}

int
Vo_Init(interp)
     Tcl_Interp *interp;
{
	BU_LIST_INIT(&HeadViewObj.l);
	(void)Tcl_CreateCommand(interp, "v_open", vo_open_tcl,
				(ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

	return TCL_OK;
}

static void
vo_deleteProc(clientData)
     ClientData clientData;
{
	struct view_obj *vop = (struct view_obj *)clientData;

	/* free observers */
	bu_observer_free(&vop->vo_observers);

	bu_vls_free(&vop->vo_name);
	BU_LIST_DEQUEUE(&vop->l);
	bu_free((genptr_t)vop, "vo_deleteProc: vop");
}

/*
 * Close a view object.
 *
 * USAGE:
 *        procname close
 */
static int
vo_close_tcl(clientData, interp, argc, argv)
     ClientData      clientData;
     Tcl_Interp      *interp;
     int             argc;
     char            **argv;
{
	struct bu_vls vls;
	struct view_obj *vop = (struct view_obj *)clientData;

	if (argc != 2) {
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib vo_close");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	/* Among other things, this will call vo_deleteProc. */
	Tcl_DeleteCommand(interp, bu_vls_addr(&vop->vo_name));

	return TCL_OK;
}

/*
 * Create an command/object named "oname" in "interp".
 */
struct view_obj *
vo_open_cmd(Tcl_Interp		*interp,
	    char		*oname)
{
	struct view_obj *vop;

	BU_GETSTRUCT(vop,view_obj);

	/* initialize view_obj */
	bu_vls_init(&vop->vo_name);
	bu_vls_strcpy(&vop->vo_name, oname);
	vop->vo_scale = 1.0;
	vop->vo_size = 2.0 * vop->vo_scale;
	vop->vo_invSize = 1.0 / vop->vo_size;
	vop->vo_local2base = 1.0;		/* default units - mm */
	vop->vo_base2local = 1.0;		/* default units - mm */
	VSET(vop->vo_eye_pos, 0.0, 0.0, 1.0);
	MAT_IDN(vop->vo_rotation);
	MAT_IDN(vop->vo_center);
	VSETALL(vop->vo_keypoint, 0.0);
	vop->vo_coord = 'v';
	vop->vo_rotate_about = 'v';
	vo_update(vop, interp, 0);
	BU_LIST_INIT(&vop->vo_observers.l);

	/* append to list of view_obj's */
	BU_LIST_APPEND(&HeadViewObj.l,&vop->l);

	return vop;
}

/*
 * Open a view object.
 *
 * USAGE: v_open [name]
 */
int vo_open_tcl(ClientData	clientData,
		Tcl_Interp	*interp,
		int             argc,
		char            **argv)
{
	struct view_obj *vop;

	if (argc == 1) {
		/* get list of view objects */
		for (BU_LIST_FOR(vop, view_obj, &HeadViewObj.l))
			Tcl_AppendResult(interp, bu_vls_addr(&vop->vo_name), " ", (char *)NULL);

		return TCL_OK;
	}

	/* first, delete any commands by this name */
	(void)Tcl_DeleteCommand(interp, argv[1]);

	vop = vo_open_cmd(interp, argv[1]);
	(void)Tcl_CreateCommand(interp,
				bu_vls_addr(&vop->vo_name),
				vo_cmd,
				(ClientData)vop,
				vo_deleteProc);

	/* Return new function name as result */
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, bu_vls_addr(&vop->vo_name), (char *)NULL);

	return TCL_OK;
}

#if 0
/*
 * Get or set the view scale.
 */
static int
vo_scale_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct view_obj *vop = (struct view_obj *)clientData;
	struct bu_vls vls;
	fastf_t scale;

	/* get view scale */
	if (argc == 2) {
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "%g", vop->vo_scale * vop->vo_base2local);
		Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);

		return TCL_OK;
	}

	/* set view scale */
	if (argc == 3) {
		if (sscanf(argv[2], "%lf", &scale) != 1) {
			Tcl_AppendResult(interp, "bad scale value - ",
					 argv[2], (char *)NULL);
			return TCL_ERROR;
		}

		vop->vo_scale = vop->vo_local2base * scale;
		vop->vo_size = 2.0 * scale;
		vop->vo_invSize = 1.0 / vop->vo_size;
		vo_update(vop, interp, 1);

		return TCL_OK;
	}

	/* compose error message */
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib vo_scale");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return TCL_ERROR;
}
#endif

/*
 * Get or set the view size.
 */
static int
vo_size_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct view_obj *vop = (struct view_obj *)clientData;
	struct bu_vls vls;
	fastf_t size;

	/* get view size */
	if (argc == 2) {
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "%g", vop->vo_size * vop->vo_base2local);
		Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);

		return TCL_OK;
	}

	/* set view size */
	if (argc == 3) {
		if (sscanf(argv[2], "%lf", &size) != 1) {
			Tcl_AppendResult(interp, "bad size - ",
					 argv[2], (char *)NULL);
			return TCL_ERROR;
		}

		vop->vo_size = vop->vo_local2base * size;
		vop->vo_invSize = 1.0 / size;
		vop->vo_scale = 0.5 * size;
		vo_update(vop, interp, 1);

		return TCL_OK;
	}

	/* compose error message */
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib vo_size");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return TCL_ERROR;
}

/*
 * Get the inverse view size.
 */
static int
vo_invSize_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct view_obj *vop = (struct view_obj *)clientData;
	struct bu_vls vls;

	if (argc == 2) {
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "%g", vop->vo_invSize * vop->vo_base2local);
		Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);

		return TCL_OK;
	}

	/* compose error message */
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib vo_invSize");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return TCL_ERROR;
}

/*
 * Get or set the azimuth, elevation and twist.
 */
static int
vo_aet_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct view_obj *vop = (struct view_obj *)clientData;
	struct bu_vls vls;
	vect_t aet;

	if (argc == 2) { /* get aet */
		bu_vls_init(&vls);
		bn_encode_vect(&vls, vop->vo_aet);
		Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);

		return TCL_OK;
	} else if (argc == 3) {  /* set aet */
		int n;

		if ((n = bn_decode_vect(aet, argv[2])) == 2)
			aet[2] = 0;
		else if (n != 3)
			goto error;

		VMOVE(vop->vo_aet, aet);
		vo_mat_aet(vop);
		vo_update(vop, interp, 1);

		return TCL_OK;
	}

error:
	/* compose error message */
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib vo_aet");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return TCL_ERROR;
}

/*
 * Get or set the rotation matrix.
 */
static int
vo_rmat_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct view_obj *vop = (struct view_obj *)clientData;
	struct bu_vls vls;
	mat_t rotation;

	if (argc == 2) { /* get rotation matrix */
		bu_vls_init(&vls);
		bn_encode_mat(&vls, vop->vo_rotation);
		Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);

		return TCL_OK;
	} else if (argc == 3) {  /* set rotation matrix */
		if (bn_decode_mat(rotation, argv[2]) != 16)
			return TCL_ERROR;

		MAT_COPY(vop->vo_rotation, rotation);
		vo_update(vop, interp , 1);

		return TCL_OK;
	}

	/* compose error message */
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib vo_rmat");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return TCL_ERROR;
}

/*
 * Get or set the view center.
 */
static int
vo_center_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct view_obj *vop = (struct view_obj *)clientData;
	vect_t center;
	struct bu_vls vls;

	/* get view center */
	if (argc == 2) {
		MAT_DELTAS_GET_NEG(center, vop->vo_center);
		VSCALE(center, center, vop->vo_base2local);
		bu_vls_init(&vls);
		bn_encode_vect(&vls, center);
		Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);

		return TCL_OK;
	}

	/* set view center */
	if (argc == 3) {
		if (bn_decode_vect(center, argv[2]) != 3)
			return TCL_ERROR;

		VSCALE(center, center, vop->vo_local2base);
		MAT_DELTAS_VEC_NEG(vop->vo_center, center);
		vo_update(vop, interp, 1);

		return TCL_OK;
	}

	/* compose error message */
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib vo_center");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return TCL_ERROR;
}

/*
 * Get the model2view matrix.
 */
static int
vo_model2view_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct view_obj *vop = (struct view_obj *)clientData;
	struct bu_vls vls;

	if (argc == 2) {
		bu_vls_init(&vls);
		bn_encode_mat(&vls, vop->vo_model2view);
		Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);

		return TCL_OK;
	}

	/* compose error message */
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib vo_model2view");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return TCL_ERROR;
}

/*
 * Get the pmodel2view matrix.
 */
static int
vo_pmodel2view_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct view_obj *vop = (struct view_obj *)clientData;
	struct bu_vls vls;

	if (argc == 2) {
		bu_vls_init(&vls);
		bn_encode_mat(&vls, vop->vo_pmodel2view);
		Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);

		return TCL_OK;
	}

	/* compose error message */
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib vo_pmodel2view");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return TCL_ERROR;
}

/*
 * Get the view2model matrix.
 */
static int
vo_view2model_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct view_obj *vop = (struct view_obj *)clientData;
	struct bu_vls vls;

	if (argc == 2) {
		bu_vls_init(&vls);
		bn_encode_mat(&vls, vop->vo_view2model);
		Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);

		return TCL_OK;
	}

	/* compose error message */
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib vo_view2model");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return TCL_ERROR;
}

/*
 * Get/set the perspective angle.
 *
 * Usage:
 *        procname perspective [angle]
 *
 */
static int
vo_perspective_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct view_obj *vop = (struct view_obj *)clientData;
	struct bu_vls vls;
	fastf_t perspective;

	/* get the perspective angle */
	if (argc == 2) {
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "%g", vop->vo_perspective);
		Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);

		return TCL_OK;
	}

	/* set the perspective angle */
	if (argc == 3) {
		if (sscanf(argv[2], "%lf", &perspective) != 1) {
			Tcl_AppendResult(interp, "bad perspective angle - ",
					 argv[2], (char *)NULL);
			return TCL_ERROR;
		}

		vop->vo_perspective = perspective;

#if 1
		/* This way works, with reasonable Z-clipping */
		vo_persp_mat(vop->vo_pmat, vop->vo_perspective,
			     1.0, 0.01, 1.0e10, 1.0);
#else
		vo_mike_persp_mat(vop->vo_pmat, vop->vo_eye_pos);
#endif
		vo_update(vop, interp, 1);

		return TCL_OK;
	}

	/* Compose error message */
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib vo_perspective");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return TCL_ERROR;
}

/*
 * Get the perspective matrix.
 */
static int
vo_pmat_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct view_obj *vop = (struct view_obj *)clientData;
	struct bu_vls vls;

	if (argc == 2) {
		bu_vls_init(&vls);
		bn_encode_mat(&vls, vop->vo_pmat);
		Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);

		return TCL_OK;
	}

	/* compose error message */
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib vo_pmat");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return TCL_ERROR;
}

static void
vo_update(vop, interp, oflag)
     struct view_obj *vop;
     Tcl_Interp *interp;
     int oflag;
{
	vect_t work, work1;
	vect_t temp, temp1;

	bn_mat_mul(vop->vo_model2view,
		   vop->vo_rotation,
		   vop->vo_center);
	vop->vo_model2view[15] = vop->vo_scale;
	bn_mat_inv(vop->vo_view2model, vop->vo_model2view);

	/* Find current azimuth, elevation, and twist angles */
	VSET(work, 0.0, 0.0, 1.0);       /* view z-direction */
	MAT4X3VEC(temp , vop->vo_view2model , work);
	VSET(work1 , 1.0, 0.0, 0.0);      /* view x-direction */
	MAT4X3VEC(temp1 , vop->vo_view2model , work1);

	/* calculate angles using accuracy of 0.005, since display
	 * shows 2 digits right of decimal point */
	bn_aet_vec(&vop->vo_aet[0],
		   &vop->vo_aet[1],
		   &vop->vo_aet[2],
		   temp, temp1, (fastf_t)0.005);

	/* Force azimuth range to be [0,360] */
	if ((NEAR_ZERO(vop->vo_aet[1] - 90.0,(fastf_t)0.005) ||
	     NEAR_ZERO(vop->vo_aet[1] + 90.0,(fastf_t)0.005)) &&
	    vop->vo_aet[0] < 0 &&
	    !NEAR_ZERO(vop->vo_aet[0],(fastf_t)0.005))
		vop->vo_aet[0] += 360.0;
	else if (NEAR_ZERO(vop->vo_aet[0],(fastf_t)0.005))
		vop->vo_aet[0] = 0.0;

	/* apply the perspective angle to model2view */
	bn_mat_mul(vop->vo_pmodel2view, vop->vo_pmat, vop->vo_model2view);

	if (oflag)
		bu_observer_notify(interp, &vop->vo_observers, bu_vls_addr(&vop->vo_name));
}

static void
vo_mat_aet(vop)
     struct view_obj *vop;
{
	mat_t tmat;
	fastf_t twist;
	fastf_t c_twist;
	fastf_t s_twist;

	bn_mat_angles(vop->vo_rotation,
		      270.0 + vop->vo_aet[1],
		      0.0,
		      270.0 - vop->vo_aet[0]);

	twist = -vop->vo_aet[2] * bn_degtorad;
	c_twist = cos(twist);
	s_twist = sin(twist);
	bn_mat_zrot(tmat, s_twist, c_twist);
	bn_mat_mul2(tmat, vop->vo_rotation);
}

/*
 *			P E R S P _ M A T
 *
 *  This code came from mged/dozoom.c.
 *  Compute a perspective matrix for a right-handed coordinate system.
 *  Reference: SGI Graphics Reference Appendix C
 *  (Note:  SGI is left-handed, but the fix is done in the Display Manger).
 */
static void
vo_persp_mat(m, fovy, aspect, near, far, backoff)
     mat_t	m;
     fastf_t	fovy, aspect, near, far, backoff;
{
	mat_t	m2, tran;

	fovy *= 3.1415926535/180.0;

	MAT_IDN(m2);
	m2[5] = cos(fovy/2.0) / sin(fovy/2.0);
	m2[0] = m2[5]/aspect;
	m2[10] = (far+near) / (far-near);
	m2[11] = 2*far*near / (far-near);	/* This should be negative */

	m2[14] = -1;		/* XXX This should be positive */
	m2[15] = 0;

	/* Move eye to origin, then apply perspective */
	MAT_IDN(tran);
	tran[11] = -backoff;
	bn_mat_mul(m, m2, tran);
}

/*
 *  This code came from mged/dozoom.c.
 *  Create a perspective matrix that transforms the +/1 viewing cube,
 *  with the acutal eye position (not at Z=+1) specified in viewing coords,
 *  into a related space where the eye has been sheared onto the Z axis
 *  and repositioned at Z=(0,0,1), with the same perspective field of view
 *  as before.
 *
 *  The Zbuffer clips off stuff with negative Z values.
 *
 *  pmat = persp * xlate * shear
 */
static void
vo_mike_persp_mat(pmat, eye)
     mat_t		pmat;
     const point_t	eye;
{
	mat_t	shear;
	mat_t	persp;
	mat_t	xlate;
	mat_t	t1, t2;
	point_t	sheared_eye;

	if( eye[Z] < SMALL )  {
		VPRINT("mike_persp_mat(): ERROR, z<0, eye", eye);
		return;
	}

	/* Shear "eye" to +Z axis */
	MAT_IDN(shear);
	shear[2] = -eye[X]/eye[Z];
	shear[6] = -eye[Y]/eye[Z];

	MAT4X3VEC( sheared_eye, shear, eye );
	if( !NEAR_ZERO(sheared_eye[X], .01) || !NEAR_ZERO(sheared_eye[Y], .01) )  {
		VPRINT("ERROR sheared_eye", sheared_eye);
		return;
	}

	/* Translate along +Z axis to put sheared_eye at (0,0,1). */
	MAT_IDN(xlate);
	/* XXX should I use MAT_DELTAS_VEC_NEG()?  X and Y should be 0 now */
	MAT_DELTAS( xlate, 0, 0, 1-sheared_eye[Z] );

	/* Build perspective matrix inline, substituting fov=2*atan(1,Z) */
	MAT_IDN( persp );
	/* From page 492 of Graphics Gems */
	persp[0] = sheared_eye[Z];	/* scaling: fov aspect term */
	persp[5] = sheared_eye[Z];	/* scaling: determines fov */

	/* From page 158 of Rogers Mathematical Elements */
	/* Z center of projection at Z=+1, r=-1/1 */
	persp[14] = -1;

	bn_mat_mul( t1, xlate, shear );
	bn_mat_mul( t2, persp, t1 );

	/* Now, move eye from Z=1 to Z=0, for clipping purposes */
	MAT_DELTAS( xlate, 0, 0, -1 );

	bn_mat_mul( pmat, xlate, t2 );
}

/*
 * Set the eye point.
 *
 * Usage:
 *	procname eye eye_point
 */
static int
vo_eye_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct view_obj *vop = (struct view_obj *)clientData;
	point_t	eye_model;
	vect_t	xlate;
	vect_t	new_cent;

	if (argc != 3) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib vo_eye");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	if (bn_decode_vect(eye_model, argv[2]) != 3)
		return TCL_ERROR;

	VSCALE(eye_model, eye_model, vop->vo_local2base);

	/* First step:  put eye at view center (view 0,0,0) */
	MAT_DELTAS_VEC_NEG(vop->vo_center, eye_model);
	vo_update(vop, interp, 0);

	/*  Second step:  put eye at view 0,0,1.
	 *  For eye to be at 0,0,1, the old 0,0,-1 needs to become 0,0,0.
	 */
	VSET(xlate, 0.0, 0.0, -1.0);	/* correction factor */
	MAT4X3PNT(new_cent, vop->vo_view2model, xlate);
	MAT_DELTAS_VEC_NEG(vop->vo_center, new_cent);
	vo_update(vop, interp, 1);

	return TCL_OK;
}

/*
 * Set the eye position.
 *
 * Usage:
 *	procname eye_pos pos
 */
static int
vo_eye_pos_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct view_obj *vop = (struct view_obj *)clientData;
	vect_t eye_pos;

	if (argc != 3) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib vo_eye_pos");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	if (bn_decode_vect(eye_pos, argv[2]) != 3)
		return TCL_ERROR;

	VSCALE(eye_pos, eye_pos, vop->vo_local2base);
	VMOVE(vop->vo_eye_pos, eye_pos);

	/* update perspective matrix */
	vo_mike_persp_mat(vop->vo_pmat, vop->vo_eye_pos);

	/* update all other view related matrices */
	vo_update(vop, interp, 1);

	return TCL_OK;
}

/*
 * Set look-at point.
 *
 * Usage:
 *	procname lookat lookat_point
 */
static int
vo_lookat_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct view_obj *vop = (struct view_obj *)clientData;
	point_t look;
	point_t eye;
	point_t tmp;
	point_t new_center;
	vect_t dir;
	fastf_t new_az, new_el;

	if (argc != 3) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib vo_lookat");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	if (bn_decode_vect(look, argv[2]) != 3) {
		return TCL_ERROR;
	}

	VSCALE(look, look, vop->vo_local2base);

	VSET(tmp, 0.0, 0.0, 1.0);
	MAT4X3PNT(eye, vop->vo_view2model, tmp);

	VSUB2(dir, eye, look);
	VUNITIZE(dir);
	bn_ae_vec(&new_az, &new_el, dir);

	VSET(vop->vo_aet, new_az, new_el, vop->vo_aet[Z]);
	vo_mat_aet(vop);

	VJOIN1(new_center, eye, -vop->vo_scale, dir);
	MAT_DELTAS_VEC_NEG(vop->vo_center, new_center);

	vo_update(vop, interp, 1);

	return TCL_OK;
}

/*
 * Usage:
 *	procname orient quat
 */
static int
vo_orientation_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct view_obj *vop = (struct view_obj *)clientData;
	quat_t quat;

	if (argc != 3) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib vo_orient");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	if (bn_decode_quat(quat, argv[2]) != 4) {
		return TCL_ERROR;
	}

	quat_quat2mat(vop->vo_rotation, quat);
	vo_update(vop, interp, 1);

	return TCL_OK;
}

/*
 * Usage:
 *	procname pov center quat scale eye_pos perspective
 */
static int
vo_pov_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct view_obj *vop = (struct view_obj *)clientData;
	vect_t center;
	quat_t quat;
	vect_t eye_pos;
	fastf_t scale;
	fastf_t perspective;

	if (argc != 7) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib vo_pov");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	/***************** Get the arguments *******************/

	if (bn_decode_vect(center, argv[2]) != 3) {
		return TCL_ERROR;
	}

	if (bn_decode_quat(quat, argv[3]) != 4) {
		return TCL_ERROR;
	}

	if (sscanf(argv[4], "%lf", &scale) != 1) {
		return TCL_ERROR;
	}

	if (bn_decode_vect(eye_pos, argv[5]) != 3) {
		return TCL_ERROR;
	}

	if (sscanf(argv[4], "%lf", &perspective) != 1) {
		return TCL_ERROR;
	}

	/***************** Use the arguments *******************/

	VSCALE(center, center, vop->vo_local2base);
	MAT_DELTAS_VEC_NEG(vop->vo_center, center);
	quat_quat2mat(vop->vo_rotation, quat);
	vop->vo_scale = vop->vo_local2base * scale;
	VSCALE(eye_pos, eye_pos, vop->vo_local2base);
	VMOVE(vop->vo_eye_pos, eye_pos);
	vop->vo_perspective = perspective;

	vo_update(vop, interp, 1);

	return TCL_OK;
}

/*
 * Usage:
 *	procname zoom scale_factor
 */
static int
vo_zoom_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct view_obj *vop = (struct view_obj *)clientData;
	fastf_t sf;

	if (argc != 3) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib vo_zoom");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);

		return TCL_ERROR;
	}

	if (sscanf(argv[2], "%lf", &sf) != 1) {
		Tcl_AppendResult(interp, "bad zoom value - ",
				 argv[2], (char *)NULL);
		return TCL_ERROR;
	}

	vop->vo_scale /= sf;
	vop->vo_size = 2.0 * vop->vo_scale;
	vop->vo_invSize = 1.0 / vop->vo_size;
	vo_update(vop, interp, 1);

	return TCL_OK;
}

/*
 * Set/get local units.
 *
 * Usage:
 *	procname units unit_spec
 */
static int
vo_units_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct view_obj *vop = (struct view_obj *)clientData;
	struct bu_vls vls;

	/* get units */
	if (argc == 2) {
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "%s", bu_units_string(vop->vo_local2base));
		Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);

		return TCL_OK;
	}

	/* set units */
	if (argc == 3) {
		double uval;

		if ((uval = bu_units_conversion(argv[2])) == 0) {
			bu_vls_init(&vls);
			bu_vls_printf(&vls, "unrecognized unit type - %s\n", argv[2]);
			Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
			bu_vls_free(&vls);

			return TCL_ERROR;
		}

		vop->vo_local2base = uval;
		vop->vo_base2local = 1.0 / vop->vo_local2base;

		return TCL_OK;
	}

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib vo_units");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return TCL_ERROR;
}

/*
 * Get base2local conversion factor.
 *
 * Usage:
 *	procname base2local
 */
static int
vo_base2local_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct view_obj *vop = (struct view_obj *)clientData;
	struct bu_vls vls;

	bu_vls_init(&vls);

	if (argc != 2) {
		bu_vls_printf(&vls, "helplib vo_base2local");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);

		return TCL_ERROR;
	}

	bu_vls_printf(&vls, "%g", vop->vo_base2local);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	return TCL_OK;
}

/*
 * Get local2base conversion factor.
 *
 * Usage:
 *	procname local2base
 */
static int
vo_local2base_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct view_obj *vop = (struct view_obj *)clientData;
	struct bu_vls vls;

	bu_vls_init(&vls);

	if (argc != 2) {
		bu_vls_printf(&vls, "helplib vo_local2base");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);

		return TCL_ERROR;
	}

	bu_vls_printf(&vls, "%g", vop->vo_local2base);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	return TCL_OK;
}

/*
 * Rotate the view according to xyz.
 *
 * Usage:
 *	procname rot [-v|-m] xyz
 */
static int
vo_rot_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct view_obj *vop = (struct view_obj *)clientData;
	vect_t rvec;
	mat_t rmat;
	mat_t temp1, temp2;
	char coord = vop->vo_coord;

	if (argc < 3 || 4 < argc) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib vo_rot");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);

		return TCL_ERROR;
	}

	/* process coord flag */
	if (argc == 4) {
		if (argv[2][0] == '-' &&
		    (argv[2][1] == 'v' || argv[2][1] == 'm') &&
		    argv[2][2] == '\0') {
			coord = argv[2][1];
			--argc;
			++argv;
		} else {
			struct bu_vls vls;

			bu_vls_init(&vls);
			bu_vls_printf(&vls, "helplib vo_rot");
			Tcl_Eval(interp, bu_vls_addr(&vls));
			bu_vls_free(&vls);

			return TCL_ERROR;
		}
	}

	if (bn_decode_vect(rvec, argv[2]) != 3) {
		Tcl_AppendResult(interp, "vo_rot: bad xyz - ", argv[2], (char *)NULL);
		return TCL_ERROR;
	}

	VSCALE(rvec, rvec, -1.0);
	bn_mat_angles(rmat, rvec[X], rvec[Y], rvec[Z]);

	switch (coord) {
	case 'm':
		/* transform model rotations into view rotations */
		bn_mat_inv(temp1, vop->vo_rotation);
		bn_mat_mul(temp2, vop->vo_rotation, rmat);
		bn_mat_mul(rmat, temp2, temp1);
		break;
	case 'v':
	default:
		break;
	}

	/* Calculate new view center */
	if (vop->vo_rotate_about != 'v') {
		point_t		rot_pt;
		point_t		new_origin;
		mat_t		viewchg, viewchginv;
		point_t		new_cent_view;
		point_t		new_cent_model;

		switch (vop->vo_rotate_about) {
		case 'e':
			VSET(rot_pt, 0.0, 0.0, 1.0);
			break;
		case 'k':
			MAT4X3PNT(rot_pt, vop->vo_model2view, vop->vo_keypoint);
			break;
		case 'm':
			/* rotate around model center (0,0,0) */
			VSET(new_origin, 0.0, 0.0, 0.0);
			MAT4X3PNT(rot_pt, vop->vo_model2view, new_origin);
			break;
		default:
			{
				struct bu_vls vls;

				bu_vls_init(&vls);
				bu_vls_printf(&vls, "vo_rot_tcl: bad rotate_about - %c\n", vop->vo_rotate_about);
				Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)0);
				bu_vls_free(&vls);
				return TCL_ERROR;
			}
		}

		bn_mat_xform_about_pt(viewchg, rmat, rot_pt);
		bn_mat_inv(viewchginv, viewchg);

		/* Convert origin in new (viewchg) coords back to old view coords */
		VSET(new_origin, 0.0, 0.0, 0.0);
		MAT4X3PNT(new_cent_view, viewchginv, new_origin);
		MAT4X3PNT(new_cent_model, vop->vo_view2model, new_cent_view);
		MAT_DELTAS_VEC_NEG(vop->vo_center, new_cent_model);
	}

	/* pure rotation */
	bn_mat_mul2(rmat, vop->vo_rotation);
	vo_update(vop, interp, 1);

	return TCL_OK;
}

/*
 * Translate the view according to xyz.
 *
 * Usage:
 *	procname tra xyz
 */
static int
vo_tra_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct view_obj *vop = (struct view_obj *)clientData;
	vect_t tvec;
	point_t delta;
	point_t work;
	point_t vc, nvc;
	char coord = vop->vo_coord;

	if (argc < 3 || 4 < argc) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib vo_tra");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);

		return TCL_ERROR;
	}

	/* process coord flag */
	if (argc == 4) {
		if (argv[2][0] == '-' &&
		    (argv[2][1] == 'v' || argv[2][1] == 'm') &&
		    argv[2][2] == '\0') {
			coord = argv[2][1];
			--argc;
			++argv;
		} else {
			struct bu_vls vls;

			bu_vls_init(&vls);
			bu_vls_printf(&vls, "helplib vo_tra");
			Tcl_Eval(interp, bu_vls_addr(&vls));
			bu_vls_free(&vls);

			return TCL_ERROR;
		}
	}

	if (bn_decode_vect(tvec, argv[2]) != 3) {
		Tcl_AppendResult(interp, "vo_tra: bad xyz - ", argv[2], (char *)NULL);
		return TCL_ERROR;
	}

	switch (coord) {
	case 'm':
		VSCALE(delta, tvec, vop->vo_local2base);
		MAT_DELTAS_GET_NEG(vc, vop->vo_center);
		break;
	case 'v':
		VSCALE(tvec, tvec, -2.0*vop->vo_local2base*vop->vo_invSize);
		MAT4X3PNT(work, vop->vo_view2model, tvec);
		MAT_DELTAS_GET_NEG(vc, vop->vo_center);
		VSUB2(delta, work, vc);
		break;
	}

	VSUB2(nvc, vc, delta);
	MAT_DELTAS_VEC_NEG(vop->vo_center, nvc);
	vo_update(vop, interp, 1);

	return TCL_OK;
}

/*
 * Make xyz the new view center.
 *
 * Usage:
 *	procname slew xy
 */
static int
vo_slew_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct view_obj *vop = (struct view_obj *)clientData;
	vect_t slewvec;
	point_t model_center;

	if (argc != 3) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib vo_slew");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);

		return TCL_ERROR;
	}

	if (sscanf(argv[2], "%lf %lf", &slewvec[X], &slewvec[Y]) != 2) {
		Tcl_AppendResult(interp, "vo_slew: bad xy - ", argv[2], (char *)NULL);
		return TCL_ERROR;
	}

	slewvec[Z] = 0.0;
	MAT4X3PNT(model_center, vop->vo_view2model, slewvec);
	MAT_DELTAS_VEC_NEG(vop->vo_center, model_center);
	vo_update(vop, interp, 1);

	return TCL_OK;
}

/*
 * Attach/detach observers to/from list.
 *
 * Usage:
 *	  procname observer cmd [args]
 *
 */
static int
vo_observer_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct view_obj *vop = (struct view_obj *)clientData;

	if (argc < 3) {
		struct bu_vls vls;

		/* return help message */
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib vo_observer");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	return bu_cmd((ClientData)&vop->vo_observers,
		      interp, argc - 2, argv + 2, bu_observer_cmds, 0);
}

/*
 * Get/set the coordinate system.
 *
 * Usage:
 *	  procname coord [v|m]
 *
 */
static int
vo_coord_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct bu_vls vls;
	struct view_obj *vop = (struct view_obj *)clientData;

	/* Get coord */
	if (argc == 2) {
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "%c", vop->vo_coord);
		Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)0);
		bu_vls_free(&vls);
		return TCL_OK;
	}

	/* Set coord */
	if (argc == 3) {
		switch (argv[2][0]) {
		case 'm':
		case 'v':
			vop->vo_coord = argv[2][0];
			return TCL_OK;
		}
	}

	/* return help message */
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib vo_coord");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
}

/*
 * Get/set the rotate about point.
 *
 * Usage:
 *	  procname rotate_about [e|k|m|v]
 *
 */
static int
vo_rotate_about_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct bu_vls vls;
	struct view_obj *vop = (struct view_obj *)clientData;

	/* Get rotate_about */
	if (argc == 2) {
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "%c", vop->vo_rotate_about);
		Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)0);
		bu_vls_free(&vls);
		return TCL_OK;
	}

	/* Set rotate_about */
	if (argc == 3 && argv[2][1] == '\0') {
		switch (argv[2][0]) {
		case 'e':
		case 'k':
		case 'm':
		case 'v':
			vop->vo_rotate_about = argv[2][0];
			return TCL_OK;
		}
	}

	/* return help message */
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib vo_rotate_about");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
}

/*
 * Get/set the keypoint.
 *
 * Usage:
 *	  procname keypoint [point]
 *
 */
static int
vo_keypoint_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct bu_vls vls;
	vect_t tvec;
	struct view_obj *vop = (struct view_obj *)clientData;

	/* Get the keypoint */
	if (argc == 2) {
		bu_vls_init(&vls);
		VSCALE(tvec, vop->vo_keypoint, vop->vo_base2local);
		bn_encode_vect(&vls, tvec);
		Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)0);
		bu_vls_free(&vls);
		return TCL_OK;
	}

	/* Set the keypoint */
	if (argc == 3) {
		if (bn_decode_vect(tvec, argv[2]) != 3) {
			Tcl_AppendResult(interp, "vo_keypoint: bad xyz - ", argv[2], (char *)0);
			return TCL_ERROR;
		}

		VSCALE(vop->vo_keypoint, tvec, vop->vo_local2base)
		return TCL_OK;
	}

	/* return help message */
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib vo_keypoint");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
}
