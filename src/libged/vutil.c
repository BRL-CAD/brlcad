/*                         V U T I L . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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
/** @file libged/vutil.c
 *
 * This file contains view related utility functions.
 *
 */

#include "common.h"

#include <string.h>

#include "bsg/feature.h"
#include "nmg/display.h"
#include "./ged_private.h"
#include "ged/view.h"
#include "ged/bsg_ged_draw.h"

int
_ged_do_rot(struct ged *gedp,
	    char coord,
	    mat_t rmat,
	    int (*func)(struct ged *, char, char, mat_t))
{
    mat_t temp1, temp2;

    if (func != (int (*)(struct ged *, char, char, mat_t))0)
	return (*func)(gedp, coord, gedp->ged_gvp->gv_rotate_about, rmat);

    switch (coord) {
	case 'm':
	    /* transform model rotations into view rotations */
	    bn_mat_inv(temp1, gedp->ged_gvp->gv_rotation);
	    bn_mat_mul(temp2, gedp->ged_gvp->gv_rotation, rmat);
	    bn_mat_mul(rmat, temp2, temp1);
	    break;
	case 'v':
	default:
	    break;
    }

    /* Calculate new view center */
    if (gedp->ged_gvp->gv_rotate_about != 'v') {
	point_t rot_pt;
	point_t new_origin;
	mat_t viewchg, viewchginv;
	point_t new_cent_view;
	point_t new_cent_model;

	switch (gedp->ged_gvp->gv_rotate_about) {
	    case 'e':
		VSET(rot_pt, 0.0, 0.0, 1.0);
		break;
	    case 'k':
		MAT4X3PNT(rot_pt, gedp->ged_gvp->gv_model2view, gedp->ged_gvp->gv_keypoint);
		break;
	    case 'm':
		/* rotate around model center (0, 0, 0) */
		VSET(new_origin, 0.0, 0.0, 0.0);
		MAT4X3PNT(rot_pt, gedp->ged_gvp->gv_model2view, new_origin);
		break;
	    default:
		return BRLCAD_ERROR;
	}

	bn_mat_xform_about_pnt(viewchg, rmat, rot_pt);
	bn_mat_inv(viewchginv, viewchg);

	/* Convert origin in new (viewchg) coords back to old view coords */
	VSET(new_origin, 0.0, 0.0, 0.0);
	MAT4X3PNT(new_cent_view, viewchginv, new_origin);
	MAT4X3PNT(new_cent_model, gedp->ged_gvp->gv_view2model, new_cent_view);
	MAT_DELTAS_VEC_NEG(gedp->ged_gvp->gv_center, new_cent_model);
    }

    /* pure rotation */
    bn_mat_mul2(rmat, gedp->ged_gvp->gv_rotation);
    bsg_update(gedp->ged_gvp);

    return BRLCAD_OK;
}


int
_ged_do_slew(struct ged *gedp, vect_t svec)
{
    point_t model_center;

    MAT4X3PNT(model_center, gedp->ged_gvp->gv_view2model, svec);
    MAT_DELTAS_VEC_NEG(gedp->ged_gvp->gv_center, model_center);
    bsg_update(gedp->ged_gvp);

    return BRLCAD_OK;
}


int
_ged_do_tra(struct ged *gedp,
	    char coord,
	    vect_t tvec,
	    int (*func)(struct ged *, char, vect_t))
{
    point_t delta;
    point_t work;
    point_t vc, nvc;

    if (func != (int (*)(struct ged *, char, vect_t))0)
	return (*func)(gedp, coord, tvec);

    switch (coord) {
	case 'm':
	    VSCALE(delta, tvec, -gedp->dbip->dbi_base2local);
	    MAT_DELTAS_GET_NEG(vc, gedp->ged_gvp->gv_center);
	    break;
	case 'v':
	default:
	    VSCALE(tvec, tvec, -2.0*gedp->dbip->dbi_base2local*gedp->ged_gvp->gv_isize);
	    MAT4X3PNT(work, gedp->ged_gvp->gv_view2model, tvec);
	    MAT_DELTAS_GET_NEG(vc, gedp->ged_gvp->gv_center);
	    VSUB2(delta, work, vc);
	    break;
    }

    VSUB2(nvc, vc, delta);
    MAT_DELTAS_VEC_NEG(gedp->ged_gvp->gv_center, nvc);
    bsg_update(gedp->ged_gvp);

    return BRLCAD_OK;
}

void
nmg_plot_eu(struct ged *gedp, struct edgeuse *es_eu, const struct bn_tol *tol)
{
    if (!gedp || !es_eu || !tol)
	return;

    if (*es_eu->g.magic_p != NMG_EDGE_G_LSEG_MAGIC)
	return;

    struct model *m = nmg_find_model(&es_eu->l.magic);
    NMG_CK_MODEL(m);

    /* get space for list of items processed */
    long *tab = (long *)bu_calloc(m->maxindex+1, sizeof(long), "nmg_ed tab[]");
    struct bsg_line_layer_builder *plot = bsg_line_layer_builder_create();

    nmg_line_layer_around_eu(plot, es_eu, tab, 1, tol);
    if (gedp->ged_gvp) {
	(void)bsg_feature_replace_line_layer_builder(gedp->ged_gvp,
		"nmg::_EU_", 0, plot, NULL);
    }

    bsg_line_layer_builder_free(plot);
    bu_free((void *)tab, "nmg_ed tab[]");
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
