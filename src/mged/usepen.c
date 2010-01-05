/*                        U S E P E N . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2010 United States Government as represented by
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
/** @file usepen.c
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "dg.h"

#include "./mged.h"
#include "./titles.h"
#include "./mged_dm.h"

#include "./sedit.h"

/*	Degree <-> Radian conversion factors	*/
double	degtorad =  0.01745329251994329573;
double	radtodeg = 57.29577951308232098299;

struct ged_display_list *illum_gdlp = GED_DISPLAY_LIST_NULL;
struct solid	*illump = SOLID_NULL;	/* == 0 if none, else points to ill. solid */
int		ipathpos = 0;	/* path index of illuminated element */
				/* set by e9.c, cleared here */
void		wrt_view(fastf_t *out, const fastf_t *change, const fastf_t *in), wrt_point(fastf_t *out, const fastf_t *change, const fastf_t *in, const fastf_t *point);
static void	illuminate(int y);

/*
 *			F _ M O U S E
 *
 * X and Y are expected to be in -2048 <= x, y <= +2047 range.
 * The "up" flag is 1 on the not-pressed to pressed transition,
 * and 0 on the pressed to not-pressed transition.
 *
 * Note -
 *  The mouse is the focus of much of the editing activity in GED.
 *  The editor operates in one of seven basic editing states, recorded
 *  in the variable called "state".  When no editing is taking place,
 *  the editor is in state ST_VIEW.  There are two paths out of ST_VIEW:
 *
 *  BE_S_ILLUMINATE, when pressed, takes the editor into ST_S_PICK,
 *  where the mouse is used to pick a solid to edit, using our
 *  unusual "illuminate" technique.  Moving the mouse varies the solid
 *  being illuminated.  When the mouse is pressed, the editor moves into
 *  state ST_S_EDIT, and solid editing may begin.  Solid editing is
 *  terminated via BE_ACCEPT and BE_REJECT.
 *
 *  BE_O_ILLUMINATE, when pressed, takes the editor into ST_O_PICK,
 *  again performing the illuminate procedure.  When the mouse is pressed,
 *  the editor moves into state ST_O_PATH.  Now, moving the mouse allows
 *  the user to choose the portion of the path relation to be edited.
 *  When the mouse is pressed, the editor moves into state ST_O_EDIT,
 *  and object editing may begin.  Object editing is terminated via
 *  BE_ACCEPT and BE_REJECT.
 *
 *  The only way to exit the intermediate states (non-VIEW, non-EDIT)
 *  is by completing the sequence, or pressing BE_REJECT.
 */
int
f_mouse(
    ClientData clientData,
    Tcl_Interp *interp,
    int	argc,
    char	**argv)
{
    vect_t	mousevec;		/* float pt -1..+1 mouse pos vect */
    int	isave;
    int	up;
    int	xpos;
    int	ypos;

    if (argc < 4 || 4 < argc) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "help M");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    up = atoi(argv[1]);
    xpos = atoi(argv[2]);
    ypos = atoi(argv[3]);

    /* Build floating point mouse vector, -1 to +1 */
    mousevec[X] =  xpos * INV_GED;
    mousevec[Y] =  ypos * INV_GED;
    mousevec[Z] = 0;

    if (mged_variables->mv_faceplate && mged_variables->mv_orig_gui && up) {
	/*
	 * If mouse press is in scroll area, see if scrolling, and if so,
	 * divert this mouse press.
	 */
	if ( (xpos >= MENUXLIM) || scroll_active )  {
	    int i;

	    if (scroll_active)
		ypos = scroll_y;

	    if ( (i = scroll_select( xpos, ypos, 1 )) < 0 )  {
		Tcl_AppendResult(interp,
				 "mouse press outside valid scroll area\n",
				 (char *)NULL);
		return TCL_ERROR;
	    }

	    if ( i > 0 )  {
		scroll_active = 1;
		scroll_y = ypos;

		/* Scroller bars claimed button press */
		return TCL_OK;
	    }
	    /* Otherwise, fall through */
	}

	/*
	 * If menu is active, and mouse press is in menu area,
	 * divert this mouse press for menu purposes.
	 */
	if ( xpos < MENUXLIM )  {
	    int i;

	    if ( (i = mmenu_select( ypos, 1 )) < 0 )  {
		Tcl_AppendResult(interp,
				 "mouse press outside valid menu\n",
				 (char *)NULL);
		return TCL_ERROR;
	    }

	    if ( i > 0 )  {
		/* Menu claimed button press */
		return TCL_OK;
	    }
	    /* Otherwise, fall through */
	}
    }

    /*
     *  In the best of all possible worlds, nothing should happen
     *  when the mouse is not pressed;  this would relax the requirement
     *  for the host being informed when the mouse changes position.
     *  However, for now, illuminate mode makes this impossible.
     */
    if ( up == 0 )  switch ( state )  {

	case ST_VIEW:
	case ST_S_EDIT:
	case ST_O_EDIT:
	default:
	    return TCL_OK;		/* Take no action in these states */

	case ST_O_PICK:
	case ST_S_PICK:
	    /*
	     * Use the mouse for illuminating a solid
	     */
	    illuminate( ypos );
	    return TCL_OK;

	case ST_O_PATH:
	    /*
	     * Convert DT position to path element select
	     */
	    isave = ipathpos;
	    ipathpos = illump->s_fullpath.fp_len-1 - (
		(ypos+(int)GED_MAX) * (illump->s_fullpath.fp_len) / (int)GED_RANGE);
	    if ( ipathpos != isave )
		view_state->vs_flag = 1;
	    return TCL_OK;

    } else switch ( state )  {

	case ST_VIEW:
	    /*
	     * Use the DT for moving view center.
	     * Make indicated point be new view center (NEW).
	     */
	    slewview( mousevec );
	    return TCL_OK;

	case ST_O_PICK:
	    ipathpos = 0;
	    (void)chg_state( ST_O_PICK, ST_O_PATH, "mouse press");
	    view_state->vs_flag = 1;
	    return TCL_OK;

	case ST_S_PICK:
	    /* Check details, Init menu, set state */
	    init_sedit();		/* does chg_state */
	    view_state->vs_flag = 1;
	    return TCL_OK;

	case ST_S_EDIT:
	    if ((SEDIT_TRAN || SEDIT_SCALE || SEDIT_PICK) && mged_variables->mv_transform == 'e')
		sedit_mouse( mousevec );
	    else
		slewview( mousevec );
	    return TCL_OK;

	case ST_O_PATH:
	    /*
	     * Set combination "illuminate" mode.  This code
	     * assumes that the user has already illuminated
	     * a single solid, and wishes to move a collection of
	     * objects of which the illuminated solid is a part.
	     * The whole combination will not illuminate (to save
	     * vector drawing time), but all the objects should
	     * move/scale in unison.
	     */
	{
	    char	*av[3];
	    char	num[8];
	    (void)sprintf(num, "%d", ipathpos);
	    av[0] = "matpick";
	    av[1] = num;
	    av[2] = (char *)NULL;
	    (void)f_matpick( clientData, interp, 2, av );
	    /* How to record this in the journal file? */
	    return TCL_OK;
	}

	case ST_S_VPICK:
	    sedit_vpick( mousevec );
	    return TCL_OK;

	case ST_O_EDIT:
	    if ((OEDIT_TRAN || OEDIT_SCALE) && mged_variables->mv_transform == 'e')
		objedit_mouse( mousevec );
	    else
		slewview( mousevec );

	    return TCL_OK;

	default:
	    state_err( "mouse press" );
	    return TCL_ERROR;
    }
    /* NOTREACHED */
}

/*
 *			I L L U M I N A T E
 *
 *  All solids except for the illuminated one have s_iflag set to DOWN.
 *  The illuminated one has s_iflag set to UP, and also has the global
 *  variable "illump" pointing at it.
 */
static void
illuminate(int y) {
    struct ged_display_list *gdlp;
    struct ged_display_list *next_gdlp;
    int count;
    struct solid *sp;

    /*
     * Divide the mouse into 'ndrawn' VERTICAL zones, and use the
     * zone number as a sequential position among solids
     * which are drawn.
     */
    count = ((fastf_t)y + GED_MAX) * ndrawn / GED_RANGE;

    gdlp = BU_LIST_NEXT(ged_display_list, &gedp->ged_gdp->gd_headDisplay);
    while (BU_LIST_NOT_HEAD(gdlp, &gedp->ged_gdp->gd_headDisplay)) {
	next_gdlp = BU_LIST_PNEXT(ged_display_list, gdlp);

	FOR_ALL_SOLIDS(sp, &gdlp->gdl_headSolid) {
	    /* Only consider solids which are presently in view */
	    if ( sp->s_flag == UP )  {
		if ( count-- == 0 ) {
		    sp->s_iflag = UP;
		    illump = sp;
		    illum_gdlp = gdlp;
		}  else  {
		    /* All other solids have s_iflag set DOWN */
		    sp->s_iflag = DOWN;
		}
	    }
	}

	gdlp = next_gdlp;
    }

    update_views = 1;
}

/*
 *                        A I L L
 *
 *   advance illump or ipathpos
 */
int
f_aip(
    ClientData clientData,
    Tcl_Interp *interp,
    int argc,
    char **argv)
{
    struct ged_display_list *gdlp;
    struct solid *sp;

    if (argc < 1 || 2 < argc) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helpdevel aip");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (!ndrawn) {
	return TCL_OK;
    } else if (state != ST_S_PICK && state != ST_O_PICK  && state != ST_O_PATH) {
	return TCL_OK;
    }

    if (state == ST_O_PATH) {
	if (argc == 1 || *argv[1] == 'f') {
	    ++ipathpos;
	    if (ipathpos >= illump->s_fullpath.fp_len)
		ipathpos = 0;
	} else if (*argv[1] == 'b') {
	    --ipathpos;
	    if (ipathpos < 0)
		ipathpos = illump->s_fullpath.fp_len-1;
	} else {
	    Tcl_AppendResult(interp, "aip: bad parameter - ", argv[1], "\n", (char *)NULL);
	    return TCL_ERROR;
	}
    } else {
	gdlp = illum_gdlp;
	sp = illump;
	sp->s_iflag = DOWN;
	if (argc == 1 || *argv[1] == 'f') {
	    if (BU_LIST_NEXT_IS_HEAD(sp, &gdlp->gdl_headSolid)) {
		/* Advance the gdlp (i.e. display list) */
		if (BU_LIST_NEXT_IS_HEAD(gdlp, &gedp->ged_gdp->gd_headDisplay))
		    gdlp = BU_LIST_NEXT(ged_display_list, &gedp->ged_gdp->gd_headDisplay);
		else
		    gdlp = BU_LIST_PNEXT(ged_display_list, gdlp);


		sp = BU_LIST_NEXT(solid, &gdlp->gdl_headSolid);
	    } else
		sp = BU_LIST_PNEXT(solid, sp);
	} else if (*argv[1] == 'b') {
	    if (BU_LIST_PREV_IS_HEAD(sp, &gdlp->gdl_headSolid)) {
		/* Advance the gdlp (i.e. display list) */
		if (BU_LIST_PREV_IS_HEAD(gdlp, &gedp->ged_gdp->gd_headDisplay))
		    gdlp = BU_LIST_PREV(ged_display_list, &gedp->ged_gdp->gd_headDisplay);
		else
		    gdlp = BU_LIST_PLAST(ged_display_list, gdlp);

		sp = BU_LIST_PREV(solid, &gdlp->gdl_headSolid);
	    } else
		sp = BU_LIST_PLAST(solid, sp);
	} else {
	    Tcl_AppendResult(interp, "aip: bad parameter - ", argv[1], "\n", (char *)NULL);
	    return TCL_ERROR;
	}

	sp->s_iflag = UP;
	illump = sp;
	illum_gdlp = gdlp;
    }

    update_views = 1;
    return TCL_OK;
}

/*
 *  			W R T _ V I E W
 *
 *  Given a model-space transformation matrix "change",
 *  return a matrix which applies the change with-respect-to
 *  the view center.
 */
void
wrt_view( mat_t out, const mat_t change, const mat_t in )
{
    static mat_t t1, t2;

    bn_mat_mul(t1, view_state->vs_gvp->gv_center, in);
    bn_mat_mul(t2, change, t1);

    /* Build "fromViewcenter" matrix */
    MAT_IDN(t1);
    MAT_DELTAS(t1, -view_state->vs_gvp->gv_center[MDX], -view_state->vs_gvp->gv_center[MDY], -view_state->vs_gvp->gv_center[MDZ]);
    bn_mat_mul(out, t1, t2);
}

/*
 *  			W R T _ P O I N T
 *
 *  Given a model-space transformation matrix "change",
 *  return a matrix which applies the change with-respect-to
 *  "point".
 */
void
wrt_point( mat_t out, const mat_t change, const mat_t in, const point_t point )
{
    mat_t	t;

    bn_mat_xform_about_pt( t, change, point );

    if (out == in)
	bn_mat_mul2( t, out );
    else
	bn_mat_mul( out, t, in );
}

/*
 *  			W R T _ P O I N T _ D I R E C
 *
 *  Given a model-space transformation matrix "change",
 *  return a matrix which applies the change with-respect-to
 *  given "point" and "direc".
 */
void
wrt_point_direc( mat_t out, const mat_t change, const mat_t in, const point_t point, const vect_t direc )
{
    static mat_t	t1;
    static mat_t	pt_to_origin, origin_to_pt;
    static mat_t	d_to_zaxis, zaxis_to_d;
    static vect_t	zaxis;

    /* build "point to origin" matrix */
    MAT_IDN( pt_to_origin );
    MAT_DELTAS_VEC_NEG(pt_to_origin, point);

    /* build "origin to point" matrix */
    MAT_IDN( origin_to_pt );
    MAT_DELTAS_VEC_NEG(origin_to_pt, point);

    /* build "direc to zaxis" matrix */
    VSET(zaxis, 0.0, 0.0, 1.0);
    bn_mat_fromto(d_to_zaxis, direc, zaxis);

    /* build "zaxis to direc" matrix */
    bn_mat_inv(zaxis_to_d, d_to_zaxis);

    /* apply change matrix...
     *	t1 = change * d_to_zaxis * pt_to_origin * in
     */
    bn_mat_mul4( t1, change, d_to_zaxis, pt_to_origin, in );

    /* apply origin_to_pt matrix:
     *	out = origin_to_pt * zaxis_to_d *
     *		change * d_to_zaxis * pt_to_origin * in
     */
    bn_mat_mul3( out, origin_to_pt, zaxis_to_d, t1 );
}

/*
 *			F _ M A T P I C K
 *
 *  When in O_PATH state, select the arc which contains the matrix
 *  which is going to be "object edited".
 *  The choice is recorded in variable "ipathpos".
 *
 *  There are two syntaxes:
 *	matpick a/b	Pick arc between a and b.
 *	matpick #	Similar to internal interface.
 *			0 = top level object is a solid.
 *			n = edit arc from path [n-1] to [n]
 */
int
f_matpick(
    ClientData clientData,
    Tcl_Interp *interp,
    int	argc,
    char	**argv)
{
    struct ged_display_list *gdlp;
    struct ged_display_list *next_gdlp;
    struct solid	*sp;
    char			*cp;
    int		j;
    int			illum_only = 0;

    CHECK_DBI_NULL;

    if (argc < 2 || 3 < argc) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "help matpick");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (!strcmp("-n", argv[1])) {
	illum_only = 1;
	--argc;
	++argv;
    }

    if (argc != 2) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "help matpick");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if ( not_state( ST_O_PATH, "Object Edit matrix pick" ) )
	return TCL_ERROR;

    if ( (cp = strchr( argv[1], '/' )) != NULL )  {
	struct directory	*d0, *d1;
	if ( (d1 = db_lookup( dbip, cp+1, LOOKUP_NOISY )) == DIR_NULL )
	    return TCL_ERROR;
	*cp = '\0';		/* modifies argv[1] */
	if ( (d0 = db_lookup( dbip, argv[1], LOOKUP_NOISY )) == DIR_NULL )
	    return TCL_ERROR;
	/* Find arc on illump path which runs from d0 to d1 */
	for ( j=1; j < illump->s_fullpath.fp_len; j++ )  {
	    if ( DB_FULL_PATH_GET(&illump->s_fullpath, j-1) != d0 )  continue;
	    if ( DB_FULL_PATH_GET(&illump->s_fullpath, j-0) != d1 )  continue;
	    ipathpos = j;
	    goto got;
	}
	Tcl_AppendResult(interp, "matpick: unable to find arc ", d0->d_namep,
			 "/", d1->d_namep, " in current selection.  Re-specify.\n",
			 (char *)NULL);
	return TCL_ERROR;
    } else {
	ipathpos = atoi(argv[1]);
	if ( ipathpos < 0 )  ipathpos = 0;
	else if ( ipathpos >= illump->s_fullpath.fp_len )
	    ipathpos = illump->s_fullpath.fp_len-1;
    }
 got:
    /* Include all solids with same tree top */
    gdlp = BU_LIST_NEXT(ged_display_list, &gedp->ged_gdp->gd_headDisplay);
    while (BU_LIST_NOT_HEAD(gdlp, &gedp->ged_gdp->gd_headDisplay)) {
	next_gdlp = BU_LIST_PNEXT(ged_display_list, gdlp);

	FOR_ALL_SOLIDS(sp, &gdlp->gdl_headSolid) {
	    for ( j = 0; j <= ipathpos; j++ )  {
		if ( DB_FULL_PATH_GET(&sp->s_fullpath, j) !=
		     DB_FULL_PATH_GET(&illump->s_fullpath, j) )
		    break;
	    }
	    /* Only accept if top of tree is identical */
	    if ( j == ipathpos+1 )
		sp->s_iflag = UP;
	    else
		sp->s_iflag = DOWN;
	}

	gdlp = next_gdlp;
    }

    if (!illum_only) {
	(void)chg_state( ST_O_PATH, ST_O_EDIT, "mouse press" );
	chg_l2menu(ST_O_EDIT);

	/* begin object editing - initialize */
	init_oedit();
    }

    update_views = 1;
    return TCL_OK;
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
