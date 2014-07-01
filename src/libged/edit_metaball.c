/*                         E D I T _ M E T A B A L L . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2014 United States Government as represented by
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
/** @file libged/edit_metaball.c
 *
 * Functions -
 *
 *
 */

#include "common.h"

#include <string.h>
#include "bio.h"

#include "bu/cmd.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"

#include "./ged_private.h"


#define GED_METABALL_SCALE(_d, _scale) \
    if ((_scale) < 0.0) \
	(_d) = -(_scale); \
    else		  \
	(_d) *= (_scale);


/*
 * Returns the index for the metaball point matching mbpp.
 */
int
_ged_get_metaball_i_pt(struct rt_metaball_internal *mbip, struct wdb_metaballpt *mbpp)
{
    struct wdb_metaballpt *curr_mbpp;
    int mbp_i = 0;

    for (BU_LIST_FOR(curr_mbpp, wdb_metaballpt, &mbip->metaball_ctrl_head)) {
	if (curr_mbpp == mbpp)
	    return mbp_i;

	++mbp_i;
    }

    return -1;
}


/*
 * Returns point mbp_i.
 */
struct wdb_metaballpt *
_ged_get_metaball_pt_i(struct rt_metaball_internal *mbip, int mbp_i)
{
    int i = 0;
    struct wdb_metaballpt *curr_mbpp;

    for (BU_LIST_FOR(curr_mbpp, wdb_metaballpt, &mbip->metaball_ctrl_head)) {
	if (i == mbp_i)
	    return curr_mbpp;

	++i;
    }

    return (struct wdb_metaballpt *)NULL;
}


int
_ged_set_metaball(struct ged *gedp, struct rt_metaball_internal *mbip, const char *attribute, fastf_t sf)
{
    RT_METABALL_CK_MAGIC(mbip);

    switch (attribute[0]) {
	case 'm':
	case 'M':
	    if (sf <= METABALL_METABALL)
		mbip->method = METABALL_METABALL;
	    else if (sf >= METABALL_BLOB)
		mbip->method = METABALL_BLOB;
	    else
		mbip->method = (int)sf;

	    break;
	case 't':
	case 'T':
	    if (sf < 0)
		mbip->threshold = -sf;
	    else
		mbip->threshold = sf;

	    break;
	default:
	    bu_vls_printf(gedp->ged_result_str, "bad metaball attribute - %s", attribute);
	    return GED_ERROR;
    }

    return GED_OK;
}


int
_ged_scale_metaball(struct ged *gedp, struct rt_metaball_internal *mbip, const char *attribute, fastf_t sf, int rflag)
{
    int mbp_i;
    struct wdb_metaballpt *mbpp;

    RT_METABALL_CK_MAGIC(mbip);

    if (!rflag && sf > 0)
	sf = -sf;

    switch (attribute[0]) {
	case 'f':
	case 'F':
	    if (sscanf(attribute+1, "%d", &mbp_i) != 1)
		mbp_i = 0;

	    if ((mbpp = _ged_get_metaball_pt_i(mbip, mbp_i)) == (struct wdb_metaballpt *)NULL)
		return GED_ERROR;

	    BU_CKMAG(mbpp, WDB_METABALLPT_MAGIC, "wdb_metaballpt");
	    GED_METABALL_SCALE(mbpp->fldstr, sf);

	    break;
	case 's':
	case 'S':
	    if (sscanf(attribute+1, "%d", &mbp_i) != 1)
		mbp_i = 0;

	    if ((mbpp = _ged_get_metaball_pt_i(mbip, mbp_i)) == (struct wdb_metaballpt *)NULL)
		return GED_ERROR;

	    BU_CKMAG(mbpp, WDB_METABALLPT_MAGIC, "wdb_metaballpt");
	    GED_METABALL_SCALE(mbpp->sweat, sf);

	    break;
	default:
	    bu_vls_printf(gedp->ged_result_str, "bad metaball attribute - %s", attribute);
	    return GED_ERROR;
    }

    return GED_OK;
}


struct wdb_metaballpt *
find_metaballpt_nearest_pt(const struct bu_list *metaball_hd, const point_t model_pt, matp_t view2model)
{
    struct wdb_metaballpt *mbpp;
    struct wdb_metaballpt *nearest=(struct wdb_metaballpt *)NULL;
    struct bn_tol tmp_tol;
    fastf_t min_dist = MAX_FASTF;
    vect_t dir, work;

    tmp_tol.magic = BN_TOL_MAGIC;
    tmp_tol.dist = 0.0;
    tmp_tol.dist_sq = tmp_tol.dist * tmp_tol.dist;
    tmp_tol.perp = 0.0;
    tmp_tol.para = 1.0 - tmp_tol.perp;

    /* get a direction vector in model space corresponding to z-direction in view */
    VSET(work, 0.0, 0.0, 1.0);
    MAT4X3VEC(dir, view2model, work);

    for (BU_LIST_FOR(mbpp, wdb_metaballpt, metaball_hd)) {
	fastf_t dist;

	dist = bn_dist_line3_pt3(model_pt, dir, mbpp->coord);
	if (dist < min_dist) {
	    min_dist = dist;
	    nearest = mbpp;
	}
    }

    return nearest;
}


int
ged_find_metaballpt_nearest_pt(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    static const char *usage = "metaball x y z";
    struct rt_db_internal intern;
    struct wdb_metaballpt *nearest;
    point_t model_pt;
    double scan[3];
    mat_t mat;
    int pt_i;
    const char *last;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 3 && argc != 5) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if ((last = strrchr(argv[1], '/')) == NULL)
	last = argv[1];
    else
	++last;

    if (last[0] == '\0') {
	bu_vls_printf(gedp->ged_result_str, "%s: illegal input - %s", argv[0], argv[1]);
	return GED_ERROR;
    }

    dp = db_lookup(gedp->ged_wdbp->dbip, last, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: failed to find %s", argv[0], argv[1]);
	return GED_ERROR;
    }

    if (argc == 3) {
	if (sscanf(argv[2], "%lf %lf %lf", &scan[X], &scan[Y], &scan[Z]) != 3) {
	    bu_vls_printf(gedp->ged_result_str, "%s: bad point - %s", argv[0], argv[2]);
	    return GED_ERROR;
	}
    } else if (sscanf(argv[2], "%lf", &scan[X]) != 1 ||
	       sscanf(argv[3], "%lf", &scan[Y]) != 1 ||
	       sscanf(argv[4], "%lf", &scan[Z]) != 1) {
	bu_vls_printf(gedp->ged_result_str, "%s: bad X, Y or Z", argv[0]);
	return GED_ERROR;
    }
    /* convert from double to fastf_t */
    VMOVE(model_pt, scan);

    if (wdb_import_from_path2(gedp->ged_result_str, &intern, argv[1], gedp->ged_wdbp, mat) == GED_ERROR)
	return GED_ERROR;

    nearest = find_metaballpt_nearest_pt(&((struct rt_metaball_internal *)intern.idb_ptr)->metaball_ctrl_head,
				     model_pt, gedp->ged_gvp->gv_view2model);
    pt_i = _ged_get_metaball_i_pt((struct rt_metaball_internal *)intern.idb_ptr, nearest);
    rt_db_free_internal(&intern);

    if (pt_i < 0) {
	bu_vls_printf(gedp->ged_result_str, "%s: failed to find point for %s", argv[0], argv[1]);
	return GED_ERROR;
    }

    bu_vls_printf(gedp->ged_result_str, "%d", pt_i);
    return GED_OK;
}


struct wdb_metaballpt *
_ged_add_metaballpt(struct rt_metaball_internal *mbip, struct wdb_metaballpt *mbp, const point_t new_pt)
{
    struct wdb_metaballpt *last;
    struct wdb_metaballpt *newmbp;

    RT_METABALL_CK_MAGIC(mbip);

    if (mbp) {
	BU_CKMAG(mbp, WDB_METABALLPT_MAGIC, "metaball point");
	last = mbp;
    } else {
	/* add new point to end of metaball solid */
	last = BU_LIST_LAST(wdb_metaballpt, &mbip->metaball_ctrl_head);

	if (last->l.magic == BU_LIST_HEAD_MAGIC) {
	    BU_GET(newmbp, struct wdb_metaballpt);
	    newmbp->l.magic = WDB_METABALLPT_MAGIC;
	    newmbp->fldstr = 1.0;
	    newmbp->sweat = 1.0;
	    VMOVE(newmbp->coord, new_pt);
	    BU_LIST_INSERT(&mbip->metaball_ctrl_head, &newmbp->l);
	    return newmbp;
	}
    }

    /* build new point */
    BU_GET(newmbp, struct wdb_metaballpt);
    newmbp->l.magic = WDB_METABALLPT_MAGIC;
    newmbp->fldstr = 1.0;
    newmbp->sweat = 1.0;
    VMOVE(newmbp->coord, new_pt);

    if (mbp) {
	/* append after current point */
	BU_LIST_APPEND(&mbp->l, &newmbp->l);
    } else {
	/* add to end of metaball solid */
	BU_LIST_INSERT(&mbip->metaball_ctrl_head, &newmbp->l);
    }

    return newmbp;
}


int
ged_add_metaballpt(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    static const char *usage = "metaball pt";
    struct rt_db_internal intern;
    struct rt_metaball_internal *mbip;
    mat_t mat;
    point_t view_mb_pt;
    point_t view_coord;
    point_t mb_pt;
    struct wdb_metaballpt *lastmbp;
    double scan[3];
    const char *last;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if ((last = strrchr(argv[1], '/')) == NULL)
	last = argv[1];
    else
	++last;

    if (last[0] == '\0') {
	bu_vls_printf(gedp->ged_result_str, "%s: illegal input - %s", argv[0], argv[1]);
	return GED_ERROR;
    }

    dp = db_lookup(gedp->ged_wdbp->dbip, last, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: failed to find %s", argv[0], argv[1]);
	return GED_ERROR;
    }

    if (sscanf(argv[2], "%lf %lf %lf", &scan[X], &scan[Y], &scan[Z]) != 3) {
	bu_vls_printf(gedp->ged_result_str, "%s: bad point - %s", argv[0], argv[2]);
	return GED_ERROR;
    }
    /* convert from double to fastf_t */
    VMOVE(view_mb_pt, scan);

    if (wdb_import_from_path2(gedp->ged_result_str, &intern, argv[1], gedp->ged_wdbp, mat) == GED_ERROR)
	return GED_ERROR;

    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD ||
	intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_METABALL) {
	bu_vls_printf(gedp->ged_result_str, "Object not a METABALL");
	rt_db_free_internal(&intern);

	return GED_ERROR;
    }

    mbip = (struct rt_metaball_internal *)intern.idb_ptr;

    /* use the view z from the last metaball point */
    lastmbp = BU_LIST_LAST(wdb_metaballpt, &mbip->metaball_ctrl_head);

    MAT4X3PNT(view_coord, gedp->ged_gvp->gv_model2view, lastmbp->coord);
    view_mb_pt[Z] = view_coord[Z];
    MAT4X3PNT(mb_pt, gedp->ged_gvp->gv_view2model, view_mb_pt);

    if (_ged_add_metaballpt(mbip, (struct wdb_metaballpt *)NULL, mb_pt) == (struct wdb_metaballpt *)NULL) {
	rt_db_free_internal(&intern);
	bu_vls_printf(gedp->ged_result_str, "%s: cannot move point there", argv[0]);
	return GED_ERROR;
    }

    {
	mat_t invmat;
	struct wdb_metaballpt *curr_mbp;
	point_t curr_pt;

	bn_mat_inv(invmat, mat);
	for (BU_LIST_FOR(curr_mbp, wdb_metaballpt, &mbip->metaball_ctrl_head)) {
	    MAT4X3PNT(curr_pt, invmat, curr_mbp->coord);
	    VMOVE(curr_mbp->coord, curr_pt);
	}

	GED_DB_PUT_INTERNAL(gedp, dp, &intern, &rt_uniresource, GED_ERROR);
    }

    rt_db_free_internal(&intern);
    return GED_OK;
}


struct wdb_metaballpt *
_ged_delete_metaballpt(struct wdb_metaballpt *mbp)
{
    struct wdb_metaballpt *next;
    struct wdb_metaballpt *prev;
    struct wdb_metaballpt *head;

    BU_CKMAG(mbp, WDB_METABALLPT_MAGIC, "metaball point");

    /* Find the head */
    head = mbp;
    while (head->l.magic != BU_LIST_HEAD_MAGIC)
	head = BU_LIST_NEXT(wdb_metaballpt, &head->l);

    next = BU_LIST_NEXT(wdb_metaballpt, &mbp->l);
    if (next->l.magic == BU_LIST_HEAD_MAGIC)
	next = (struct wdb_metaballpt *)NULL;

    prev = BU_LIST_PREV(wdb_metaballpt, &mbp->l);
    if (prev->l.magic == BU_LIST_HEAD_MAGIC)
	prev = (struct wdb_metaballpt *)NULL;

    if (!prev && !next) {
	return mbp;
    }

    BU_LIST_DEQUEUE(&mbp->l);

    BU_PUT(mbp, struct wdb_metaballpt);

    if (prev)
	return prev;
    else
	return next;

}


int
ged_delete_metaballpt(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    static const char *usage = "metaball pt_i";
    struct rt_db_internal intern;
    struct wdb_metaballpt *mbp;
    struct rt_metaball_internal *mbip;
    int pt_i;
    const char *last;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if ((last = strrchr(argv[1], '/')) == NULL)
	last = argv[1];
    else
	++last;

    if (last[0] == '\0') {
	bu_vls_printf(gedp->ged_result_str, "%s: illegal input - %s", argv[0], argv[1]);
	return GED_ERROR;
    }

    dp = db_lookup(gedp->ged_wdbp->dbip, last, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: failed to find %s", argv[0], argv[1]);
	return GED_ERROR;
    }

    if (sscanf(argv[2], "%d", &pt_i) != 1) {
	bu_vls_printf(gedp->ged_result_str, "%s: bad metaball point index - %s", argv[0], argv[3]);
	return GED_ERROR;
    }

    if (rt_db_get_internal(&intern, dp, gedp->ged_wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
	bu_vls_printf(gedp->ged_result_str, "%s: failed to get internal for %s", argv[0], argv[1]);
	return GED_ERROR;
    }

    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD ||
	intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_METABALL) {
	bu_vls_printf(gedp->ged_result_str, "%s is not a METABALL", argv[1]);
	rt_db_free_internal(&intern);

	return GED_ERROR;
    }

    mbip = (struct rt_metaball_internal *)intern.idb_ptr;
    if ((mbp = _ged_get_metaball_pt_i(mbip, pt_i)) == (struct wdb_metaballpt *)NULL) {
	rt_db_free_internal(&intern);
	bu_vls_printf(gedp->ged_result_str, "%s: bad metaball point index - %s", argv[0], argv[2]);
	return GED_ERROR;
    }

    if (_ged_delete_metaballpt(mbp) == mbp) {
	rt_db_free_internal(&intern);
	bu_vls_printf(gedp->ged_result_str, "%s: cannot delete last metaball point %d", argv[0], pt_i);
	return GED_ERROR;
    }

    GED_DB_PUT_INTERNAL(gedp, dp, &intern, &rt_uniresource, GED_ERROR);

    rt_db_free_internal(&intern);
    return GED_OK;
}


int
ged_move_metaballpt(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    static const char *usage = "[-r] metaball seg_i pt";
    struct rt_db_internal intern;
    struct wdb_metaballpt *mbp;
    struct rt_metaball_internal *mbip;
    mat_t mat;
    point_t mb_pt;
    double scan[3];
    int seg_i;
    int rflag = 0;
    const char *last;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 4 || 5 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if (argc == 5) {
	if (argv[1][0] != '-' || argv[1][1] != 'r' || argv[1][2] != '\0') {
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	    return GED_ERROR;
	}

	rflag = 1;
	--argc;
	++argv;
    }

    if ((last = strrchr(argv[1], '/')) == NULL)
	last = argv[1];
    else
	++last;

    if (last[0] == '\0') {
	bu_vls_printf(gedp->ged_result_str, "%s: illegal input - %s", argv[0], argv[1]);
	return GED_ERROR;
    }

    dp = db_lookup(gedp->ged_wdbp->dbip, last, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: failed to find %s", argv[0], argv[1]);
	return GED_ERROR;
    }

    if (sscanf(argv[2], "%d", &seg_i) != 1) {
	bu_vls_printf(gedp->ged_result_str, "%s: bad metaball point index - %s", argv[0], argv[2]);
	return GED_ERROR;
    }

    if (sscanf(argv[3], "%lf %lf %lf", &scan[X], &scan[Y], &scan[Z]) != 3) {
	bu_vls_printf(gedp->ged_result_str, "%s: bad point - %s", argv[0], argv[3]);
	return GED_ERROR;
    }
    VSCALE(mb_pt, scan, gedp->ged_wdbp->dbip->dbi_local2base);

    if (wdb_import_from_path2(gedp->ged_result_str, &intern, argv[1], gedp->ged_wdbp, mat) == GED_ERROR)
	return GED_ERROR;

    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD ||
	intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_METABALL) {
	bu_vls_printf(gedp->ged_result_str, "Object not a METABALL");
	rt_db_free_internal(&intern);

	return GED_ERROR;
    }

    mbip = (struct rt_metaball_internal *)intern.idb_ptr;
    if ((mbp = _ged_get_metaball_pt_i(mbip, seg_i)) == (struct wdb_metaballpt *)NULL) {
	rt_db_free_internal(&intern);
	bu_vls_printf(gedp->ged_result_str, "%s: bad metaball point index - %s", argv[0], argv[2]);
	return GED_ERROR;
    }

    if (rflag) {
	VADD2(mb_pt, mb_pt, mbp->coord);
    }

    VMOVE(mbp->coord, mb_pt);

    {
	mat_t invmat;
	struct wdb_metaballpt *curr_mbp;
	point_t curr_pt;

	bn_mat_inv(invmat, mat);
	for (BU_LIST_FOR(curr_mbp, wdb_metaballpt, &mbip->metaball_ctrl_head)) {
	    MAT4X3PNT(curr_pt, invmat, curr_mbp->coord);
	    VMOVE(curr_mbp->coord, curr_pt);
	}

	GED_DB_PUT_INTERNAL(gedp, dp, &intern, &rt_uniresource, GED_ERROR);
    }

    rt_db_free_internal(&intern);
    return GED_OK;
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
