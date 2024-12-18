/*                         V U T I L . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2024 United States Government as represented by
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

#include "./ged_private.h"
#include "ged/view/ged_view_tmp.h"

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
    bv_update(gedp->ged_gvp);

    return BRLCAD_OK;
}


int
_ged_do_slew(struct ged *gedp, vect_t svec)
{
    point_t model_center;

    MAT4X3PNT(model_center, gedp->ged_gvp->gv_view2model, svec);
    MAT_DELTAS_VEC_NEG(gedp->ged_gvp->gv_center, model_center);
    bv_update(gedp->ged_gvp);

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
    bv_update(gedp->ged_gvp);

    return BRLCAD_OK;
}

unsigned long long
ged_dl_hash(struct display_list *dl)
{
    if (!dl)
	return 0;

    struct bu_data_hash_state *state = bu_data_hash_create();
    if (!state)
	return 0;

    struct display_list *gdlp;
    struct display_list *next_gdlp;
    struct bv_scene_obj *sp;

    gdlp = BU_LIST_NEXT(display_list, (struct bu_list *)dl);
    while (BU_LIST_NOT_HEAD(gdlp, dl)) {
	next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	for (BU_LIST_FOR(sp, bv_scene_obj, &gdlp->dl_head_scene_obj)) {
	    if (!sp->s_u_data)
		continue;
	    struct ged_bv_data *bdata = (struct ged_bv_data *)sp->s_u_data;
	    bu_data_hash_update(state, &bdata->s_fullpath.fp_len, sizeof(size_t));
	    bu_data_hash_update(state, &bdata->s_fullpath.fp_maxlen, sizeof(size_t));
	    for (size_t i = 0; i < DB_FULL_PATH_LEN(&bdata->s_fullpath); i++) {
		// In principle we should check all of struct directory
		// contents, but names are unique in the database and should
		// suffice for this purpose - we care if the path has changed.
		struct directory *dp = DB_FULL_PATH_GET(&bdata->s_fullpath, i);
		bu_data_hash_update(state, &dp->d_namep, strlen(dp->d_namep));
	    }
	}
	gdlp = next_gdlp;
    }

    unsigned long long hash_val = bu_data_hash_val(state);
    bu_data_hash_destroy(state);

    return hash_val;
}

void
nmg_plot_eu(struct ged *gedp, struct edgeuse *es_eu, struct bn_tol *tol, struct bu_list *vlfree)
{
    if (!gedp || !es_eu || !tol)
	return;

    if (*es_eu->g.magic_p != NMG_EDGE_G_LSEG_MAGIC)
	return;

    struct model *m = nmg_find_model(&es_eu->l.magic);
    NMG_CK_MODEL(m);

    /* get space for list of items processed */
    long *tab = (long *)bu_calloc(m->maxindex+1, sizeof(long), "nmg_ed tab[]");
    struct bv_vlblock *vbp = rt_vlblock_init();

    nmg_vlblock_around_eu(vbp, es_eu, tab, 1, vlfree, tol);
    _ged_cvt_vlblock_to_solids(gedp, vbp, "_EU_", 0);      /* swipe vlist */

    bv_vlblock_free(vbp);
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
