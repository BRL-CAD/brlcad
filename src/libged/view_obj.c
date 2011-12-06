/*                      V I E W _ O B J . C
 * BRL-CAD
 *
 * Copyright (c) 1997-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup libged */
/** @{ */
/** @file libged/view_obj.c
 *
 * A view object contains the attributes and methods for controlling
 * viewing transformations.
 *
 */
/** @} */

#include "common.h"

#include <string.h>
#include <math.h>
#include "bio.h"

#include "tcl.h"

#include "bu.h"
#include "bn.h"
#include "cmd.h"
#include "vmath.h"
#include "ged.h"


int Vo_Init(Tcl_Interp *interp);

static int vo_open_tcl(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[]);
static int vo_size_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]);
static int vo_invSize_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]);
static int vo_aet_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]);
static int vo_rmat_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]);
static int vo_center_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]);
static int vo_model2view_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]);
static int vo_pmodel2view_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]);
static int vo_view2model_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]);
static int vo_perspective_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]);
static int vo_pmat_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]);
static int vo_rot_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]);
static int vo_tra_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]);
static int vo_sca_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]);
static int vo_slew_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]);

static int vo_eye_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]);
static int vo_eye_pos_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]);
static int vo_lookat_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]);
static int vo_orientation_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]);
static int vo_pov_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]);
static int vo_units_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]);
static int vo_zoom_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]);
static int vo_local2base_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]);
static int vo_base2local_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]);
static int vo_observer_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]);
static int vo_coord_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]);
static int vo_rotate_about_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]);
static int vo_keypoint_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]);
static int vo_setview_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]);
static int vo_arot_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]);
static int vo_vrot_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]);
static int vo_mrot_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]);
static int vo_mrotPoint_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]);
static int vo_m2vPoint_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]);
static int vo_v2mPoint_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]);
static int vo_viewDir_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]);
static int vo_ae2dir_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]);
static int vo_dir2ae_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]);

static int vo_cmd(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]);
void vo_update(struct view_obj *vop, Tcl_Interp *interp, int oflag);
void vo_mat_aet(struct view_obj *vop);
static void vo_persp_mat(fastf_t *m, fastf_t fovy, fastf_t aspect, fastf_t near1, fastf_t far1, fastf_t backoff);
static void vo_mike_persp_mat(fastf_t *pmat, const fastf_t *eye);

struct view_obj HeadViewObj;		/* head of view object list */

static int
vo_cmd(ClientData clientData,
       Tcl_Interp *interp,
       int argc,
       char *argv[])
{
    static struct bu_cmdtab vo_cmds[] =
	{
	    {"ae",		vo_aet_tcl},
	    {"ae2dir",		vo_ae2dir_tcl},
	    {"arot",		vo_arot_tcl},
	    {"base2local",	vo_base2local_tcl},
	    {"center",		vo_center_tcl},
	    {"coord",		vo_coord_tcl},
	    {"dir2ae",		vo_dir2ae_tcl},
	    {"eye",		vo_eye_tcl},
	    {"eye_pos",		vo_eye_pos_tcl},
	    {"invSize",		vo_invSize_tcl},
	    {"keypoint",	vo_keypoint_tcl},
	    {"local2base",	vo_local2base_tcl},
	    {"lookat",		vo_lookat_tcl},
	    {"m2vPoint",	vo_m2vPoint_tcl},
	    {"model2view",	vo_model2view_tcl},
	    {"mrot",		vo_mrot_tcl},
	    {"mrotPoint",	vo_mrotPoint_tcl},
	    {"observer",	vo_observer_tcl},
	    {"orientation",	vo_orientation_tcl},
	    {"perspective",	vo_perspective_tcl},
	    {"pmat",		vo_pmat_tcl},
	    {"pmodel2view",	vo_pmodel2view_tcl},
	    {"pov",		vo_pov_tcl},
	    {"rmat",		vo_rmat_tcl},
	    {"rot",		vo_rot_tcl},
	    {"rotate_about",	vo_rotate_about_tcl},
	    {"sca",		vo_sca_tcl},
	    {"setview",		vo_setview_tcl},
	    {"size",		vo_size_tcl},
	    {"slew",		vo_slew_tcl},
	    {"tra",		vo_tra_tcl},
	    {"units",		vo_units_tcl},
	    {"v2mPoint",	vo_v2mPoint_tcl},
	    {"view2model",	vo_view2model_tcl},
	    {"viewDir",		vo_viewDir_tcl},
	    {"vrot",		vo_vrot_tcl},
	    {"zoom",		vo_zoom_tcl},
#if 0
	    {"knob",		vo_knob_tcl},
	    {"qorot",		vo_qorot_tcl},
	    {"qvrot",		vo_qvrot_tcl},
	    {"status",		vo_status_tcl},
	    {"",		vo__tcl},
#endif
	    {(char *)0,		(int (*)())0}
	};

    return bu_cmd(clientData, interp, argc, (const char **)argv, vo_cmds, 1);
}


int
Vo_Init(Tcl_Interp *interp)
{
    BU_LIST_INIT(&HeadViewObj.l);
    (void)Tcl_CreateCommand(interp, "v_open", vo_open_tcl,
			    (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

    return TCL_OK;
}


static void
vo_deleteProc(ClientData clientData)
{
    struct view_obj *vop = (struct view_obj *)clientData;

    /* free observers */
    bu_observer_free(&vop->vo_observers);

    bu_vls_free(&vop->vo_name);
    BU_LIST_DEQUEUE(&vop->l);
    bu_free((genptr_t)vop, "vo_deleteProc: vop");
}


/*
 * Create an command/object named "oname" in "interp".
 */
struct view_obj *
vo_open_cmd(const char *oname)
{
    Tcl_Interp *interp = (Tcl_Interp *)NULL;
    struct view_obj *vop;

    BU_GETSTRUCT(vop, view_obj);

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
    vop->vo_callback = (void (*)())0;

    /* append to list of view_obj's */
    BU_LIST_APPEND(&HeadViewObj.l, &vop->l);

    return vop;
}


/*
 * Open a view object.
 *
 * USAGE: v_open [name]
 */
static int
vo_open_tcl(ClientData UNUSED(clientData),
	    Tcl_Interp *interp,
	    int argc,
	    const char *argv[])
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

    vop = vo_open_cmd(argv[1]);
    (void)Tcl_CreateCommand(interp,
			    bu_vls_addr(&vop->vo_name),
			    (Tcl_CmdProc *)vo_cmd,
			    (ClientData)vop,
			    vo_deleteProc);

    /* Return new function name as result */
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, bu_vls_addr(&vop->vo_name), (char *)NULL);

    return TCL_OK;
}


/****************** View Object Methods ********************/


void
vo_size(struct view_obj *vop,
	Tcl_Interp *interp,
	fastf_t size)
{
    vop->vo_size = vop->vo_local2base * size;
    if (vop->vo_size < SQRT_SMALL_FASTF)
	vop->vo_size = SQRT_SMALL_FASTF;
    if (vop->vo_size < RT_MINVIEWSIZE)
	vop->vo_size = RT_MINVIEWSIZE;
    vop->vo_invSize = 1.0 / vop->vo_size;
    vop->vo_scale = 0.5 * vop->vo_size;
    vo_update(vop, interp, 1);
}


int
vo_size_cmd(struct view_obj *vop,
	    Tcl_Interp *interp,
	    int argc,
	    char *argv[])
{
    struct bu_vls vls;
    fastf_t size;

    /* get view size */
    if (argc == 1) {
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "%g", vop->vo_size * vop->vo_base2local);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	return TCL_OK;
    }

    /* set view size */
    if (argc == 2) {
	if (sscanf(argv[1], "%lf", &size) != 1 || size < SMALL_FASTF) {
	    Tcl_AppendResult(interp, "bad size - ", argv[1], (char *)NULL);
	    return TCL_ERROR;
	}

	vo_size(vop, interp, size);
	return TCL_OK;
    }

    /* compose error message */
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "helplib_alias vo_size %s", argv[0]);
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_ERROR;
}


/*
 * Get or set the view size.
 *
 * Usage:
 * procname size [s]
 */
static int
vo_size_tcl(ClientData clientData,
	    Tcl_Interp *interp,
	    int argc,
	    char *argv[])
{
    struct view_obj *vop = (struct view_obj *)clientData;

    return vo_size_cmd(vop, interp, argc-1, argv+1);
}


int
vo_invSize_cmd(struct view_obj *vop,
	       Tcl_Interp *interp,
	       int argc,
	       char *argv[])
{
    struct bu_vls vls;

    if (argc == 1) {
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "%g", vop->vo_invSize * vop->vo_base2local);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	return TCL_OK;
    }

    /* compose error message */
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "helplib_alias vo_invSize %s", argv[0]);
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_ERROR;
}


/*
 * Get the inverse view size.
 *
 * Usage:
 * procname
 */
static int
vo_invSize_tcl(ClientData clientData,
	       Tcl_Interp *interp,
	       int argc,
	       char *argv[])
{
    struct view_obj *vop = (struct view_obj *)clientData;

    return vo_invSize_cmd(vop, interp, argc-1, argv+1);
}


int
vo_aet_cmd(struct view_obj *vop,
	   Tcl_Interp *interp,
	   int argc,
	   char *argv[])
{
    struct bu_vls vls;
    vect_t aet;
    int iflag = 0;

    if (argc == 1) {
	/* get aet */
	bu_vls_init(&vls);
	bn_encode_vect(&vls, vop->vo_aet);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	return TCL_OK;
    }

    /* Check for -i option */
    if (argv[1][0] == '-' && argv[1][1] == 'i') {
	iflag = 1;  /* treat arguments as incremental values */
	++argv;
	--argc;
    }

    if (argc == 2) {
	/* set aet */
	int n;

	if ((n = bn_decode_vect(aet, argv[1])) == 2)
	    aet[2] = 0;
	else if (n != 3)
	    goto bad;

	if (iflag) {
	    VADD2(vop->vo_aet, vop->vo_aet, aet);
	} else {
	    VMOVE(vop->vo_aet, aet);
	}
	vo_mat_aet(vop);
	vo_update(vop, interp, 1);

	return TCL_OK;
    }

    if (argc == 3 || argc == 4) {
	if (sscanf(argv[1], "%lf", &aet[X]) != 1) {
	    Tcl_AppendResult(interp, "vo_aet: bad azimuth - ", argv[1], "\n", (char *)0);
	    return TCL_ERROR;
	}

	if (sscanf(argv[2], "%lf", &aet[Y]) != 1) {
	    Tcl_AppendResult(interp, "vo_aet: bad elevation - ", argv[2], "\n", (char *)0);
	    return TCL_ERROR;
	}

	if (argc == 4) {
	    if (sscanf(argv[3], "%lf", &aet[Z]) != 1) {
		Tcl_AppendResult(interp, "vo_aet: bad twist - ", argv[3], "\n", (char *)0);
		return TCL_ERROR;
	    }
	} else
	    aet[Z] = 0.0;

	if (iflag) {
	    VADD2(vop->vo_aet, vop->vo_aet, aet);
	} else {
	    VMOVE(vop->vo_aet, aet);
	}
	vo_mat_aet(vop);
	vo_update(vop, interp, 1);

	return TCL_OK;
    }

bad:
    /* compose error message */
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "helplib_alias vo_aet %s", argv[0]);
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_ERROR;
}


/*
 * Get or set the azimuth, elevation and twist.
 *
 * Usage:
 * procname ae [[-i] az el [tw]]
 */
static int
vo_aet_tcl(ClientData clientData,
	   Tcl_Interp *interp,
	   int argc,
	   char *argv[])
{
    struct view_obj *vop = (struct view_obj *)clientData;

    return vo_aet_cmd(vop, interp, argc-1, argv+1);
}


int
vo_rmat_cmd(struct view_obj *vop,
	    Tcl_Interp *interp,
	    int argc,
	    char *argv[])
{
    struct bu_vls vls;
    mat_t rotation;

    if (argc == 1) {
	/* get rotation matrix */
	bu_vls_init(&vls);
	bn_encode_mat(&vls, vop->vo_rotation);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	return TCL_OK;
    } else if (argc == 2) {
	/* set rotation matrix */
	if (bn_decode_mat(rotation, argv[1]) != 16)
	    return TCL_ERROR;

	MAT_COPY(vop->vo_rotation, rotation);
	vo_update(vop, interp, 1);

	return TCL_OK;
    }

    /* compose error message */
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "helplib_alias vo_rmat %s", argv[0]);
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_ERROR;
}


/*
 * Get or set the rotation matrix.
 *
 * Usage:
 * procname
 */
static int
vo_rmat_tcl(ClientData clientData,
	    Tcl_Interp *interp,
	    int argc,
	    char *argv[])
{
    struct view_obj *vop = (struct view_obj *)clientData;

    return vo_rmat_cmd(vop, interp, argc-1, argv+1);
}


void
vo_center(struct view_obj *vop,
	  Tcl_Interp *interp,
	  point_t center)
{
    VSCALE(center, center, vop->vo_local2base);
    MAT_DELTAS_VEC_NEG(vop->vo_center, center);
    vo_update(vop, interp, 1);
}


int
vo_center_cmd(struct view_obj *vop,
	      Tcl_Interp *interp,
	      int argc,
	      char *argv[])
{
    point_t center;
    struct bu_vls vls;


    /* get view center */
    if (argc == 1) {
	MAT_DELTAS_GET_NEG(center, vop->vo_center);
	VSCALE(center, center, vop->vo_base2local);
	bu_vls_init(&vls);
	bn_encode_vect(&vls, center);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	return TCL_OK;
    }

    /* set view center */
    if (argc == 2 || argc == 4) {
	if (argc == 2) {
	    if (bn_decode_vect(center, argv[1]) != 3)
		goto bad;
	} else {
	    if (sscanf(argv[1], "%lf", &center[X]) != 1) {
		Tcl_AppendResult(interp, "vo_center: bad X value - ", argv[1], "\n", (char *)0);
		return TCL_ERROR;
	    }

	    if (sscanf(argv[2], "%lf", &center[Y]) != 1) {
		Tcl_AppendResult(interp, "vo_center: bad Y value - ", argv[2], "\n", (char *)0);
		return TCL_ERROR;
	    }

	    if (sscanf(argv[3], "%lf", &center[Z]) != 1) {
		Tcl_AppendResult(interp, "vo_center: bad Z value - ", argv[3], "\n", (char *)0);
		return TCL_ERROR;
	    }
	}

	vo_center(vop, interp, center);
	return TCL_OK;
    }

bad:
    /* compose error message */
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "helplib_alias vo_center %s", argv[0]);
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_ERROR;
}


/*
 * Get or set the view center.
 *
 * Usage:
 * procname
 */
static int
vo_center_tcl(ClientData clientData,
	      Tcl_Interp *interp,
	      int argc,
	      char *argv[])
{
    struct view_obj *vop = (struct view_obj *)clientData;

    return vo_center_cmd(vop, interp, argc-1, argv+1);
}


int
vo_model2view_cmd(struct view_obj *vop,
		  Tcl_Interp *interp,
		  int argc,
		  char *argv[])
{
    struct bu_vls vls;

    if (argc == 1) {
	bu_vls_init(&vls);
	bn_encode_mat(&vls, vop->vo_model2view);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	return TCL_OK;
    }

    /* compose error message */
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "helplib_alias vo_model2view %s", argv[0]);
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_ERROR;
}


/*
 * Get the model2view matrix.
 *
 * Usage:
 * procname
 */
static int
vo_model2view_tcl(ClientData clientData,
		  Tcl_Interp *interp,
		  int argc,
		  char *argv[])
{
    struct view_obj *vop = (struct view_obj *)clientData;

    return vo_model2view_cmd(vop, interp, argc-1, argv+1);
}


int
vo_pmodel2view_cmd(struct view_obj *vop,
		   Tcl_Interp *interp,
		   int argc,
		   char *argv[])
{
    struct bu_vls vls;

    if (argc == 1) {
	bu_vls_init(&vls);
	bn_encode_mat(&vls, vop->vo_pmodel2view);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	return TCL_OK;
    }

    /* compose error message */
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "helplib_alias vo_pmodel2view %s", argv[0]);
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_ERROR;
}


/*
 * Get the pmodel2view matrix.
 *
 * Usage:
 * procname pmodel2view
 */
static int
vo_pmodel2view_tcl(ClientData clientData,
		   Tcl_Interp *interp,
		   int argc,
		   char *argv[])
{
    struct view_obj *vop = (struct view_obj *)clientData;

    return vo_pmodel2view_cmd(vop, interp, argc-1, argv+1);
}


int
vo_view2model_cmd(struct view_obj *vop,
		  Tcl_Interp *interp,
		  int argc,
		  char *argv[])
{
    struct bu_vls vls;

    if (argc == 1) {
	bu_vls_init(&vls);
	bn_encode_mat(&vls, vop->vo_view2model);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	return TCL_OK;
    }

    /* compose error message */
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "helplib_alias vo_view2model %s", argv[0]);
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_ERROR;
}


/*
 * Usage:
 * procname view2model
 */
static int
vo_view2model_tcl(ClientData clientData,
		  Tcl_Interp *interp,
		  int argc,
		  char *argv[])
{
    struct view_obj *vop = (struct view_obj *)clientData;

    return vo_view2model_cmd(vop, interp, argc-1, argv+1);
}


int
vo_perspective_cmd(struct view_obj *vop,
		   Tcl_Interp *interp,
		   int argc,
		   char *argv[])
{
    struct bu_vls vls;
    fastf_t perspective;

    /* get the perspective angle */
    if (argc == 1) {
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "%g", vop->vo_perspective);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	return TCL_OK;
    }

    /* set the perspective angle */
    if (argc == 2) {
	if (sscanf(argv[1], "%lf", &perspective) != 1) {
	    Tcl_AppendResult(interp, "bad perspective angle - ",
			     argv[1], (char *)NULL);
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
    bu_vls_printf(&vls, "helplib_alias vo_perspective %s", argv[0]);
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_ERROR;
}


/*
 * Get/set the perspective angle.
 *
 * Usage:
 * procname perspective [angle]
 */
static int
vo_perspective_tcl(ClientData clientData,
		   Tcl_Interp *interp,
		   int argc,
		   char *argv[])
{
    struct view_obj *vop = (struct view_obj *)clientData;

    return vo_perspective_cmd(vop, interp, argc-1, argv+1);
}


int
vo_pmat_cmd(struct view_obj *vop,
	    Tcl_Interp *interp,
	    int argc,
	    char *argv[])
{
    struct bu_vls vls;

    if (argc == 1) {
	bu_vls_init(&vls);
	bn_encode_mat(&vls, vop->vo_pmat);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	return TCL_OK;
    }

    /* compose error message */
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "helplib_alias vo_pmat %s", argv[0]);
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_ERROR;
}


/*
 * Get the perspective matrix.
 *
 * Usage:
 * procname pmat
 */
static int
vo_pmat_tcl(ClientData clientData,
	    Tcl_Interp *interp,
	    int argc,
	    char *argv[])
{
    struct view_obj *vop = (struct view_obj *)clientData;

    return vo_pmat_cmd(vop, interp, argc-1, argv+1);
}


int
vo_eye_cmd(struct view_obj *vop,
	   Tcl_Interp *interp,
	   int argc,
	   char *argv[])
{
    point_t eye_model;
    vect_t xlate;
    vect_t new_cent;
    struct bu_vls vls;

    /* get eye */
    if (argc == 1) {
	point_t eye;

	/* calculate eye point */
	VSET(xlate, 0.0, 0.0, 1.0);
	MAT4X3PNT(eye, vop->vo_view2model, xlate);
	VSCALE(eye, eye, vop->vo_base2local);

	bu_vls_init(&vls);
	bn_encode_vect(&vls, eye);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	return TCL_OK;
    }

    if (argc != 2 && argc != 4)
	goto bad;

    if (argc == 2) {
	if (bn_decode_vect(eye_model, argv[1]) != 3)
	    goto bad;
    } else {
	if (sscanf(argv[1], "%lf", &eye_model[X]) != 1) {
	    Tcl_AppendResult(interp, "vo_eye: bad X value - ", argv[1], "\n", (char *)0);
	    return TCL_ERROR;
	}

	if (sscanf(argv[2], "%lf", &eye_model[Y]) != 1) {
	    Tcl_AppendResult(interp, "vo_eye: bad Y value - ", argv[2], "\n", (char *)0);
	    return TCL_ERROR;
	}

	if (sscanf(argv[3], "%lf", &eye_model[Z]) != 1) {
	    Tcl_AppendResult(interp, "vo_eye: bad Z value - ", argv[3], "\n", (char *)0);
	    return TCL_ERROR;
	}
    }

    VSCALE(eye_model, eye_model, vop->vo_local2base);

    /* First step:  put eye at view center (view 0, 0, 0) */
    MAT_DELTAS_VEC_NEG(vop->vo_center, eye_model);
    vo_update(vop, interp, 0);

    /* Second step:  put eye at view 0, 0, 1.
     * For eye to be at 0, 0, 1, the old 0, 0, -1 needs to become 0, 0, 0.
     */
    VSET(xlate, 0.0, 0.0, -1.0);	/* correction factor */
    MAT4X3PNT(new_cent, vop->vo_view2model, xlate);
    MAT_DELTAS_VEC_NEG(vop->vo_center, new_cent);
    vo_update(vop, interp, 1);

    return TCL_OK;

bad:
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "helplib_alias vo_eye %s", argv[0]);
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
}


/*
 * Get/set the eye point.
 *
 * Usage:
 * procname eye [eye_point]
 */
static int
vo_eye_tcl(ClientData clientData,
	   Tcl_Interp *interp,
	   int argc,
	   char *argv[])
{
    struct view_obj *vop = (struct view_obj *)clientData;

    return vo_eye_cmd(vop, interp, argc-1, argv+1);
}


int
vo_eye_pos_cmd(struct view_obj *vop,
	       Tcl_Interp *interp,
	       int argc,
	       char *argv[])
{
    vect_t eye_pos;
    struct bu_vls vls;

    if (argc != 2 && argc != 4)
	goto bad;

    if (argc == 2) {
	if (bn_decode_vect(eye_pos, argv[1]) != 3)
	    goto bad;
    } else {
	if (sscanf(argv[1], "%lf", &eye_pos[X]) != 1) {
	    Tcl_AppendResult(interp, "vo_eye_pos: bad X value - ", argv[1], "\n", (char *)0);
	    return TCL_ERROR;
	}

	if (sscanf(argv[2], "%lf", &eye_pos[Y]) != 1) {
	    Tcl_AppendResult(interp, "vo_eye_pos: bad Y value - ", argv[2], "\n", (char *)0);
	    return TCL_ERROR;
	}

	if (sscanf(argv[3], "%lf", &eye_pos[Z]) != 1) {
	    Tcl_AppendResult(interp, "vo_eye_pos: bad Z value - ", argv[3], "\n", (char *)0);
	    return TCL_ERROR;
	}
    }

    VSCALE(eye_pos, eye_pos, vop->vo_local2base);
    VMOVE(vop->vo_eye_pos, eye_pos);

    /* update perspective matrix */
    vo_mike_persp_mat(vop->vo_pmat, vop->vo_eye_pos);

    /* update all other view related matrices */
    vo_update(vop, interp, 1);

    return TCL_OK;

bad:
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "helplib_alias vo_eye_pos %s", argv[0]);
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
}


/*
 * Set the eye position.
 *
 * Usage:
 * procname eye_pos pos
 */
static int
vo_eye_pos_tcl(ClientData clientData,
	       Tcl_Interp *interp,
	       int argc,
	       char *argv[])
{
    struct view_obj *vop = (struct view_obj *)clientData;

    return vo_eye_pos_cmd(vop, interp, argc-1, argv+1);
}


int
vo_lookat_cmd(struct view_obj *vop,
	      Tcl_Interp *interp,
	      int argc,
	      char *argv[])
{
    point_t look;
    point_t eye;
    point_t tmp;
    point_t new_center;
    vect_t dir;
    fastf_t new_az, new_el;
    struct bu_vls vls;

    if (argc != 2 && argc != 4)
	goto bad;

    if (argc == 2) {
	if (bn_decode_vect(look, argv[1]) != 3)
	    goto bad;
    } else {
	if (sscanf(argv[1], "%lf", &look[X]) != 1) {
	    Tcl_AppendResult(interp, "vo_lookat: bad X value - ", argv[1], "\n", (char *)0);
	    return TCL_ERROR;
	}

	if (sscanf(argv[2], "%lf", &look[Y]) != 1) {
	    Tcl_AppendResult(interp, "vo_lookat: bad Y value - ", argv[2], "\n", (char *)0);
	    return TCL_ERROR;
	}

	if (sscanf(argv[3], "%lf", &look[Z]) != 1) {
	    Tcl_AppendResult(interp, "vo_lookat: bad Z value - ", argv[3], "\n", (char *)0);
	    return TCL_ERROR;
	}
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

bad:
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "helplib_alias vo_lookat %s", argv[0]);
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
}


/*
 * Set look-at point.
 *
 * Usage:
 * procname lookat lookat_point
 */
static int
vo_lookat_tcl(ClientData clientData,
	      Tcl_Interp *interp,
	      int argc,
	      char *argv[])
{
    struct view_obj *vop = (struct view_obj *)clientData;

    return vo_lookat_cmd(vop, interp, argc-1, argv+1);
}


int
vo_orientation_cmd(struct view_obj *vop,
		   Tcl_Interp *interp,
		   int argc,
		   char *argv[])
{
    quat_t quat;
    struct bu_vls vls;

    if (argc != 2 && argc != 5)
	goto bad;

    if (argc == 2) {
	if (bn_decode_quat(quat, argv[1]) != 4)
	    goto bad;
    } else {
	int i;

	for (i = 1; i < 5; ++i)
	    if (sscanf(argv[i], "%lf", &quat[i-1]) != 1)
		goto bad;
    }

    quat_quat2mat(vop->vo_rotation, quat);
    vo_update(vop, interp, 1);

    return TCL_OK;

bad:
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "helplib_alias vo_orient %s", argv[0]);
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
}


/*
 * Usage:
 * procname orient quat
 */
static int
vo_orientation_tcl(ClientData clientData,
		   Tcl_Interp *interp,
		   int argc,
		   char *argv[])
{
    struct view_obj *vop = (struct view_obj *)clientData;

    return vo_orientation_cmd(vop, interp, argc-1, argv+1);
}


int
vo_pov_cmd(struct view_obj *vop,
	   Tcl_Interp *interp,
	   int argc,
	   char *argv[])
{
    vect_t center;
    quat_t quat;
    vect_t eye_pos;
    fastf_t scale;
    fastf_t perspective;

    if (argc != 6) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias vo_pov %s", argv[0]);
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    /***************** Get the arguments *******************/

    if (bn_decode_vect(center, argv[1]) != 3) {
	Tcl_AppendResult(interp, "vo_pov: bad center - ", argv[1], "\n", (char *)0);
	return TCL_ERROR;
    }

    if (bn_decode_quat(quat, argv[2]) != 4) {
	Tcl_AppendResult(interp, "vo_pov: bad quat - ", argv[2], "\n", (char *)0);
	return TCL_ERROR;
    }

    if (sscanf(argv[3], "%lf", &scale) != 1) {
	Tcl_AppendResult(interp, "vo_pov: bad scale - ", argv[3], "\n", (char *)0);
	return TCL_ERROR;
    }

    if (bn_decode_vect(eye_pos, argv[4]) != 3) {
	Tcl_AppendResult(interp, "vo_pov: bad eye position - ", argv[4], "\n", (char *)0);
	return TCL_ERROR;
    }

    if (sscanf(argv[5], "%lf", &perspective) != 1) {
	Tcl_AppendResult(interp, "vo_pov: bad perspective - ", argv[5], "\n", (char *)0);
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
 * procname pov center quat scale eye_pos perspective
 */
static int
vo_pov_tcl(ClientData clientData,
	   Tcl_Interp *interp,
	   int argc,
	   char *argv[])
{
    struct view_obj *vop = (struct view_obj *)clientData;

    return vo_pov_cmd(vop, interp, argc-1, argv+1);
}


int
vo_zoom(struct view_obj *vop,
	Tcl_Interp *interp,
	fastf_t sf)
{
    if (sf <= SMALL_FASTF || INFINITY < sf) {
	Tcl_AppendResult(interp, "vo_zoom - scale factor out of range\n", (char *)0);
	return TCL_ERROR;
    }

    vop->vo_scale /= sf;
    if (vop->vo_scale < RT_MINVIEWSCALE)
	vop->vo_scale = RT_MINVIEWSCALE;
    vop->vo_size = 2.0 * vop->vo_scale;
    vop->vo_invSize = 1.0 / vop->vo_size;
    vo_update(vop, interp, 1);

    return TCL_OK;
}


int
vo_zoom_cmd(struct view_obj *vop,
	    Tcl_Interp *interp,
	    int argc,
	    char *argv[])
{
    fastf_t sf;

    if (argc != 2) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias vo_zoom %s", argv[0]);
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return TCL_ERROR;
    }

    if (sscanf(argv[1], "%lf", &sf) != 1) {
	Tcl_AppendResult(interp, "bad zoom value - ", argv[1], "\n", (char *)0);
	return TCL_ERROR;
    }

    return vo_zoom(vop, interp, sf);
}


/*
 * Usage:
 * procname zoom scale_factor
 */
static int
vo_zoom_tcl(ClientData clientData,
	    Tcl_Interp *interp,
	    int argc,
	    char *argv[])
{
    struct view_obj *vop = (struct view_obj *)clientData;

    return vo_zoom_cmd(vop, interp, argc-1, argv+1);
}


int
vo_units_cmd(struct view_obj *vop,
	     Tcl_Interp *interp,
	     int argc,
	     char *argv[])
{
    struct bu_vls vls;

    /* get units */
    if (argc == 1) {
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "%s", bu_units_string(vop->vo_local2base));
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	return TCL_OK;
    }

    /* set units */
    if (argc == 2) {
	double uval;

	uval = bu_units_conversion(argv[1]);
	if (ZERO(uval)) {
	    bu_vls_init(&vls);
	    bu_vls_printf(&vls, "unrecognized unit type - %s\n", argv[1]);
	    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	    bu_vls_free(&vls);

	    return TCL_ERROR;
	}

	vop->vo_local2base = uval;
	vop->vo_base2local = 1.0 / vop->vo_local2base;

	return TCL_OK;
    }

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "helplib_alias vo_units %s", argv[0]);
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_ERROR;
}


/*
 * Set/get local units.
 *
 * Usage:
 * procname units [unit_spec]
 */
static int
vo_units_tcl(ClientData clientData,
	     Tcl_Interp *interp,
	     int argc,
	     char *argv[])
{
    struct view_obj *vop = (struct view_obj *)clientData;

    return vo_units_cmd(vop, interp, argc-1, argv+1);
}


int
vo_base2local_cmd(struct view_obj *vop,
		  Tcl_Interp *interp,
		  int argc,
		  char *argv[])
{
    struct bu_vls vls;

    bu_vls_init(&vls);

    if (argc != 1) {
	bu_vls_printf(&vls, "helplib_alias vo_base2local %s", argv[0]);
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
 * Get base2local conversion factor.
 *
 * Usage:
 * procname base2local
 */
static int
vo_base2local_tcl(ClientData clientData,
		  Tcl_Interp *interp,
		  int argc,
		  char *argv[])
{
    struct view_obj *vop = (struct view_obj *)clientData;

    return vo_base2local_cmd(vop, interp, argc-1, argv+1);
}


int
vo_local2base_cmd(struct view_obj *vop,
		  Tcl_Interp *interp,
		  int argc,
		  char *argv[])
{
    struct bu_vls vls;

    bu_vls_init(&vls);

    if (argc != 1) {
	bu_vls_printf(&vls, "helplib_alias vo_local2base %s", argv[0]);
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
 * Get local2base conversion factor.
 *
 * Usage:
 * procname local2base
 */
static int
vo_local2base_tcl(ClientData clientData,
		  Tcl_Interp *interp,
		  int argc,
		  char *argv[])
{
    struct view_obj *vop = (struct view_obj *)clientData;

    return vo_local2base_cmd(vop, interp, argc-1, argv+1);
}


int
vo_rot(struct view_obj *vop,
       Tcl_Interp *interp,
       char coord,
       char rotate_about,
       mat_t rmat,
       int (*func)())
{
    mat_t temp1, temp2;

    if (func != (int (*)())0)
	return (*func)(vop, interp, coord, rotate_about, rmat);

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
    if (rotate_about != 'v') {
	point_t rot_pt;
	point_t new_origin;
	mat_t viewchg, viewchginv;
	point_t new_cent_view;
	point_t new_cent_model;

	switch (rotate_about) {
	    case 'e':
		VSET(rot_pt, 0.0, 0.0, 1.0);
		break;
	    case 'k':
		MAT4X3PNT(rot_pt, vop->vo_model2view, vop->vo_keypoint);
		break;
	    case 'm':
		/* rotate around model center (0, 0, 0) */
		VSET(new_origin, 0.0, 0.0, 0.0);
		MAT4X3PNT(rot_pt, vop->vo_model2view, new_origin);
		break;
	    default: {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "vo_rot_tcl: bad rotate_about - %c\n", rotate_about);
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


int
vo_rot_cmd(struct view_obj *vop,
	   Tcl_Interp *interp,
	   int argc,
	   char *argv[],
	   int (*func)())
{
    vect_t rvec;
    mat_t rmat;
    char coord = vop->vo_coord;
    struct bu_vls vls;

    if (argc < 2 || 5 < argc)
	goto bad;

    /* process coord flag */
    if (argv[1][0] == '-' && (argv[1][1] == 'v' || argv[1][1] == 'm') && argv[1][2] == '\0') {
	coord = argv[1][1];
	--argc;
	++argv;
    }

    if (argc != 2 && argc != 4)
	goto bad;

    if (argc == 2) {
	if (bn_decode_vect(rvec, argv[1]) != 3)
	    goto bad;
    } else {
	if (sscanf(argv[1], "%lf", &rvec[X]) != 1) {
	    Tcl_AppendResult(interp, "vo_rot: bad X value - ", argv[1], "\n", (char *)0);
	    return TCL_ERROR;
	}

	if (sscanf(argv[2], "%lf", &rvec[Y]) != 1) {
	    Tcl_AppendResult(interp, "vo_rot: bad Y value - ", argv[2], "\n", (char *)0);
	    return TCL_ERROR;
	}

	if (sscanf(argv[3], "%lf", &rvec[Z]) != 1) {
	    Tcl_AppendResult(interp, "vo_rot: bad Z value - ", argv[3], "\n", (char *)0);
	    return TCL_ERROR;
	}
    }

    VSCALE(rvec, rvec, -1.0);
    bn_mat_angles(rmat, rvec[X], rvec[Y], rvec[Z]);

    return vo_rot(vop, interp, coord, vop->vo_rotate_about, rmat, func);

bad:
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "helplib_alias vo_rot %s", argv[0]);
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_ERROR;
}


/*
 * Rotate the view according to xyz.
 *
 * Usage:
 * procname rot [-v|-m] xyz
 */
static int
vo_rot_tcl(ClientData clientData,
	   Tcl_Interp *interp,
	   int argc,
	   char *argv[])
{
    struct view_obj *vop = (struct view_obj *)clientData;

    return vo_rot_cmd(vop, interp, argc-1, argv+1, (int (*)())0);
}


int
vo_tra(struct view_obj *vop,
       Tcl_Interp *interp,
       char coord,
       vect_t tvec,
       int (*func)())
{
    point_t delta;
    point_t work;
    point_t vc, nvc;

    if (func != (int (*)())0)
	return (*func)(vop, interp, coord, tvec);

    switch (coord) {
	case 'm':
	    VSCALE(delta, tvec, vop->vo_local2base);
	    MAT_DELTAS_GET_NEG(vc, vop->vo_center);
	    break;
	case 'v':
	default:
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


int
vo_tra_cmd(struct view_obj *vop,
	   Tcl_Interp *interp,
	   int argc,
	   char *argv[],
	   int (*func)())
{
    vect_t tvec;
    char coord = vop->vo_coord;
    struct bu_vls vls;

    if (argc < 2 || 5 < argc)
	goto bad;

    /* process coord flag */
    if (argv[1][0] == '-' && (argv[1][1] == 'v' || argv[1][1] == 'm') && argv[1][2] == '\0') {
	coord = argv[1][1];
	--argc;
	++argv;
    }

    if (argc != 2 && argc != 4)
	goto bad;

    if (argc == 2) {
	if (bn_decode_vect(tvec, argv[1]) != 3)
	    goto bad;
    } else {
	if (sscanf(argv[1], "%lf", &tvec[X]) != 1) {
	    Tcl_AppendResult(interp, "vo_tra: bad X value - ", argv[1], "\n", (char *)0);
	    return TCL_ERROR;
	}

	if (sscanf(argv[2], "%lf", &tvec[Y]) != 1) {
	    Tcl_AppendResult(interp, "vo_tra: bad Y value - ", argv[2], "\n", (char *)0);
	    return TCL_ERROR;
	}

	if (sscanf(argv[3], "%lf", &tvec[Z]) != 1) {
	    Tcl_AppendResult(interp, "vo_tra: bad Z value - ", argv[3], "\n", (char *)0);
	    return TCL_ERROR;
	}
    }

    return vo_tra(vop, interp, coord, tvec, func);

bad:
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "helplib_alias vo_tra %s", argv[0]);
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_ERROR;
}


/*
 * Translate the view according to xyz.
 *
 * Usage:
 * procname tra [-v|-m] xyz
 */
static int
vo_tra_tcl(ClientData clientData,
	   Tcl_Interp *interp,
	   int argc,
	   char *argv[])
{
    struct view_obj *vop = (struct view_obj *)clientData;

    return vo_tra_cmd(vop, interp, argc-1, argv+1, (int (*)())0);
}


int
vo_slew(struct view_obj *vop,
	Tcl_Interp *interp,
	vect_t svec)
{
    point_t model_center;

    MAT4X3PNT(model_center, vop->vo_view2model, svec);
    MAT_DELTAS_VEC_NEG(vop->vo_center, model_center);
    vo_update(vop, interp, 1);

    return TCL_OK;
}


int
vo_slew_cmd(struct view_obj *vop,
	    Tcl_Interp *interp,
	    int argc,
	    char *argv[])
{
    struct bu_vls vls;
    vect_t svec;

    if (argc == 2) {
	int n;

	if ((n = bn_decode_vect(svec, argv[1])) != 3) {
	    if (n != 2)
		goto bad;

	    svec[Z] = 0.0;
	}

	return vo_slew(vop, interp, svec);
    }

    if (argc == 3 || argc == 4) {
	if (sscanf(argv[1], "%lf", &svec[X]) != 1) {
	    Tcl_AppendResult(interp, "vo_slew: bad X value - ", argv[1], "\n", (char *)0);
	    return TCL_ERROR;
	}

	if (sscanf(argv[2], "%lf", &svec[Y]) != 1) {
	    Tcl_AppendResult(interp, "vo_slew: bad Y value - ", argv[2], "\n", (char *)0);
	    return TCL_ERROR;
	}

	if (argc == 4) {
	    if (sscanf(argv[3], "%lf", &svec[Z]) != 1) {
		Tcl_AppendResult(interp, "vo_slew: bad Z value - ", argv[3], "\n", (char *)0);
		return TCL_ERROR;
	    }
	} else
	    svec[Z] = 0.0;

	return vo_slew(vop, interp, svec);
    }

bad:
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "helplib_alias vo_slew %s", argv[0]);
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_ERROR;
}


/*
 * Make xyz the new view center.
 *
 * Usage:
 * procname slew xy
 */
static int
vo_slew_tcl(ClientData clientData,
	    Tcl_Interp *interp,
	    int argc,
	    char *argv[])
{
    struct view_obj *vop = (struct view_obj *)clientData;

    return vo_slew_cmd(vop, interp, argc-1, argv+1);
}


int
vo_observer_cmd(struct view_obj *vop,
		Tcl_Interp *interp,
		int argc,
		char *argv[])
{
    if (argc < 2) {
	struct bu_vls vls;

	/* return help message */
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias vo_observer %s", argv[0]);
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    return bu_observer_cmd((ClientData)&vop->vo_observers, interp, argc-1, (const char **)argv+1);
}


/*
 * Attach/detach observers to/from list.
 *
 * Usage:
 * procname observer cmd [args]
 */
static int
vo_observer_tcl(ClientData clientData,
		Tcl_Interp *interp,
		int argc,
		char *argv[])
{
    struct view_obj *vop = (struct view_obj *)clientData;

    return vo_observer_cmd(vop, interp, argc-1, argv+1);
}


int
vo_coord_cmd(struct view_obj *vop,
	     Tcl_Interp *interp,
	     int argc,
	     char *argv[])
{
    struct bu_vls vls;

    /* Get coord */
    if (argc == 1) {
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "%c", vop->vo_coord);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)0);
	bu_vls_free(&vls);
	return TCL_OK;
    }

    /* Set coord */
    if (argc == 2) {
	switch (argv[1][0]) {
	    case 'm':
	    case 'v':
		vop->vo_coord = argv[1][0];
		return TCL_OK;
	}
    }

    /* return help message */
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "helplib_alias vo_coord %s", argv[0]);
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
}


/*
 * Get/set the coordinate system.
 *
 * Usage:
 * procname coord [v|m]
 */
static int
vo_coord_tcl(ClientData clientData,
	     Tcl_Interp *interp,
	     int argc,
	     char *argv[])
{
    struct view_obj *vop = (struct view_obj *)clientData;

    return vo_coord_cmd(vop, interp, argc-1, argv+1);
}


int
vo_rotate_about_cmd(struct view_obj *vop,
		    Tcl_Interp *interp,
		    int argc,
		    char *argv[])
{
    struct bu_vls vls;

    /* Get rotate_about */
    if (argc == 1) {
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "%c", vop->vo_rotate_about);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)0);
	bu_vls_free(&vls);
	return TCL_OK;
    }

    /* Set rotate_about */
    if (argc == 2 && argv[1][1] == '\0') {
	switch (argv[1][0]) {
	    case 'e':
	    case 'k':
	    case 'm':
	    case 'v':
		vop->vo_rotate_about = argv[1][0];
		return TCL_OK;
	}
    }

    /* return help message */
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "helplib_alias vo_rotate_about %s", argv[0]);
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
}


/*
 * Get/set the rotate about point.
 *
 * Usage:
 * procname rotate_about [e|k|m|v]
 */
static int
vo_rotate_about_tcl(ClientData clientData,
		    Tcl_Interp *interp,
		    int argc,
		    char *argv[])
{
    struct view_obj *vop = (struct view_obj *)clientData;

    return vo_rotate_about_cmd(vop, interp, argc-1, argv+1);
}


int
vo_keypoint_cmd(struct view_obj *vop,
		Tcl_Interp *interp,
		int argc,
		char *argv[])
{
    struct bu_vls vls;
    vect_t tvec;

    /* Get the keypoint */
    if (argc == 1) {
	bu_vls_init(&vls);
	VSCALE(tvec, vop->vo_keypoint, vop->vo_base2local);
	bn_encode_vect(&vls, tvec);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)0);
	bu_vls_free(&vls);
	return TCL_OK;
    }

    /* Set the keypoint */
    if (argc == 2) {
	if (bn_decode_vect(tvec, argv[1]) != 3)
	    goto bad;
    } else if (argc == 4) {
	if (sscanf(argv[1], "%lf", &tvec[X]) != 1) {
	    Tcl_AppendResult(interp, "vo_keypoint: bad X value - ", argv[1], "\n", (char *)0);
	    return TCL_ERROR;
	}

	if (sscanf(argv[2], "%lf", &tvec[Y]) != 1) {
	    Tcl_AppendResult(interp, "vo_keypoint: bad Y value - ", argv[2], "\n", (char *)0);
	    return TCL_ERROR;
	}

	if (sscanf(argv[3], "%lf", &tvec[Z]) != 1) {
	    Tcl_AppendResult(interp, "vo_keypoint: bad Z value - ", argv[3], "\n", (char *)0);
	    return TCL_ERROR;
	}
    }

    VSCALE(vop->vo_keypoint, tvec, vop->vo_local2base);
    return TCL_OK;

bad:
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "helplib_alias vo_keypoint %s", argv[0]);
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
}


/*
 * Get/set the keypoint.
 *
 * Usage:
 * procname keypoint [point]
 */
static int
vo_keypoint_tcl(ClientData clientData,
		Tcl_Interp *interp,
		int argc,
		char *argv[])
{
    struct view_obj *vop = (struct view_obj *)clientData;

    return vo_keypoint_cmd(vop, interp, argc-1, argv+1);
}


/*
 * V O _ S E T V I E W
 *
 * Set the view.  Angles are DOUBLES, in degrees.
 *
 * Given that viewvec = scale . rotate . (xlate to view center) . modelvec,
 * we just replace the rotation matrix.
 * (This assumes rotation around the view center).
 */
void
vo_setview(struct view_obj *vop,
	   Tcl_Interp *interp,
	   vect_t rvec)		/* DOUBLE angles, in degrees */
{
    bn_mat_angles(vop->vo_rotation, rvec[X], rvec[Y], rvec[Z]);
    vo_update(vop, interp, 1);
}


int
vo_setview_cmd(struct view_obj *vop,
	       Tcl_Interp *interp,
	       int argc,
	       char *argv[])
{
    vect_t rvec;
    struct bu_vls vls;

    if (argc != 2 && argc != 4)
	goto bad;

    if (argc == 2) {
	if (bn_decode_vect(rvec, argv[1]) != 3)
	    goto bad;
    } else {
	if (sscanf(argv[1], "%lf", &rvec[X]) != 1) {
	    Tcl_AppendResult(interp, "vo_setview_cmd: bad X value - ", argv[1], "\n", (char *)0);
	    return TCL_ERROR;
	}

	if (sscanf(argv[2], "%lf", &rvec[Y]) != 1) {
	    Tcl_AppendResult(interp, "vo_setview_cmd: bad Y value - ", argv[2], "\n", (char *)0);
	    return TCL_ERROR;
	}

	if (sscanf(argv[3], "%lf", &rvec[Z]) != 1) {
	    Tcl_AppendResult(interp, "vo_setview_cmd: bad Z value - ", argv[3], "\n", (char *)0);
	    return TCL_ERROR;
	}
    }

    vo_setview(vop, interp, rvec);
    return TCL_OK;

bad:
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "helplib_alias vo_setview %s", argv[0]);
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
}


/*
 * Usage:
 * procname setview x y z
 */
static int
vo_setview_tcl(ClientData clientData,
	       Tcl_Interp *interp,
	       int argc,
	       char *argv[])
{
    struct view_obj *vop = (struct view_obj *)clientData;

    return vo_setview_cmd(vop, interp, argc-1, argv+1);
}


int
vo_arot_cmd(struct view_obj *vop,
	    Tcl_Interp *interp,
	    int argc,
	    char *argv[],
	    int (*func)())
{
    mat_t newrot;
    point_t pt;
    vect_t axis;
    fastf_t angle;

    if (argc != 5) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias vo_arot %s", argv[0]);
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (sscanf(argv[1], "%lf", &axis[X]) != 1) {
	Tcl_AppendResult(interp, "vo_arot: bad X value - ", argv[1], "\n", (char *)0);
	return TCL_ERROR;
    }

    if (sscanf(argv[2], "%lf", &axis[Y]) != 1) {
	Tcl_AppendResult(interp, "vo_arot: bad Y value - ", argv[2], "\n", (char *)0);
	return TCL_ERROR;
    }

    if (sscanf(argv[3], "%lf", &axis[Z]) != 1) {
	Tcl_AppendResult(interp, "vo_arot: bad Z value - ", argv[3], "\n", (char *)0);
	return TCL_ERROR;
    }

    if (sscanf(argv[4], "%lf", &angle) != 1) {
	Tcl_AppendResult(interp, "vo_arot: bad angle - ", argv[4], "\n", (char *)0);
	return TCL_ERROR;
    }

    VSETALL(pt, 0.0);
    VUNITIZE(axis);

    bn_mat_arb_rot(newrot, pt, axis, angle*bn_degtorad);

    return vo_rot(vop, interp, vop->vo_coord, vop->vo_rotate_about, newrot, func);
}


/*
 * Usage:
 * procname arot x y z angle
 */
static int
vo_arot_tcl(ClientData clientData,
	    Tcl_Interp *interp,
	    int argc,
	    char *argv[])
{
    struct view_obj *vop = (struct view_obj *)clientData;

    return vo_arot_cmd(vop, interp, argc-1, argv+1, (int (*)())0);
}


int
vo_vrot_cmd(struct view_obj *vop,
	    Tcl_Interp *interp,
	    int argc,
	    char *argv[])
{
    vect_t rvec;
    mat_t rmat;
    struct bu_vls vls;

    if (argc != 2 && argc != 4)
	goto bad;

    if (argc == 2) {
	if (bn_decode_vect(rvec, argv[1]) != 3)
	    goto bad;
    } else {
	if (sscanf(argv[1], "%lf", &rvec[X]) < 1) {
	    Tcl_AppendResult(interp, "vo_vrot: bad X value - ", argv[1], "\n", (char *)0);
	    return TCL_ERROR;
	}

	if (sscanf(argv[2], "%lf", &rvec[Y]) < 1) {
	    Tcl_AppendResult(interp, "vo_vrot: bad Y value - ", argv[2], "\n", (char *)0);
	    return TCL_ERROR;
	}

	if (sscanf(argv[3], "%lf", &rvec[Z]) < 1) {
	    Tcl_AppendResult(interp, "vo_vrot: bad Z value - ", argv[3], "\n", (char *)0);
	    return TCL_ERROR;
	}
    }

    VSCALE(rvec, rvec, -1.0);
    bn_mat_angles(rmat, rvec[X], rvec[Y], rvec[Z]);

    return vo_rot(vop, interp, 'v', vop->vo_rotate_about, rmat, (int (*)())0);

bad:
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "helplib_alias vo_vrot %s", argv[0]);
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
}


/*
 * Usage:
 * procname vrot x y z
 */
static int
vo_vrot_tcl(ClientData clientData,
	    Tcl_Interp *interp,
	    int argc,
	    char *argv[])
{
    struct view_obj *vop = (struct view_obj *)clientData;

    return vo_vrot_cmd(vop, interp, argc-1, argv+1);
}


int
vo_mrot_cmd(struct view_obj *vop,
	    Tcl_Interp *interp,
	    int argc,
	    char *argv[],
	    int (*func)())
{
    vect_t rvec;
    mat_t rmat;
    struct bu_vls vls;

    if (argc != 2 && argc != 4)
	goto bad;

    if (argc == 2) {
	if (bn_decode_vect(rvec, argv[1]) != 3)
	    goto bad;
    } else {
	if (sscanf(argv[1], "%lf", &rvec[X]) < 1) {
	    Tcl_AppendResult(interp, "vo_mrot: bad X value - ", argv[1], "\n", (char *)0);
	    return TCL_ERROR;
	}

	if (sscanf(argv[2], "%lf", &rvec[Y]) < 1) {
	    Tcl_AppendResult(interp, "vo_mrot: bad Y value - ", argv[2], "\n", (char *)0);
	    return TCL_ERROR;
	}

	if (sscanf(argv[3], "%lf", &rvec[Z]) < 1) {
	    Tcl_AppendResult(interp, "vo_mrot: bad Z value - ", argv[3], "\n", (char *)0);
	    return TCL_ERROR;
	}
    }

    VSCALE(rvec, rvec, -1.0);
    bn_mat_angles(rmat, rvec[X], rvec[Y], rvec[Z]);

    return vo_rot(vop, interp, 'm', vop->vo_rotate_about, rmat, func);

bad:
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "helplib_alias vo_mrot %s", argv[0]);
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
}


/*
 * Usage:
 * procname mrot x y z
 */
static int
vo_mrot_tcl(ClientData clientData,
	    Tcl_Interp *interp,
	    int argc,
	    char *argv[])
{
    struct view_obj *vop = (struct view_obj *)clientData;

    return vo_mrot_cmd(vop, interp, argc-1, argv+1, (int (*)())0);
}


int
vo_mrotPoint_cmd(struct view_obj *vop,
		 Tcl_Interp *interp,
		 int argc,
		 char *argv[])
{
    struct bu_vls vls;

    /* Parse the incoming point */
    if (argc == 2 || argc == 4) {
	point_t viewPt;
	point_t modelPt;
	mat_t invRot;

	if (argc == 2) {
	    if (bn_decode_vect(viewPt, argv[1]) != 3)
		goto bad;
	} else {
	    if (sscanf(argv[1], "%lf", &viewPt[X]) != 1) {
		Tcl_AppendResult(interp,
				 "vo_mrotPoint: bad X value - ",
				 argv[1],
				 "\n",
				 (char *)0);
		return TCL_ERROR;
	    }

	    if (sscanf(argv[2], "%lf", &viewPt[Y]) != 1) {
		Tcl_AppendResult(interp,
				 "vo_mrotPoint: bad Y value - ",
				 argv[2],
				 "\n",
				 (char *)0);
		return TCL_ERROR;
	    }

	    if (sscanf(argv[3], "%lf", &viewPt[Z]) != 1) {
		Tcl_AppendResult(interp,
				 "vo_mrotPoint: bad Z value - ",
				 argv[3],
				 "\n",
				 (char *)0);
		return TCL_ERROR;
	    }
	}

	bu_vls_init(&vls);
	bn_mat_inv(invRot, vop->vo_rotation);
	MAT4X3PNT(modelPt, invRot, viewPt);
	bn_encode_vect(&vls, modelPt);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	return TCL_OK;
    }

bad:
    /* compose error message */
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "helplib_alias vo_mrotPoint %s", argv[0]);
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_ERROR;
}


/*
 * Convert view point to a model point (rotation only).
 *
 * Usage:
 * procname mrotPoint vx vy vz
 */
static int
vo_mrotPoint_tcl(ClientData clientData,
		 Tcl_Interp *interp,
		 int argc,
		 char *argv[])
{
    struct view_obj *vop = (struct view_obj *)clientData;

    return vo_mrotPoint_cmd(vop, interp, argc-1, argv+1);
}


int
vo_m2vPoint_cmd(struct view_obj *vop,
		Tcl_Interp *interp,
		int argc,
		char *argv[])
{
    struct bu_vls vls;

    /* Parse the incoming point */
    if (argc == 2 || argc == 4) {
	point_t viewPt;
	point_t modelPt;

	if (argc == 2) {
	    if (bn_decode_vect(viewPt, argv[1]) != 3)
		goto bad;
	} else {
	    if (sscanf(argv[1], "%lf", &modelPt[X]) != 1) {
		Tcl_AppendResult(interp,
				 "vo_m2vPoint: bad X value - ",
				 argv[1],
				 "\n",
				 (char *)0);
		return TCL_ERROR;
	    }

	    if (sscanf(argv[2], "%lf", &modelPt[Y]) != 1) {
		Tcl_AppendResult(interp,
				 "vo_m2vPoint: bad Y value - ",
				 argv[2],
				 "\n",
				 (char *)0);
		return TCL_ERROR;
	    }

	    if (sscanf(argv[3], "%lf", &modelPt[Z]) != 1) {
		Tcl_AppendResult(interp,
				 "vo_m2vPoint: bad Z value - ",
				 argv[3],
				 "\n",
				 (char *)0);
		return TCL_ERROR;
	    }
	}

	bu_vls_init(&vls);
	MAT4X3PNT(viewPt, vop->vo_model2view, modelPt);
	bn_encode_vect(&vls, viewPt);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	return TCL_OK;
    }

bad:
    /* compose error message */
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "helplib_alias vo_m2vPoint %s", argv[0]);
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_ERROR;
}


/*
 * Convert model point to a view point.
 *
 * Usage:
 * procname m2vPoint vx vy vz
 */
static int
vo_m2vPoint_tcl(ClientData clientData,
		Tcl_Interp *interp,
		int argc,
		char *argv[])
{
    struct view_obj *vop = (struct view_obj *)clientData;

    return vo_m2vPoint_cmd(vop, interp, argc-1, argv+1);
}


int
vo_v2mPoint_cmd(struct view_obj *vop,
		Tcl_Interp *interp,
		int argc,
		char *argv[])
{
    struct bu_vls vls;

    /* Parse the incoming point */
    if (argc == 2 || argc == 4) {
	point_t viewPt;
	point_t modelPt;

	if (argc == 2) {
	    if (bn_decode_vect(viewPt, argv[1]) != 3)
		goto bad;
	} else {
	    if (sscanf(argv[1], "%lf", &viewPt[X]) != 1) {
		Tcl_AppendResult(interp,
				 "vo_v2mPoint: bad X value - ",
				 argv[1],
				 "\n",
				 (char *)0);
		return TCL_ERROR;
	    }

	    if (sscanf(argv[2], "%lf", &viewPt[Y]) != 1) {
		Tcl_AppendResult(interp,
				 "vo_v2mPoint: bad Y value - ",
				 argv[2],
				 "\n",
				 (char *)0);
		return TCL_ERROR;
	    }

	    if (sscanf(argv[3], "%lf", &viewPt[Z]) != 1) {
		Tcl_AppendResult(interp,
				 "vo_v2mPoint: bad Z value - ",
				 argv[3],
				 "\n",
				 (char *)0);
		return TCL_ERROR;
	    }
	}

	bu_vls_init(&vls);
	MAT4X3PNT(modelPt, vop->vo_view2model, viewPt);
	bn_encode_vect(&vls, modelPt);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	return TCL_OK;
    }

bad:
    /* compose error message */
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "helplib_alias vo_v2mPoint %s", argv[0]);
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_ERROR;
}


/*
 * Convert view point to a model point.
 *
 * Usage:
 * procname v2mPoint vx vy vz
 */
static int
vo_v2mPoint_tcl(ClientData clientData,
		Tcl_Interp *interp,
		int argc,
		char *argv[])
{
    struct view_obj *vop = (struct view_obj *)clientData;

    return vo_v2mPoint_cmd(vop, interp, argc-1, argv+1);
}


int
vo_sca(struct view_obj *vop,
       Tcl_Interp *interp,
       fastf_t sf,
       int (*func)())
{
    if (func != (int (*)())0)
	return (*func)(vop, interp, sf);

    if (sf <= SMALL_FASTF || INFINITY < sf)
	return TCL_OK;

    vop->vo_scale *= sf;
    if (vop->vo_scale < RT_MINVIEWSIZE)
	vop->vo_scale = RT_MINVIEWSIZE;
    vop->vo_size = 2.0 * vop->vo_scale;
    vop->vo_invSize = 1.0 / vop->vo_size;
    vo_update(vop, interp, 1);
    return TCL_OK;
}


int
vo_sca_cmd(struct view_obj *vop,
	   Tcl_Interp *interp,
	   int argc,
	   char *argv[],
	   int (*func)())
{
    fastf_t sf;

    if (argc != 2) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias vo_sca %s", argv[0]);
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (sscanf(argv[1], "%lf", &sf) != 1) {
	Tcl_AppendResult(interp, "vo_sca: bad scale factor - ", argv[1], "\n", (char *)0);
	return TCL_ERROR;
    }

    return vo_sca(vop, interp, sf, func);
}


/*
 * Usage:
 * procname sca [sf]
 */
static int
vo_sca_tcl(ClientData clientData,
	   Tcl_Interp *interp,
	   int argc,
	   char *argv[])
{
    struct view_obj *vop = (struct view_obj *)clientData;

    return vo_sca_cmd(vop, interp, argc-1, argv+1, (int (*)())0);
}


int
vo_viewDir_cmd(struct view_obj *vop,
	       Tcl_Interp *interp,
	       int argc,
	       char *argv[])
{
    vect_t view;
    vect_t model;
    mat_t invRot;
    struct bu_vls vls;
    int iflag = 0;

    if (2 < argc) {
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias vo_viewDir %s", argv[0]);
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    /* Look for -i option */
    if (argc == 2 &&
	argv[1][0] == '-' &&
	argv[1][1] == 'i' &&
	argv[1][2] == '\0')
	iflag = 1;

    if (iflag) {
	VSET(view, 0.0, 0.0, -1.0);
    } else {
	VSET(view, 0.0, 0.0, 1.0);
    }

    bn_mat_inv(invRot, vop->vo_rotation);
    MAT4X3PNT(model, invRot, view);

    bu_vls_init(&vls);
    bn_encode_vect(&vls, model);
    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);

    return TCL_OK;
}


/*
 * Usage:
 * procname viewDir[-i]
 */
static int
vo_viewDir_tcl(ClientData clientData,
	       Tcl_Interp *interp,
	       int argc,
	       char *argv[])
{
    struct view_obj *vop = (struct view_obj *)clientData;

    return vo_viewDir_cmd(vop, interp, argc-1, argv+1);
}


/* skeleton functions for view_obj methods */
int
vo_ae2dir_cmd(struct view_obj *UNUSED(vop), Tcl_Interp *interp, int argc, char *argv[])
{
    fastf_t az, el;
    vect_t dir;
    int iflag;
    struct bu_vls vls;


    if (argc < 3 || 4 < argc) {
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias vo_ae2dir %s", argv[0]);
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    /* Look for -i option */
    if (argc == 4) {
	if (argv[1][0] != '-' ||
	    argv[1][1] != 'i' ||
	    argv[1][2] != '\0') {
	    bu_vls_init(&vls);
	    bu_vls_printf(&vls, "helplib_alias vo_ae2dir %s", argv[0]);
	    Tcl_Eval(interp, bu_vls_addr(&vls));
	    bu_vls_free(&vls);
	    return TCL_ERROR;
	}

	if (sscanf(argv[2], "%lf", &az) != 1 ||
	    sscanf(argv[3], "%lf", &el) != 1) {
	    bu_vls_init(&vls);
	    bu_vls_printf(&vls, "helplib_alias vo_ae2dir %s", argv[0]);
	    Tcl_Eval(interp, bu_vls_addr(&vls));
	    bu_vls_free(&vls);
	    return TCL_ERROR;
	}

	iflag = 1;
    } else {
	if (sscanf(argv[1], "%lf", &az) != 1 ||
	    sscanf(argv[2], "%lf", &el) != 1) {
	    bu_vls_init(&vls);
	    bu_vls_printf(&vls, "helplib_alias vo_ae2dir %s", argv[0]);
	    Tcl_Eval(interp, bu_vls_addr(&vls));
	    bu_vls_free(&vls);
	    return TCL_ERROR;
	}

	iflag = 0;
    }

    az *= bn_degtorad;
    el *= bn_degtorad;
    V3DIR_FROM_AZEL(dir, az, el);

    if (iflag)
	VSCALE(dir, dir, -1);

    bu_vls_init(&vls);
    bn_encode_vect(&vls, dir);
    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);

    return TCL_OK;
}


/*
 * Usage:
 * procname ae2dir [-i] az el
 */
static int
vo_ae2dir_tcl(ClientData clientData,
	      Tcl_Interp *interp,
	      int argc,
	      char *argv[])
{
    struct view_obj *vop = (struct view_obj *)clientData;

    return vo_ae2dir_cmd(vop, interp, argc-1, argv+1);
}


/**
 * guts to the dir2ae command
 */
int
vo_dir2ae_cmd(struct view_obj *UNUSED(vop), Tcl_Interp *interp, int argc, char *argv[])
{
    fastf_t az, el;
    vect_t dir;
    int iflag = 0;
    struct bu_vls vls;

    if (argc < 4 || 5 < argc) {
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias vo_dir2ae %s", argv[0]);
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    /* Look for -i option */
    if (argc == 5) {
	if (argv[1][0] != '-' ||
	    argv[1][1] != 'i' ||
	    argv[1][2] != '\0') {
	    bu_vls_init(&vls);
	    bu_vls_printf(&vls, "helplib_alias vo_dir2ae %s", argv[0]);
	    Tcl_Eval(interp, bu_vls_addr(&vls));
	    bu_vls_free(&vls);
	    return TCL_ERROR;
	}
	iflag = 1;
	argc--; argv++;
    }

    if (sscanf(argv[1], "%lf", &dir[X]) != 1 ||
	sscanf(argv[2], "%lf", &dir[Y]) != 1 ||
	sscanf(argv[3], "%lf", &dir[Z]) != 1) {
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib_alias vo_dir2ae %s", argv[0]);
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    AZEL_FROM_V3DIR(az, el, dir);

    if (iflag)
	VSCALE(dir, dir, -1);

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "%lf %lf", az, el);
    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);

    return TCL_OK;
}


/**
 * Usage:
 * procname dir2ae [-i] x y z
 */
static int
vo_dir2ae_tcl(ClientData clientData,
	      Tcl_Interp *interp,
	      int argc,
	      char *argv[])
{
    struct view_obj *vop = (struct view_obj *)clientData;

    return vo_dir2ae_cmd(vop, interp, argc-1, argv+1);
}


/****************** Utility Routines ********************/
void
vo_update(struct view_obj *vop,
	  Tcl_Interp *interp,
	  int oflag)
{
    vect_t work, work1;
    vect_t temp, temp1;

    /* set up the view matrix */
    bn_mat_mul(vop->vo_model2view, vop->vo_rotation, vop->vo_center);
    vop->vo_model2view[15] = vop->vo_scale;

    /* XXX validation needs to occur before here to make sure we're
     * not attempting to invert a singular matrix.
     */
    bn_mat_inv(vop->vo_view2model, vop->vo_model2view);

    /* Find current azimuth, elevation, and twist angles */
    VSET(work, 0.0, 0.0, 1.0);       /* view z-direction */
    MAT4X3VEC(temp, vop->vo_view2model, work);
    VSET(work1, 1.0, 0.0, 0.0);      /* view x-direction */
    MAT4X3VEC(temp1, vop->vo_view2model, work1);

    /* calculate angles using accuracy of 0.005, since display
     * shows 2 digits right of decimal point */
    bn_aet_vec(&vop->vo_aet[0],
	       &vop->vo_aet[1],
	       &vop->vo_aet[2],
	       temp, temp1, (fastf_t)0.005);

    /* Force azimuth range to be [0, 360] */
    if ((NEAR_EQUAL(vop->vo_aet[1], 90.0, (fastf_t)0.005) ||
	 NEAR_EQUAL(vop->vo_aet[1], -90.0, (fastf_t)0.005)) &&
	vop->vo_aet[0] < 0 &&
	!NEAR_ZERO(vop->vo_aet[0], (fastf_t)0.005))
	vop->vo_aet[0] += 360.0;
    else if (NEAR_ZERO(vop->vo_aet[0], (fastf_t)0.005))
	vop->vo_aet[0] = 0.0;

    /* apply the perspective angle to model2view */
    bn_mat_mul(vop->vo_pmodel2view, vop->vo_pmat, vop->vo_model2view);

    if (vop->vo_callback)
	(*vop->vo_callback)(vop->vo_clientData, vop);
    else if (oflag && interp != (Tcl_Interp *)NULL)
	bu_observer_notify(interp, &vop->vo_observers, bu_vls_addr(&vop->vo_name));
}


void
vo_mat_aet(struct view_obj *vop)
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


/**
 * P E R S P _ M A T
 *
 * Compute a perspective matrix for a right-handed coordinate system.
 * Reference: SGI Graphics Reference Appendix f
 *
 * (Note: SGI is left-handed, but the fix is done in the Display
 * Manager).
 */
static void
vo_persp_mat(mat_t m,
	     fastf_t fovy,
	     fastf_t aspect,
	     fastf_t near1,
	     fastf_t far1,
	     fastf_t backoff)
{
    mat_t m2, tran;

    fovy *= 3.1415926535/180.0;

    MAT_IDN(m2);
    m2[5] = cos(fovy/2.0) / sin(fovy/2.0);
    m2[0] = m2[5]/aspect;
    m2[10] = (far1+near1) / (far1-near1);
    m2[11] = 2*far1*near1 / (far1-near1);	/* This should be negative */

    m2[14] = -1;		/* XXX This should be positive */
    m2[15] = 0;

    /* Move eye to origin, then apply perspective */
    MAT_IDN(tran);
    tran[11] = -backoff;
    bn_mat_mul(m, m2, tran);
}


/**
 * Create a perspective matrix that transforms the +/1 viewing cube,
 * with the acutal eye position (not at Z=+1) specified in viewing
 * coords, into a related space where the eye has been sheared onto
 * the Z axis and repositioned at Z=(0, 0, 1), with the same
 * perspective field of view as before.
 *
 * The Zbuffer clips off stuff with negative Z values.
 *
 * pmat = persp * xlate * shear
 */
static void
vo_mike_persp_mat(mat_t pmat,
		  const point_t eye)
{
    mat_t shear;
    mat_t persp;
    mat_t xlate;
    mat_t t1, t2;
    point_t sheared_eye;

    if (eye[Z] <= SMALL) {
	VPRINT("mike_persp_mat(): ERROR, z<0, eye", eye);
	return;
    }

    /* Shear "eye" to +Z axis */
    MAT_IDN(shear);
    shear[2] = -eye[X]/eye[Z];
    shear[6] = -eye[Y]/eye[Z];

    MAT4X3VEC(sheared_eye, shear, eye);
    if (!NEAR_ZERO(sheared_eye[X], .01) || !NEAR_ZERO(sheared_eye[Y], .01)) {
	VPRINT("ERROR sheared_eye", sheared_eye);
	return;
    }

    /* Translate along +Z axis to put sheared_eye at (0, 0, 1). */
    MAT_IDN(xlate);
    /* XXX should I use MAT_DELTAS_VEC_NEG()?  X and Y should be 0 now */
    MAT_DELTAS(xlate, 0, 0, 1-sheared_eye[Z]);

    /* Build perspective matrix inline, substituting fov=2*atan(1, Z) */
    MAT_IDN(persp);
    /* From page 492 of Graphics Gems */
    persp[0] = sheared_eye[Z];	/* scaling: fov aspect term */
    persp[5] = sheared_eye[Z];	/* scaling: determines fov */

    /* From page 158 of Rogers Mathematical Elements */
    /* Z center of projection at Z=+1, r=-1/1 */
    persp[14] = -1;

    bn_mat_mul(t1, xlate, shear);
    bn_mat_mul(t2, persp, t1);

    /* Now, move eye from Z=1 to Z=0, for clipping purposes */
    MAT_DELTAS(xlate, 0, 0, -1);

    bn_mat_mul(pmat, xlate, t2);
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
