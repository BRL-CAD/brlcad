/*                         V U T I L . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2021 United States Government as represented by
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

#define XXH_INLINE_ALL
#include "xxhash.h"

#include "./ged_private.h"

/**
 * FIXME: this routine is suspect and needs investigating.  if run
 * during view initialization, the shaders regression test fails.
 */
void
_ged_mat_aet(struct bview *gvp)
{
    mat_t tmat;
    fastf_t twist;
    fastf_t c_twist;
    fastf_t s_twist;

    bn_mat_angles(gvp->gv_rotation,
		  270.0 + gvp->gv_aet[1],
		  0.0,
		  270.0 - gvp->gv_aet[0]);

    twist = -gvp->gv_aet[2] * DEG2RAD;
    c_twist = cos(twist);
    s_twist = sin(twist);
    bn_mat_zrot(tmat, s_twist, c_twist);
    bn_mat_mul2(tmat, gvp->gv_rotation);
}


int
_ged_do_rot(struct ged *gedp,
	    char coord,
	    mat_t rmat,
	    int (*func)())
{
    mat_t temp1, temp2;

    if (func != (int (*)())0)
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
		return GED_ERROR;
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
    bview_update(gedp->ged_gvp);

    return GED_OK;
}


int
_ged_do_slew(struct ged *gedp, vect_t svec)
{
    point_t model_center;

    MAT4X3PNT(model_center, gedp->ged_gvp->gv_view2model, svec);
    MAT_DELTAS_VEC_NEG(gedp->ged_gvp->gv_center, model_center);
    bview_update(gedp->ged_gvp);

    return GED_OK;
}


int
_ged_do_tra(struct ged *gedp,
	    char coord,
	    vect_t tvec,
	    int (*func)())
{
    point_t delta;
    point_t work;
    point_t vc, nvc;

    if (func != (int (*)())0)
	return (*func)(gedp, coord, tvec);

    switch (coord) {
	case 'm':
	    VSCALE(delta, tvec, -gedp->ged_wdbp->dbip->dbi_base2local);
	    MAT_DELTAS_GET_NEG(vc, gedp->ged_gvp->gv_center);
	    break;
	case 'v':
	default:
	    VSCALE(tvec, tvec, -2.0*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_gvp->gv_isize);
	    MAT4X3PNT(work, gedp->ged_gvp->gv_view2model, tvec);
	    MAT_DELTAS_GET_NEG(vc, gedp->ged_gvp->gv_center);
	    VSUB2(delta, work, vc);
	    break;
    }

    VSUB2(nvc, vc, delta);
    MAT_DELTAS_VEC_NEG(gedp->ged_gvp->gv_center, nvc);
    bview_update(gedp->ged_gvp);

    return GED_OK;
}

unsigned long long
dl_hash(struct display_list *dl)
{
    if (!dl)
	return 0;

    XXH64_hash_t hash_val;
    XXH64_state_t *state;
    state = XXH64_createState();
    if (!state)
	return 0;
    XXH64_reset(state, 0);

    struct display_list *gdlp;
    struct display_list *next_gdlp;
    struct bview_scene_obj *sp;

    gdlp = BU_LIST_NEXT(display_list, (struct bu_list *)dl);
    while (BU_LIST_NOT_HEAD(gdlp, dl)) {
	next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	XXH64_update(state, bu_vls_cstr(&gdlp->dl_path), bu_vls_strlen(&gdlp->dl_path));
	XXH64_update(state, &gdlp->dl_wflag, sizeof(int));

	for (BU_LIST_FOR(sp, bview_scene_obj, &gdlp->dl_head_scene_obj)) {
	    XXH64_update(state, &sp->s_size, sizeof(fastf_t));
	    XXH64_update(state, &sp->s_csize, sizeof(fastf_t));
	    XXH64_update(state, &sp->s_center, sizeof(vect_t));
	    struct bn_vlist *tvp;
	    for (BU_LIST_FOR(tvp, bn_vlist, &((struct bn_vlist *)&sp->s_vlist)->l)) {
		XXH64_update(state, &tvp->nused, sizeof(size_t));
		XXH64_update(state, &tvp->cmd, sizeof(int[BN_VLIST_CHUNK]));
		XXH64_update(state, &tvp->pt, sizeof(point_t[BN_VLIST_CHUNK]));
	    }
	    XXH64_update(state, &sp->s_vlen, sizeof(int));
	    XXH64_update(state, &sp->s_fullpath.fp_len, sizeof(size_t));
	    XXH64_update(state, &sp->s_fullpath.fp_maxlen, sizeof(size_t));
	    for (size_t i = 0; i < DB_FULL_PATH_LEN(&sp->s_fullpath); i++) {
		// In principle we should check all of struct directory
		// contents, but names are unique in the database and should
		// suffice for this purpose - we care if the path has changed.
		struct directory *dp = DB_FULL_PATH_GET(&sp->s_fullpath, i);
		XXH64_update(state, &dp->d_namep, strlen(dp->d_namep));
	    }
	    //XXH64_update(state, &sp->s_fullpath, sizeof(struct db_full_path));
	    XXH64_update(state, &sp->s_flag, sizeof(char));
	    XXH64_update(state, &sp->s_iflag, sizeof(char));
	    XXH64_update(state, &sp->s_soldash, sizeof(char));
	    XXH64_update(state, &sp->s_Eflag, sizeof(char));
	    XXH64_update(state, &sp->s_uflag, sizeof(char));
	    XXH64_update(state, &sp->s_dflag, sizeof(char));
	    XXH64_update(state, &sp->s_cflag, sizeof(char));
	    XXH64_update(state, &sp->s_wflag, sizeof(char));
	    XXH64_update(state, &sp->s_basecolor, sizeof(unsigned char[3]));
	    XXH64_update(state, &sp->s_color, sizeof(unsigned char[3]));
	    XXH64_update(state, &sp->s_regionid, sizeof(short));
	    XXH64_update(state, &sp->s_dlist, sizeof(unsigned int));
	    XXH64_update(state, &sp->s_transparency, sizeof(fastf_t));
	    XXH64_update(state, &sp->s_dmode, sizeof(int));
	    XXH64_update(state, &sp->s_hiddenLine, sizeof(int));
	    XXH64_update(state, &sp->s_mat, sizeof(mat_t));
	}

	gdlp = next_gdlp;
    }

    hash_val = XXH64_digest(state);
    XXH64_freeState(state);

    return (unsigned long long)hash_val;
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
