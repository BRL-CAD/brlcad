/*                      C H G M O D E L . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2008 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file chgmodel.c
 *
 * This module contains functions which change particulars of the
 * model, generally on a single solid or combination.
 * Changes to the tree structure of the model are done in chgtree.c
 *
 */

#include "common.h"

#include <signal.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "nmg.h"
#include "nurb.h"
#include "rtgeom.h"
#include "ged.h"
#include "wdb.h"

#include "./mged.h"
#include "./mged_solid.h"
#include "./mged_dm.h"
#include "./sedit.h"
#include "./cmd.h"


extern struct bn_tol mged_tol;

void set_tran();
void aexists(char *name);

int newedge;		/* new edge for arb editing */


/* Add/modify item and air codes of a region */
/* Format: item region item <air>	*/
int
f_itemair(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    Tcl_DString ds;
    int ret;
    
    CHECK_DBI_NULL;
    CHECK_READ_ONLY;

    ret = ged_item(wdbp, argc, argv);

    /* Convert to Tcl codes */
    if (ret == GED_OK)
	ret = TCL_OK;
    else
	ret = TCL_ERROR;

    Tcl_DStringInit(&ds);
    Tcl_DStringAppend(&ds, bu_vls_addr(&wdbp->wdb_result_str), -1);
    Tcl_DStringResult(interp, &ds);

    return ret;
}

/* Modify material information */
/* Usage:  mater region_name shader r g b inherit */
int
f_mater(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    register struct directory *dp;
    int r=0, g=0, b=0;
    int skip_args = 0;
    char inherit;
    struct rt_db_internal	intern;
    struct rt_comb_internal	*comb;

    CHECK_DBI_NULL;
    CHECK_READ_ONLY;

    if (argc < 2 || 8 < argc) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "help mater");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if ( (dp = db_lookup( dbip,  argv[1], LOOKUP_NOISY )) == DIR_NULL )
	return TCL_ERROR;
    if ( (dp->d_flags & DIR_COMB) == 0 )  {
	Tcl_AppendResult(interp, dp->d_namep, ": not a combination\n", (char *)NULL);
	return TCL_ERROR;
    }

    if ( rt_db_get_internal( &intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource ) < 0 )  {
	TCL_READ_ERR_return;
    }
    comb = (struct rt_comb_internal *)intern.idb_ptr;
    RT_CK_COMB(comb);

    if ( argc >= 3 )  {
	if ( strncmp( argv[2], "del", 3 ) != 0 )  {
	    /* Material */
	    bu_vls_trunc( &comb->shader, 0 );
	    if ( bu_shader_to_tcl_list( argv[2], &comb->shader ))
	    {
		Tcl_AppendResult(interp, "Problem with shader string: ", argv[2], (char *)NULL );
		return TCL_ERROR;
	    }
	} else {
	    bu_vls_free( &comb->shader );
	}
    } else {
	/* Shader */
	struct bu_vls tmp_vls;

	bu_vls_init( &tmp_vls );
	if ( bu_vls_strlen( &comb->shader ) )
	{
	    if ( bu_shader_to_key_eq( bu_vls_addr(&comb->shader), &tmp_vls ) )
	    {
		Tcl_AppendResult(interp, "Problem with on disk shader string: ", bu_vls_addr(&comb->shader), (char *)NULL );
		bu_vls_free( &tmp_vls );
		return TCL_ERROR;
	    }
	}
	curr_cmd_list->cl_quote_string = 1;
	Tcl_AppendResult(interp, "Shader = ", bu_vls_addr(&tmp_vls),
			 "\n", MORE_ARGS_STR,
			 "Shader?  ('del' to delete, CR to skip) ", (char *)NULL);

	if ( bu_vls_strlen( &comb->shader ) == 0 )
	    bu_vls_printf(&curr_cmd_list->cl_more_default, "del");
	else
	    bu_vls_printf(&curr_cmd_list->cl_more_default, "\"%S\"", &tmp_vls );

	bu_vls_free( &tmp_vls );

	goto fail;
    }

    if (argc >= 4) {
	if ( strncmp(argv[3], "del", 3) == 0 ) {
	    /* leave color as is */
	    comb->rgb_valid = 0;
	    skip_args = 2;
	} else if (argc < 6) {
	    /* prompt for color */
	    goto color_prompt;
	} else {
	    /* change color */
	    sscanf(argv[3], "%d", &r);
	    sscanf(argv[4], "%d", &g);
	    sscanf(argv[5], "%d", &b);
	    comb->rgb[0] = r;
	    comb->rgb[1] = g;
	    comb->rgb[2] = b;
	    comb->rgb_valid = 1;
	}
    } else {
	/* Color */
    color_prompt:
	if ( comb->rgb_valid ) {
	    struct bu_vls tmp_vls;

	    bu_vls_init(&tmp_vls);
	    bu_vls_printf(&tmp_vls, "Color = %d %d %d\n",
			  comb->rgb[0], comb->rgb[1], comb->rgb[2] );
	    Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	    bu_vls_free(&tmp_vls);

	    bu_vls_printf(&curr_cmd_list->cl_more_default, "%d %d %d",
			  comb->rgb[0],
			  comb->rgb[1],
			  comb->rgb[2] );
	} else {
	    Tcl_AppendResult(interp, "Color = (No color specified)\n", (char *)NULL);
	    bu_vls_printf(&curr_cmd_list->cl_more_default, "del");
	}

	Tcl_AppendResult(interp, MORE_ARGS_STR,
			 "Color R G B (0..255)? ('del' to delete, CR to skip) ", (char *)NULL);
	goto fail;
    }

    if (argc >= 7 - skip_args) {
	inherit = *argv[6 - skip_args];
    } else {
	/* Inherit */
	switch ( comb->inherit )  {
	    case 0:
		Tcl_AppendResult(interp, "Inherit = 0:  lower nodes (towards leaves) override\n",
				 (char *)NULL);
		break;
	    default:
		Tcl_AppendResult(interp, "Inherit = 1:  higher nodes (towards root) override\n",
				 (char *)NULL);
		break;
	}

	Tcl_AppendResult(interp, MORE_ARGS_STR,
			 "Inheritance (0|1)? (CR to skip) ", (char *)NULL);
	switch ( comb->inherit ) {
	    default:
		bu_vls_printf(&curr_cmd_list->cl_more_default, "1");
		break;
	    case 0:
		bu_vls_printf(&curr_cmd_list->cl_more_default, "0");
		break;
	}

	goto fail;
    }

    switch ( inherit )  {
	case '1':
	    comb->inherit = 1;
	    break;
	case '0':
	    comb->inherit = 0;
	    break;
	case '\0':
	case '\n':
	    break;
	default:
	    Tcl_AppendResult(interp, "Unknown response ignored\n", (char *)NULL);
	    break;
    }

    if ( rt_db_put_internal( dp, dbip, &intern, &rt_uniresource ) < 0 )  {
	TCL_WRITE_ERR_return;
    }
    return TCL_OK;
 fail:
    rt_db_free_internal( &intern, &rt_uniresource );
    return TCL_ERROR;
}

int
f_edmater(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    Tcl_DString ds;
    int ret;
    
    CHECK_DBI_NULL;
    CHECK_READ_ONLY;

    ret = ged_edmater(wdbp, argc, argv);

    /* Convert to Tcl codes */
    if (ret == GED_OK)
	ret = TCL_OK;
    else
	ret = TCL_ERROR;

    Tcl_DStringInit(&ds);
    Tcl_DStringAppend(&ds, bu_vls_addr(&wdbp->wdb_result_str), -1);
    Tcl_DStringResult(interp, &ds);

    return ret;
}


int
f_wmater(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    Tcl_DString ds;
    int ret;
    
    CHECK_DBI_NULL;
    CHECK_READ_ONLY;

    ret = ged_wmater(wdbp, argc, argv);

    /* Convert to Tcl codes */
    if (ret == GED_OK)
	ret = TCL_OK;
    else
	ret = TCL_ERROR;

    Tcl_DStringInit(&ds);
    Tcl_DStringAppend(&ds, bu_vls_addr(&wdbp->wdb_result_str), -1);
    Tcl_DStringResult(interp, &ds);

    return ret;
}


int
f_rmater(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    Tcl_DString ds;
    int ret;
    
    CHECK_DBI_NULL;
    CHECK_READ_ONLY;

    ret = ged_rmater(wdbp, argc, argv);

    /* Convert to Tcl codes */
    if (ret == GED_OK)
	ret = TCL_OK;
    else
	ret = TCL_ERROR;

    Tcl_DStringInit(&ds);
    Tcl_DStringAppend(&ds, bu_vls_addr(&wdbp->wdb_result_str), -1);
    Tcl_DStringResult(interp, &ds);

    return ret;
}


/*
 *			F _ C O M B _ C O L O R
 *
 *  Simple command-line way to set object color
 *  Usage: ocolor combination R G B
 */
int
f_comb_color(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    Tcl_DString ds;
    int ret;
    
    CHECK_DBI_NULL;
    CHECK_READ_ONLY;

    ret = ged_comb_color(wdbp, argc, argv);

    /* Convert to Tcl codes */
    if (ret == GED_OK)
	ret = TCL_OK;
    else
	ret = TCL_ERROR;

    Tcl_DStringInit(&ds);
    Tcl_DStringAppend(&ds, bu_vls_addr(&wdbp->wdb_result_str), -1);
    Tcl_DStringResult(interp, &ds);

    return ret;
}

/*
 *			F _ S H A D E R
 *
 *  Simpler, command-line version of 'mater' command.
 *
 *  Usage:
 *      shader combination shader_material [shader_argument(s)]
 */
int
f_shader(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    Tcl_DString ds;
    int ret;
    
    CHECK_DBI_NULL;
    CHECK_READ_ONLY;

    ret = ged_shader(wdbp, argc, argv);

    /* Convert to Tcl codes */
    if (ret == GED_OK)
	ret = TCL_OK;
    else
	ret = TCL_ERROR;

    Tcl_DStringInit(&ds);
    Tcl_DStringAppend(&ds, bu_vls_addr(&wdbp->wdb_result_str), -1);
    Tcl_DStringResult(interp, &ds);

    return ret;
}


/* 
 * Usage:
 *     mirror [-d dir] [-o origin] [-p scalar_pt] [-x] [-y] [-z] old new
 */
int
f_mirror(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    Tcl_DString ds;
    int ret;

    CHECK_DBI_NULL;
    CHECK_READ_ONLY;

    ret = ged_mirror(wdbp, argc, argv);

    /* Convert to Tcl codes */
    if (ret == GED_OK)
	ret = TCL_OK;
    else
	ret = TCL_ERROR;

    Tcl_DStringInit(&ds);
    Tcl_DStringAppend(&ds, bu_vls_addr(&wdbp->wdb_result_str), -1);
    Tcl_DStringResult(interp, &ds);

    if (wdbp->wdb_result_flags == 0 && ret == TCL_OK) {
	const char *av[3];

	av[0] = "draw";
	av[1] = argv[argc-1];
	av[2] = NULL;
	cmd_draw(clientData, interp, 2, av);
    }

    return ret;
}

/* Modify Combination record information */
/* Format: edcomb combname Regionflag regionid air los GIFTmater */
int
f_edcomb(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    Tcl_DString ds;
    int ret;
    
    CHECK_DBI_NULL;
    CHECK_READ_ONLY;

    ret = ged_edcomb(wdbp, argc, argv);

    /* Convert to Tcl codes */
    if (ret == GED_OK)
	ret = TCL_OK;
    else
	ret = TCL_ERROR;

    Tcl_DStringInit(&ds);
    Tcl_DStringAppend(&ds, bu_vls_addr(&wdbp->wdb_result_str), -1);
    Tcl_DStringResult(interp, &ds);

    return ret;
}

/* tell him it already exists */
void
aexists(char *name)
{
    Tcl_AppendResult(interp, name, ":  already exists\n", (char *)NULL);
}

/*
 *  			F _ M A K E
 *
 *  Create a new solid of a given type
 *  (Generic, or explicit)
 */
int
f_make(ClientData	clientData,
       Tcl_Interp	*interp,
       int		argc,
       char		**argv)
{
    Tcl_DString ds;
    int ret;

    CHECK_DBI_NULL;
    CHECK_READ_ONLY;

    if (argc == 3) {
	char *av[8];
	char center[512];
	char scale[128];

	sprintf(center, "%lf %lf %lf",
		-view_state->vs_vop->vo_center[MDX],
		-view_state->vs_vop->vo_center[MDY],
		-view_state->vs_vop->vo_center[MDZ]);
	sprintf(scale, "%lf", view_state->vs_vop->vo_scale * 2.0);

	av[0] = argv[0];
	av[1] = "-o";
	av[2] = center;
	av[3] = "-s";
	av[4] = scale;
	av[5] = argv[1];
	av[6] = argv[2];
	av[7] = (char *)0;

	ret = ged_make(wdbp, 7, av);
    } else
	ret = ged_make(wdbp, argc, argv);

    /* Convert to Tcl codes */
    if (ret == GED_OK)
	ret = TCL_OK;
    else
	ret = TCL_ERROR;

    Tcl_DStringInit(&ds);
    Tcl_DStringAppend(&ds, bu_vls_addr(&wdbp->wdb_result_str), -1);
    Tcl_DStringResult(interp, &ds);

    if (wdbp->wdb_result_flags == 0 && ret == TCL_OK) {
	const char *av[3];

	av[0] = "draw";
	av[1] = argv[argc-2];
	av[2] = NULL;
	cmd_draw(clientData, interp, 2, av);
    }

    return ret;
}

int
mged_rot_obj(Tcl_Interp *interp, int iflag, fastf_t *argvect)
{
    point_t model_pt;
    point_t point;
    point_t s_point;
    mat_t temp;
    vect_t v_work;

    update_views = 1;

    if (movedir != ROTARROW) {
	/* NOT in object rotate mode - put it in obj rot */
	movedir = ROTARROW;
    }

    /* find point for rotation to take place wrt */
#if 0
    MAT4X3PNT(model_pt, es_mat, es_keypoint);
#else
    VMOVE(model_pt, es_keypoint);
#endif
    MAT4X3PNT(point, modelchanges, model_pt);

    /* Find absolute translation vector to go from "model_pt" to
     * 	"point" without any of the rotations in "modelchanges"
     */
    VSCALE(s_point, point, modelchanges[15]);
    VSUB2(v_work, s_point, model_pt);

    /* REDO "modelchanges" such that:
     *	1. NO rotations (identity)
     *	2. trans == v_work
     *	3. same scale factor
     */
    MAT_IDN(temp);
    MAT_DELTAS_VEC(temp, v_work);
    temp[15] = modelchanges[15];
    MAT_COPY(modelchanges, temp);

    /* build new rotation matrix */
    MAT_IDN(temp);
    bn_mat_angles(temp, argvect[0], argvect[1], argvect[2]);

    if (iflag) {
	/* apply accumulated rotations */
	bn_mat_mul2(acc_rot_sol, temp);
    }

    /*XXX*/ MAT_COPY(acc_rot_sol, temp); /* used to rotate solid/object axis */

    /* Record the new rotation matrix into the revised
     *	modelchanges matrix wrt "point"
     */
    wrt_point(modelchanges, temp, modelchanges, point);

#ifdef DO_NEW_EDIT_MATS
    new_edit_mats();
#else
    new_mats();
#endif

    return TCL_OK;
}


/* allow precise changes to object rotation */
int
f_rot_obj(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    int iflag = 0;
    vect_t argvect;

    CHECK_DBI_NULL;
    CHECK_READ_ONLY;

    if (argc < 4 || 5 < argc) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "help %s", argv[0]);
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if ( not_state( ST_O_EDIT, "Object Rotation" ) )
	return TCL_ERROR;

    /* Check for -i option */
    if (argv[1][0] == '-' && argv[1][1] == 'i') {
	iflag = 1;  /* treat arguments as incremental values */
	++argv;
	--argc;
    }

    if (argc != 4)
	return TCL_ERROR;

    argvect[0] = atof(argv[1]);
    argvect[1] = atof(argv[2]);
    argvect[2] = atof(argv[3]);

    return mged_rot_obj(interp, iflag, argvect);
}

/* allow precise changes to object scaling, both local & global */
int
f_sc_obj(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    mat_t incr;
    vect_t point, temp;

    CHECK_DBI_NULL;
    CHECK_READ_ONLY;

    if (argc < 2 || 2 < argc) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "help oscale");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if ( not_state( ST_O_EDIT, "Object Scaling" ) )
	return TCL_ERROR;

    if ( atof(argv[1]) <= 0.0 ) {
	Tcl_AppendResult(interp, "ERROR: scale factor <=  0\n", (char *)NULL);
	return TCL_ERROR;
    }

    update_views = 1;

#if 0
    if (movedir != SARROW) {
	/* Put in global object scale mode */
	if ( edobj == 0 )
	    edobj = BE_O_SCALE;	/* default is global scaling */
	movedir = SARROW;
    }
#endif

    MAT_IDN(incr);

    /* switch depending on type of scaling to do */
    switch ( edobj ) {
	default:
	case BE_O_SCALE:
	    /* global scaling */
	    incr[15] = 1.0 / (atof(argv[1]) * modelchanges[15]);
	    break;
	case BE_O_XSCALE:
	    /* local scaling ... X-axis */
	    incr[0] = atof(argv[1]) / acc_sc[0];
	    acc_sc[0] = atof(argv[1]);
	    break;
	case BE_O_YSCALE:
	    /* local scaling ... Y-axis */
	    incr[5] = atof(argv[1]) / acc_sc[1];
	    acc_sc[1] = atof(argv[1]);
	    break;
	case BE_O_ZSCALE:
	    /* local scaling ... Z-axis */
	    incr[10] = atof(argv[1]) / acc_sc[2];
	    acc_sc[2] = atof(argv[1]);
	    break;
    }

    /* find point the scaling is to take place wrt */
#if 0
    MAT4X3PNT(temp, es_mat, es_keypoint);
#else
    VMOVE(temp, es_keypoint);
#endif
    MAT4X3PNT(point, modelchanges, temp);

    wrt_point(modelchanges, incr, modelchanges, point);
#ifdef DO_NEW_EDIT_MATS
    new_edit_mats();
#else
    new_mats();
#endif

    return TCL_OK;
}

/*
 *			F _ T R _ O B J
 *
 *  Bound to command "translate"
 *
 *  Allow precise changes to object translation
 */
int
f_tr_obj(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    register int i;
    mat_t incr, old;
    vect_t model_sol_pt, model_incr, ed_sol_pt, new_vertex;

    CHECK_DBI_NULL;
    CHECK_READ_ONLY;

    if (argc < 4 || 4 < argc) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "help translate");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if ( state == ST_S_EDIT )  {
	/* In solid edit mode,
	 * perform the equivalent of "press sxy" and "p xyz"
	 */
	if ( be_s_trans(clientData, interp, argc, argv) == TCL_ERROR )
	    return TCL_ERROR;
	return f_param(clientData, interp, argc, argv);
    }

    if ( not_state( ST_O_EDIT, "Object Translation") )
	return TCL_ERROR;

    /* Remainder of code concerns object edit case */

    update_views = 1;

    MAT_IDN(incr);
    MAT_IDN(old);

    if ( (movedir & (RARROW|UARROW)) == 0 ) {
	/* put in object trans mode */
	movedir = UARROW | RARROW;
    }

    for (i=0; i<3; i++) {
	new_vertex[i] = atof(argv[i+1]) * local2base;
    }
#if 0
    MAT4X3PNT(model_sol_pt, es_mat, es_keypoint);
#else
    VMOVE(model_sol_pt, es_keypoint);
#endif
    MAT4X3PNT(ed_sol_pt, modelchanges, model_sol_pt);
    VSUB2(model_incr, new_vertex, ed_sol_pt);
    MAT_DELTAS_VEC(incr, model_incr);
    MAT_COPY(old, modelchanges);
    bn_mat_mul(modelchanges, incr, old);
#ifdef DO_NEW_EDIT_MATS
    new_edit_mats();
#else
    new_mats();
#endif

    return TCL_OK;
}

/* Change the default region ident codes: item air los mat
 */
int
f_regdef(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    if (argc == 1) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "ident %d air %d los %d material %d",
		      item_default, air_default, los_default, mat_default);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	return TCL_OK;
    }

    if (argc < 2 || 5 < argc) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "help regdef");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return TCL_ERROR;
    }

    view_state->vs_flag = 1;
    item_default = atoi(argv[1]);
    wdbp->wdb_item_default = item_default;

    if (argc == 2)
	return TCL_OK;

    air_default = atoi(argv[2]);
    wdbp->wdb_air_default = air_default;
    if (air_default) {
	item_default = 0;
	wdbp->wdb_item_default = 0;
    }

    if (argc == 3)
	return TCL_OK;

    los_default = atoi(argv[3]);
    wdbp->wdb_los_default = los_default;

    if (argc == 4)
	return TCL_OK;

    mat_default = atoi(argv[4]);
    wdbp->wdb_mat_default = mat_default;

    return TCL_OK;
}

static int frac_stat;
void
mged_add_nmg_part(char *newname, struct model *m)
{
    struct rt_db_internal	new_intern;
    struct directory *new_dp;
    struct nmgregion *r;

    if (dbip == DBI_NULL)
	return;

    if ( db_lookup( dbip,  newname, LOOKUP_QUIET ) != DIR_NULL )  {
	aexists( newname );
	/* Free memory here */
	nmg_km(m);
	frac_stat = 1;
	return;
    }

    if ( (new_dp=db_diradd( dbip, newname, -1, 0, DIR_SOLID, (genptr_t)&new_intern.idb_type)) == DIR_NULL )  {
	TCL_ALLOC_ERR;
	return;
    }

    /* make sure the geometry/bounding boxes are up to date */
    for (BU_LIST_FOR(r, nmgregion, &m->r_hd))
	nmg_region_a(r, &mged_tol);


    /* Export NMG as a new solid */
    RT_INIT_DB_INTERNAL(&new_intern);
    new_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    new_intern.idb_type = ID_NMG;
    new_intern.idb_meth = &rt_functab[ID_NMG];
    new_intern.idb_ptr = (genptr_t)m;

    if ( rt_db_put_internal( new_dp, dbip, &new_intern, &rt_uniresource ) < 0 )  {
	/* Free memory */
	nmg_km(m);
	Tcl_AppendResult(interp, "rt_db_put_internal() failure\n", (char *)NULL);
	frac_stat = 1;
	return;
    }
    /* Internal representation has been freed by rt_db_put_internal */
    new_intern.idb_ptr = (genptr_t)NULL;
    frac_stat = 0;
}
/*
 *			F _ F R A C T U R E
 *
 * Usage: fracture nmgsolid [prefix]
 *
 *	given an NMG solid, break it up into several NMG solids, each
 *	containing a single shell with a single sub-element.
 */
int
f_fracture(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    register int i;
    struct directory *old_dp;
    struct rt_db_internal	old_intern;
    struct model	*m, *new_model;
    char		newname[32];
    char		prefix[32];
    int	maxdigits;
    struct nmgregion *r, *new_r;
    struct shell *s, *new_s;
    struct faceuse *fu;
    struct vertex *v_new, *v;
    unsigned long tw, tf, tp;

    CHECK_DBI_NULL;
    CHECK_READ_ONLY;

    if (argc < 2 || 3 < argc) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "help fracture");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    Tcl_AppendResult(interp, "fracture:", (char *)NULL);
    for (i=0; i < argc; i++)
	Tcl_AppendResult(interp, " ", argv[i], (char *)NULL);
    Tcl_AppendResult(interp, "\n", (char *)NULL);

    if ( (old_dp = db_lookup( dbip,  argv[1], LOOKUP_NOISY )) == DIR_NULL )
	return TCL_ERROR;

    if ( rt_db_get_internal( &old_intern, old_dp, dbip, bn_mat_identity, &rt_uniresource ) < 0 )  {
	Tcl_AppendResult(interp, "rt_db_get_internal() error\n", (char *)NULL);
	return TCL_ERROR;
    }

    if ( old_intern.idb_type != ID_NMG )
    {
	Tcl_AppendResult(interp, argv[1], " is not an NMG solid!!\n", (char *)NULL );
	rt_db_free_internal( &old_intern, &rt_uniresource );
	return TCL_ERROR;
    }

    m = (struct model *)old_intern.idb_ptr;
    NMG_CK_MODEL(m);

    /* how many characters of the solid names do we reserve for digits? */
    nmg_count_shell_kids(m, &tf, &tw, &tp);

    maxdigits = (int)(log10((double)(tf+tw+tp)) + 1.0);

    {
	struct bu_vls tmp_vls;

	bu_vls_init(&tmp_vls);
	bu_vls_printf(&tmp_vls, "%ld = %d digits\n", (long)(tf+tw+tp), maxdigits);
	Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	bu_vls_free(&tmp_vls);
    }

    /*	for (maxdigits=1, i=tf+tw+tp; i > 0; i /= 10)
     *	maxdigits++;
     */

    /* get the prefix for the solids to be created. */
    memset(prefix, 0, sizeof(prefix));
    bu_strlcpy(prefix, argv[argc-1], sizeof(prefix));
    bu_strlcat(prefix, "_", sizeof(prefix));

    /* Bust it up here */

    i = 1;
    for (BU_LIST_FOR(r, nmgregion, &m->r_hd)) {
	NMG_CK_REGION(r);
	for (BU_LIST_FOR(s, shell, &r->s_hd)) {
	    NMG_CK_SHELL(s);
	    if (s->vu_p) {
		NMG_CK_VERTEXUSE(s->vu_p);
		NMG_CK_VERTEX(s->vu_p->v_p);
		v = s->vu_p->v_p;

/*	nmg_start_dup(m); */
		new_model = nmg_mm();
		new_r = nmg_mrsv(new_model);
		new_s = BU_LIST_FIRST(shell, &r->s_hd);
		v_new = new_s->vu_p->v_p;
		if (v->vg_p) {
		    nmg_vertex_gv(v_new, v->vg_p->coord);
		}
/*	nmg_end_dup(); */

		snprintf(newname, 32, "%s%0*d", prefix, maxdigits, i++);

		mged_add_nmg_part(newname, new_model);
		if (frac_stat) return CMD_BAD;
		continue;
	    }
	    for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
		if (fu->orientation != OT_SAME)
		    continue;

		NMG_CK_FACEUSE(fu);

		new_model = nmg_mm();
		NMG_CK_MODEL(new_model);
		new_r = nmg_mrsv(new_model);
		NMG_CK_REGION(new_r);
		new_s = BU_LIST_FIRST(shell, &new_r->s_hd);
		NMG_CK_SHELL(new_s);
/*	nmg_start_dup(m); */
		NMG_CK_SHELL(new_s);
		nmg_dup_face(fu, new_s);
/*	nmg_end_dup(); */

		snprintf(newname, 32, "%s%0*d", prefix, maxdigits, i++);
		mged_add_nmg_part(newname, new_model);
		if (frac_stat) return CMD_BAD;
	    }
#if 0
	    while (BU_LIST_NON_EMPTY(&s->lu_hd)) {
		lu = BU_LIST_FIRST(loopuse, &s->lu_hd);
		new_model = nmg_mm();
		r = nmg_mrsv(new_model);
		new_s = BU_LIST_FIRST(shell, &r->s_hd);

		nmg_dup_loop(lu, new_s);
		nmg_klu(lu);

		snprintf(newname, 32, "%s%0*d", prefix, maxdigits, i++);
		mged_add_nmg_part(newname, new_model);
		if (frac_stat) return CMD_BAD;
	    }
	    while (BU_LIST_NON_EMPTY(&s->eu_hd)) {
		eu = BU_LIST_FIRST(edgeuse, &s->eu_hd);
		new_model = nmg_mm();
		r = nmg_mrsv(new_model);
		new_s = BU_LIST_FIRST(shell, &r->s_hd);

		nmg_dup_edge(eu, new_s);
		nmg_keu(eu);

		snprintf(newname, 32, "%s%0*d", prefix, maxdigits, i++);

		mged_add_nmg_part(newname, new_model);
		if (frac_stat) return TCL_ERROR;
	    }
#endif
	}
    }
    return TCL_OK;

}
/*
 *			F _ Q O R O T
 *
 * Usage: qorot x y z dx dy dz theta
 *
 *	rotate an object through a specified angle
 *	about a specified ray.
 */
int
f_qorot(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    mat_t temp;
    vect_t s_point, point, v_work, model_pt;
    vect_t	specified_pt, direc;

    CHECK_DBI_NULL;
    CHECK_READ_ONLY;

    if (argc < 8 || 8 < argc) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "help qorot");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if ( not_state( ST_O_EDIT, "Object Rotation" ) )
	return TCL_ERROR;

    if (movedir != ROTARROW) {
	/* NOT in object rotate mode - put it in obj rot */
	movedir = ROTARROW;
    }
    VSET(specified_pt, atof(argv[1]), atof(argv[2]), atof(argv[3]));
    VSCALE(specified_pt, specified_pt, dbip->dbi_local2base);
    VSET(direc, atof(argv[4]), atof(argv[5]), atof(argv[6]));

    /* find point for rotation to take place wrt */
    MAT4X3PNT(model_pt, es_mat, specified_pt);
    MAT4X3PNT(point, modelchanges, model_pt);

    /* Find absolute translation vector to go from "model_pt" to
     * 	"point" without any of the rotations in "modelchanges"
     */
    VSCALE(s_point, point, modelchanges[15]);
    VSUB2(v_work, s_point, model_pt);

    /* REDO "modelchanges" such that:
     *	1. NO rotations (identity)
     *	2. trans == v_work
     *	3. same scale factor
     */
    MAT_IDN(temp);
    MAT_DELTAS_VEC(temp, v_work);
    temp[15] = modelchanges[15];
    MAT_COPY(modelchanges, temp);

    /* build new rotation matrix */
    MAT_IDN(temp);
    bn_mat_angles(temp, 0.0, 0.0, atof(argv[7]));

    /* Record the new rotation matrix into the revised
     *	modelchanges matrix wrt "point"
     */
    wrt_point_direc(modelchanges, temp, modelchanges, point, direc);

#ifdef DO_NEW_EDIT_MATS
    new_edit_mats();
#else
    new_mats();
#endif

    return TCL_OK;
}

void
set_localunit_TclVar(void)
{
    struct bu_vls vls;
    struct bu_vls units_vls;
    const char	*str;

    if (dbip == DBI_NULL)
	return;

    bu_vls_init(&vls);
    bu_vls_init(&units_vls);

    str = bu_units_string(dbip->dbi_local2base);
    if (str)
	bu_vls_strcpy(&units_vls, str);
    else
	bu_vls_printf(&units_vls, "%gmm", dbip->dbi_local2base);

    bu_vls_strcpy(&vls, "localunit");
    Tcl_SetVar(interp, bu_vls_addr(&vls), bu_vls_addr(&units_vls), TCL_GLOBAL_ONLY);

    bu_vls_free(&vls);
    bu_vls_free(&units_vls);
}


int
f_bo(ClientData	clientData,
	 Tcl_Interp	*interp,
	 int		argc,
	 char 		**argv)
{
    CHECK_DBI_NULL;

    return wdb_bo_cmd(wdbp, interp, argc, argv);
}

int cmd_bot_smooth( ClientData	clientData,
		    Tcl_Interp	*interp,
		    int		argc,
		    char 		**argv)
{
    CHECK_DBI_NULL;

    return wdb_bot_smooth_cmd( wdbp, interp, argc, argv );
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
