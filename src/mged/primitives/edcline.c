/*                         E D C L I N E . C
 * BRL-CAD
 *
 * Copyright (c) 1996-2025 United States Government as represented by
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
/** @file mged/primitives/edcline.c
 *
 */

#include "common.h"

#include <math.h>
#include <string.h>

#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "wdb.h"

#include "../mged.h"
#include "../sedit.h"
#include "../mged_dm.h"
#include "./mged_functab.h"

#define ECMD_CLINE_SCALE_H	29077	/* scale height vector */
#define ECMD_CLINE_MOVE_H	29078	/* move end of height vector */
#define ECMD_CLINE_SCALE_R	29079	/* scale radius */
#define ECMD_CLINE_SCALE_T	29080	/* scale thickness */

#define MENU_CLINE_SCALE_H	29107
#define MENU_CLINE_MOVE_H	29108
#define MENU_CLINE_SCALE_R	29109
#define MENU_CLINE_SCALE_T	29110

static void
cline_ed(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b), void *UNUSED(data))
{
    s->s_edit.edit_flag = arg;

    switch (arg) {
	case ECMD_CLINE_MOVE_H:
	    s->s_edit.solid_edit_rotate = 0;
	    s->s_edit.solid_edit_translate = 1;
	    s->s_edit.solid_edit_scale = 0;
	    s->s_edit.solid_edit_pick = 0;
	    break;
	case ECMD_CLINE_SCALE_H:
	case ECMD_CLINE_SCALE_R:
	case ECMD_CLINE_SCALE_T:
	    s->s_edit.solid_edit_rotate = 0;
	    s->s_edit.solid_edit_translate = 0;
	    s->s_edit.solid_edit_scale = 1;
	    s->s_edit.solid_edit_pick = 0;
	    break;
	default:
	    mged_set_edflag(s, arg);
	    break;
    };

    sedit(s);
}

struct menu_item cline_menu[] = {
    { "CLINE MENU",		NULL, 0 },
    { "Set H",		cline_ed, ECMD_CLINE_SCALE_H },
    { "Move End H",		cline_ed, ECMD_CLINE_MOVE_H },
    { "Set R",		cline_ed, ECMD_CLINE_SCALE_R },
    { "Set plate thickness", cline_ed, ECMD_CLINE_SCALE_T },
    { "", NULL, 0 }
};

struct menu_item *
mged_cline_menu_item(const struct bn_tol *UNUSED(tol))
{
    return cline_menu;
}

void
mged_cline_e_axes_pos(
	struct mged_state *s,
	const struct rt_db_internal *ip,
       	const struct bn_tol *UNUSED(tol))
{
    if (s->s_edit.edit_flag == ECMD_CLINE_MOVE_H) {
	struct rt_cline_internal *cli =(struct rt_cline_internal *)ip->idb_ptr;
	point_t cli_v;
	vect_t cli_h;

	RT_CLINE_CK_MAGIC(cli);

	MAT4X3PNT(cli_v, s->s_edit.e_mat, cli->v);
	MAT4X3VEC(cli_h, s->s_edit.e_mat, cli->h);
	VADD2(s->s_edit.curr_e_axes_pos, cli_h, cli_v);
    } else {
	VMOVE(s->s_edit.curr_e_axes_pos, s->s_edit.e_keypoint);
    }
}

/*
 * Scale height vector
 */
int
ecmd_cline_scale_h(struct mged_state *s)
{
    if (s->s_edit.e_inpara != 1) {
	bu_vls_printf(s->s_edit.log_str, "ERROR: only one argument needed\n");
	s->s_edit.e_inpara = 0;
	return TCL_ERROR;
    }

    if (s->s_edit.e_para[0] <= 0.0) {
	bu_vls_printf(s->s_edit.log_str, "ERROR: SCALE FACTOR <= 0\n");
	s->s_edit.e_inpara = 0;
	return TCL_ERROR;
    }

    /* must convert to base units */
    s->s_edit.e_para[0] *= s->dbip->dbi_local2base;
    s->s_edit.e_para[1] *= s->dbip->dbi_local2base;
    s->s_edit.e_para[2] *= s->dbip->dbi_local2base;

    struct rt_cline_internal *cli =
	(struct rt_cline_internal *)s->s_edit.es_int.idb_ptr;

    RT_CLINE_CK_MAGIC(cli);

    if (s->s_edit.e_inpara) {
	s->s_edit.e_para[0] *= s->s_edit.e_mat[15];
	s->s_edit.es_scale = s->s_edit.e_para[0] / MAGNITUDE(cli->h);
	VSCALE(cli->h, cli->h, s->s_edit.es_scale);
    } else if (s->s_edit.es_scale > 0.0) {
	VSCALE(cli->h, cli->h, s->s_edit.es_scale);
	s->s_edit.es_scale = 0.0;
    }

    return 0;
}

/*
 * Scale radius
 */
int
ecmd_cline_scale_r(struct mged_state *s)
{
    if (s->s_edit.e_inpara != 1) {
	bu_vls_printf(s->s_edit.log_str, "ERROR: only one argument needed\n");
	s->s_edit.e_inpara = 0;
	return TCL_ERROR;
    }

    /* must convert to base units */
    s->s_edit.e_para[0] *= s->dbip->dbi_local2base;
    s->s_edit.e_para[1] *= s->dbip->dbi_local2base;
    s->s_edit.e_para[2] *= s->dbip->dbi_local2base;

    if (s->s_edit.e_para[0] <= 0.0) {
	bu_vls_printf(s->s_edit.log_str, "ERROR: SCALE FACTOR <= 0\n");
	s->s_edit.e_inpara = 0;
	return TCL_ERROR;
    }

    struct rt_cline_internal *cli =
	(struct rt_cline_internal *)s->s_edit.es_int.idb_ptr;

    RT_CLINE_CK_MAGIC(cli);

    if (s->s_edit.e_inpara)
	cli->radius = s->s_edit.e_para[0];
    else if (s->s_edit.es_scale > 0.0) {
	cli->radius *= s->s_edit.es_scale;
	s->s_edit.es_scale = 0.0;
    }

    return 0;
}

/*
 * Scale plate thickness
 */
int
ecmd_cline_scale_t(struct mged_state *s)
{
    if (s->s_edit.e_inpara != 1) {
	bu_vls_printf(s->s_edit.log_str, "ERROR: only one argument needed\n");
	s->s_edit.e_inpara = 0;
	return TCL_ERROR;
    }

    if (s->s_edit.e_para[0] <= 0.0) {
	bu_vls_printf(s->s_edit.log_str, "ERROR: SCALE FACTOR <= 0\n");
	s->s_edit.e_inpara = 0;
	return TCL_ERROR;
    }

    /* must convert to base units */
    s->s_edit.e_para[0] *= s->dbip->dbi_local2base;
    s->s_edit.e_para[1] *= s->dbip->dbi_local2base;
    s->s_edit.e_para[2] *= s->dbip->dbi_local2base;

    struct rt_cline_internal *cli =
	(struct rt_cline_internal *)s->s_edit.es_int.idb_ptr;

    RT_CLINE_CK_MAGIC(cli);

    if (s->s_edit.e_inpara)
	cli->thickness = s->s_edit.e_para[0];
    else if (s->s_edit.es_scale > 0.0) {
	cli->thickness *= s->s_edit.es_scale;
	s->s_edit.es_scale = 0.0;
    }

    return 0;
}

/*
 * Move end of height vector
 */
int
ecmd_cline_move_h(struct mged_state *s)
{
    vect_t work;
    struct rt_cline_internal *cli =
	(struct rt_cline_internal *)s->s_edit.es_int.idb_ptr;

    RT_CLINE_CK_MAGIC(cli);

    if (s->s_edit.e_inpara) {
	if (s->s_edit.e_inpara != 3) {
	    bu_vls_printf(s->s_edit.log_str, "ERROR: three arguments needed\n");
	    s->s_edit.e_inpara = 0;
	    return TCL_ERROR;
	}

	/* must convert to base units */
	s->s_edit.e_para[0] *= s->dbip->dbi_local2base;
	s->s_edit.e_para[1] *= s->dbip->dbi_local2base;
	s->s_edit.e_para[2] *= s->dbip->dbi_local2base;

	if (s->s_edit.mv_context) {
	    MAT4X3PNT(work, s->s_edit.e_invmat, s->s_edit.e_para);
	    VSUB2(cli->h, work, cli->v);
	} else
	    VSUB2(cli->h, s->s_edit.e_para, cli->v);
    }
    /* check for zero H vector */
    if (MAGNITUDE(cli->h) <= SQRT_SMALL_FASTF) {
	bu_vls_printf(s->s_edit.log_str, "Zero H vector not allowed, resetting to +Z\n");
	mged_print_result(s, TCL_ERROR);
	VSET(cli->h, 0.0, 0.0, 1.0);
	return TCL_ERROR;
    }

    return 0;
}

void
ecmd_cline_move_h_mousevec(struct mged_state *s, const vect_t mousevec)
{
    vect_t pos_view = VINIT_ZERO;	/* Unrotated view space pos */
    vect_t tr_temp = VINIT_ZERO;	/* temp translation vector */
    vect_t temp = VINIT_ZERO;
    struct rt_cline_internal *cli =
	(struct rt_cline_internal *)s->s_edit.es_int.idb_ptr;

    RT_CLINE_CK_MAGIC(cli);

    MAT4X3PNT(pos_view, view_state->vs_gvp->gv_model2view, s->s_edit.curr_e_axes_pos);
    pos_view[X] = mousevec[X];
    pos_view[Y] = mousevec[Y];
    /* Do NOT change pos_view[Z] ! */
    MAT4X3PNT(temp, view_state->vs_gvp->gv_view2model, pos_view);
    MAT4X3PNT(tr_temp, s->s_edit.e_invmat, temp);
    VSUB2(cli->h, tr_temp, cli->v);
}

int
mged_cline_edit(struct mged_state *s, int edflag)
{
    switch (edflag) {
	case SSCALE:
	    /* scale the solid uniformly about its vertex point */
	    return mged_generic_sscale(s, &s->s_edit.es_int);
	case STRANS:
	    /* translate solid */
	    mged_generic_strans(s, &s->s_edit.es_int);
	    break;
	case SROT:
	    /* rot solid about vertex */
	    mged_generic_srot(s, &s->s_edit.es_int);
	    break;
	case ECMD_CLINE_SCALE_H:
	    return ecmd_cline_scale_h(s);
	case ECMD_CLINE_SCALE_R:
	    return ecmd_cline_scale_r(s);
	case ECMD_CLINE_SCALE_T:
	    return ecmd_cline_scale_t(s);
	case ECMD_CLINE_MOVE_H:
	    return ecmd_cline_move_h(s);
    }

    return 0;
}

int
mged_cline_edit_xy(
	struct mged_state *s,
	int edflag,
	const vect_t mousevec
	)
{
    vect_t pos_view = VINIT_ZERO;       /* Unrotated view space pos */
    struct rt_db_internal *ip = &s->s_edit.es_int;

    switch (edflag) {
	case SSCALE:
	case PSCALE:
	case ECMD_CLINE_SCALE_H:
	case ECMD_CLINE_SCALE_T:
	case ECMD_CLINE_SCALE_R:
	    mged_generic_sscale_xy(s, mousevec);
	    mged_cline_edit(s, edflag);
	    return 0;
	case STRANS:
	    mged_generic_strans_xy(&pos_view, s, mousevec);
	    break;
	case ECMD_CLINE_MOVE_H:
	    ecmd_cline_move_h_mousevec(s, mousevec);
	    break;
	default:
	    bu_vls_printf(s->s_edit.log_str, "%s: XY edit undefined in solid edit mode %d\n", MGED_OBJ[ip->idb_type].ft_label, edflag);
	    mged_print_result(s, TCL_ERROR);
	    return TCL_ERROR;
    }

    update_edit_absolute_tran(s, pos_view);
    mged_cline_edit(s, edflag);

    return 0;
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
