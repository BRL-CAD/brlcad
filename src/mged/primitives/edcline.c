/*                         E D C L I N E . C
 * BRL-CAD
 *
 * Copyright (c) 1996-2024 United States Government as represented by
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
#include "./edcline.h"

static void
cline_ed(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    es_edflag = arg;
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

/*
 * Scale height vector
 */
void
ecmd_cline_scale_h(struct mged_state *s)
{
    struct rt_cline_internal *cli =
	(struct rt_cline_internal *)s->edit_state.es_int.idb_ptr;

    RT_CLINE_CK_MAGIC(cli);

    if (inpara) {
	es_para[0] *= es_mat[15];
	s->edit_state.es_scale = es_para[0] / MAGNITUDE(cli->h);
	VSCALE(cli->h, cli->h, s->edit_state.es_scale);
    } else if (s->edit_state.es_scale > 0.0) {
	VSCALE(cli->h, cli->h, s->edit_state.es_scale);
	s->edit_state.es_scale = 0.0;
    }
}

/*
 * Scale radius
 */
void
ecmd_cline_scale_r(struct mged_state *s)
{
    struct rt_cline_internal *cli =
	(struct rt_cline_internal *)s->edit_state.es_int.idb_ptr;

    RT_CLINE_CK_MAGIC(cli);

    if (inpara)
	cli->radius = es_para[0];
    else if (s->edit_state.es_scale > 0.0) {
	cli->radius *= s->edit_state.es_scale;
	s->edit_state.es_scale = 0.0;
    }
}

/*
 * Scale plate thickness
 */
void
ecmd_cline_scale_t(struct mged_state *s)
{
    struct rt_cline_internal *cli =
	(struct rt_cline_internal *)s->edit_state.es_int.idb_ptr;

    RT_CLINE_CK_MAGIC(cli);

    if (inpara)
	cli->thickness = es_para[0];
    else if (s->edit_state.es_scale > 0.0) {
	cli->thickness *= s->edit_state.es_scale;
	s->edit_state.es_scale = 0.0;
    }
}

/*
 * Move end of height vector
 */
void
ecmd_cline_move_h(struct mged_state *s)
{
    vect_t work;
    struct rt_cline_internal *cli =
	(struct rt_cline_internal *)s->edit_state.es_int.idb_ptr;

    RT_CLINE_CK_MAGIC(cli);

    if (inpara) {
	if (mged_variables->mv_context) {
	    MAT4X3PNT(work, es_invmat, es_para);
	    VSUB2(cli->h, work, cli->v);
	} else
	    VSUB2(cli->h, es_para, cli->v);
    }
    /* check for zero H vector */
    if (MAGNITUDE(cli->h) <= SQRT_SMALL_FASTF) {
	Tcl_AppendResult(s->interp, "Zero H vector not allowed, resetting to +Z\n",
		(char *)NULL);
	mged_print_result(s, TCL_ERROR);
	VSET(cli->h, 0.0, 0.0, 1.0);
	return;
    }
}

void
ecmd_cline_move_h_mousevec(struct mged_state *s, const vect_t mousevec)
{
    vect_t pos_view = VINIT_ZERO;	/* Unrotated view space pos */
    vect_t tr_temp = VINIT_ZERO;	/* temp translation vector */
    vect_t temp = VINIT_ZERO;
    struct rt_cline_internal *cli =
	(struct rt_cline_internal *)s->edit_state.es_int.idb_ptr;

    RT_CLINE_CK_MAGIC(cli);

    MAT4X3PNT(pos_view, view_state->vs_gvp->gv_model2view, curr_e_axes_pos);
    pos_view[X] = mousevec[X];
    pos_view[Y] = mousevec[Y];
    /* Do NOT change pos_view[Z] ! */
    MAT4X3PNT(temp, view_state->vs_gvp->gv_view2model, pos_view);
    MAT4X3PNT(tr_temp, es_invmat, temp);
    VSUB2(cli->h, tr_temp, cli->v);
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
