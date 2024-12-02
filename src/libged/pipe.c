/*                        P I P E . C
 * BRL-CAD
 *
 * Copyright (c) 1995-2024 United States Government as represented by
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
/** @file libged/edpipe.c
 *
 * Functions -
 *
 * pipe_split_pnt - split a pipe segment at a given point
 *
 * find_pipe_pnt_nearest_pnt - find which segment of a pipe is nearest
 * the ray from "pt" in the viewing direction (for segment selection
 * in MGED)
 */

#include "common.h"

#include <math.h>
#include <string.h>

#include "vmath.h"
#include "nmg.h"
#include "rt/geom.h"
#include "ged.h"
#include "wdb.h"

#include "./ged_private.h"

void
pipe_split_pnt(struct bu_list *UNUSED(pipe_hd), struct wdb_pipe_pnt *UNUSED(ps), fastf_t *UNUSED(new_pt))
{
    bu_log("WARNING: pipe splitting unimplemented\n");
}


static fastf_t
edpipe_scale(fastf_t d, fastf_t scale)
{
    if (scale < 0.0) {
	/* negative value sets the scale */
	return (-scale);
    }

    /* positive value gets multiplied */
    return (d * scale);
}


void
pipe_scale_od(struct rt_pipe_internal *pipeip, fastf_t scale)
{
    struct wdb_pipe_pnt *ps;

    RT_PIPE_CK_MAGIC(pipeip);

    /* check that this can be done */
    for (BU_LIST_FOR(ps, wdb_pipe_pnt, &pipeip->pipe_segs_head)) {
	fastf_t tmp_od;

	tmp_od = edpipe_scale(ps->pp_od, scale);

	if (ps->pp_id > tmp_od) {
	    /* Silently ignore */
	    return;
	}
	if (tmp_od > 2.0*ps->pp_bendradius) {
	    /* Silently ignore */
	    return;
	}
    }

    for (BU_LIST_FOR(ps, wdb_pipe_pnt, &pipeip->pipe_segs_head)) {
	ps->pp_od = edpipe_scale(ps->pp_od, scale);
    }
}


void
pipe_scale_id(struct rt_pipe_internal *pipeip, fastf_t scale)
{
    struct wdb_pipe_pnt *ps;

    RT_PIPE_CK_MAGIC(pipeip);

    /* check that this can be done */
    for (BU_LIST_FOR(ps, wdb_pipe_pnt, &pipeip->pipe_segs_head)) {
	fastf_t tmp_id;

	tmp_id = edpipe_scale(ps->pp_id, scale);

	if (ps->pp_od < tmp_id) {
	    /* Silently ignore */
	    return;
	}
	if (tmp_id > 2.0*ps->pp_bendradius) {
	    /* Silently ignore */
	    return;
	}
    }

    for (BU_LIST_FOR(ps, wdb_pipe_pnt, &pipeip->pipe_segs_head)) {
	ps->pp_id = edpipe_scale(ps->pp_id, scale);
    }
}


void
pipe_seg_scale_od(struct wdb_pipe_pnt *ps, fastf_t scale)
{
    fastf_t tmp_od;

    BU_CKMAG(ps, WDB_PIPESEG_MAGIC, "pipe segment");

    tmp_od = edpipe_scale(ps->pp_od, scale);

    /* need to check that the new OD is not less than ID
     * of any affected segment.
     */
    if (ps->pp_id > tmp_od) {
	bu_log("Cannot make OD smaller than ID\n");
	return;
    }
    if (tmp_od > 2.0*ps->pp_bendradius) {
	bu_log("Cannot make outer radius greater than bend radius\n");
	return;
    }

    ps->pp_od = edpipe_scale(ps->pp_od, scale);
}


void
pipe_seg_scale_id(struct wdb_pipe_pnt *ps, fastf_t scale)
{
    fastf_t tmp_id;

    BU_CKMAG(ps, WDB_PIPESEG_MAGIC, "pipe segment");

    tmp_id = edpipe_scale(ps->pp_id, scale);

    /* need to check that the new ID is not greater than OD */
    if (ps->pp_od < tmp_id) {
	bu_log( "Cannot make ID greater than OD\n");
	return;
    }
    if (tmp_id > 2.0*ps->pp_bendradius) {
	bu_log("Cannot make inner radius greater than bend radius\n");
	return;
    }

    ps->pp_id = edpipe_scale(ps->pp_id, scale);
}


void
pipe_seg_scale_radius(struct wdb_pipe_pnt *ps, fastf_t scale)
{
    fastf_t old_radius;
    struct wdb_pipe_pnt *head;

    BU_CKMAG(ps, WDB_PIPESEG_MAGIC, "pipe point");

    head = ps;
    while (head->l.magic != BU_LIST_HEAD_MAGIC)
	head = BU_LIST_NEXT(wdb_pipe_pnt, &head->l);

    /* make sure we can make this change */
    old_radius = ps->pp_bendradius;

    ps->pp_bendradius = edpipe_scale(ps->pp_bendradius, scale);

    if (ps->pp_bendradius < ps->pp_od * 0.5) {
	bu_log("Cannot make bend radius less than pipe outer radius\n");
	ps->pp_bendradius = old_radius;
	return;
    }

    if (rt_pipe_ck(&head->l)) {
	/* won't work, go back to original radius */
	ps->pp_bendradius = old_radius;
	return;
    }
}


void
pipe_scale_radius(struct rt_pipe_internal *pipeip, fastf_t scale)
{
    struct bu_list head;
    struct wdb_pipe_pnt *old_ps, *new_ps;

    RT_PIPE_CK_MAGIC(pipeip);

    /* make a quick check for minimum bend radius */
    for (BU_LIST_FOR(old_ps, wdb_pipe_pnt, &pipeip->pipe_segs_head)) {
	if (scale < 0.0) {
	    if ((-scale) < old_ps->pp_od * 0.5) {
		bu_log("Cannot make bend radius less than pipe outer radius\n");
		return;
	    }
	} else {
	    if (old_ps->pp_bendradius * scale < old_ps->pp_od * 0.5) {
		bu_log("Cannot make bend radius less than pipe outer radius\n");
		return;
	    }
	}
    }

    /* make temporary copy of this pipe solid */
    BU_LIST_INIT(&head);
    for (BU_LIST_FOR(old_ps, wdb_pipe_pnt, &pipeip->pipe_segs_head)) {
	BU_GET(new_ps, struct wdb_pipe_pnt);
	*new_ps = (*old_ps);
	BU_LIST_APPEND(&head, &new_ps->l);
    }

    /* make the desired editing changes to the copy */
    for (BU_LIST_FOR(new_ps, wdb_pipe_pnt, &head)) {
	new_ps->pp_bendradius = edpipe_scale(new_ps->pp_bendradius, scale);
    }

    /* check if the copy is O.K. */
    if (rt_pipe_ck(&head)) {
	/* won't work, go back to original */
	while (BU_LIST_NON_EMPTY(&head)) {
	    new_ps = BU_LIST_FIRST(wdb_pipe_pnt, &head);
	    BU_LIST_DEQUEUE(&new_ps->l);
	    BU_PUT(new_ps, struct wdb_pipe_pnt);
	}
	return;
    }

    /* free temporary pipe solid */
    while (BU_LIST_NON_EMPTY(&head)) {
	new_ps = BU_LIST_FIRST(wdb_pipe_pnt, &head);
	BU_LIST_DEQUEUE(&new_ps->l);
	BU_PUT(new_ps, struct wdb_pipe_pnt);
    }

    /* make changes to the original */
    for (BU_LIST_FOR(old_ps, wdb_pipe_pnt, &pipeip->pipe_segs_head)) {
	old_ps->pp_bendradius = edpipe_scale(old_ps->pp_bendradius, scale);
    }
}


int
_ged_scale_pipe(struct ged *gedp, struct rt_pipe_internal *pipeip, const char *attribute, fastf_t sf, int rflag)
{
    int seg_i;
    struct wdb_pipe_pnt *ps;

    RT_PIPE_CK_MAGIC(pipeip);

    /* encode rflag as a negative scale so we don't have to pass it */
    if (!rflag && sf > 0)
	sf = -sf;

    switch (attribute[0]) {
	case 'b':
	case 'r':
	    if (sscanf(attribute+1, "%d", &seg_i) != 1)
		seg_i = 0;

	    if ((ps = rt_pipe_get_seg_i(pipeip, seg_i)) == (struct wdb_pipe_pnt *)NULL)
		return BRLCAD_ERROR;

	    pipe_seg_scale_radius(ps, sf);
	    break;
	case 'B':
	case 'R':
	    pipe_scale_radius(pipeip, sf);
	    break;
	case 'i':
	    if (sscanf(attribute+1, "%d", &seg_i) != 1)
		seg_i = 0;

	    if ((ps = rt_pipe_get_seg_i(pipeip, seg_i)) == (struct wdb_pipe_pnt *)NULL)
		return BRLCAD_ERROR;

	    pipe_seg_scale_id(ps, sf);
	    break;
	case 'I':
	    pipe_scale_id(pipeip, sf);
	    break;
	case 'o':
	    if (sscanf(attribute+1, "%d", &seg_i) != 1)
		seg_i = 0;

	    if ((ps = rt_pipe_get_seg_i(pipeip, seg_i)) == (struct wdb_pipe_pnt *)NULL)
		return BRLCAD_ERROR;

	    pipe_seg_scale_od(ps, sf);
	    break;
	case 'O':
	    pipe_scale_od(pipeip, sf);
	    break;
	default:
	    bu_vls_printf(gedp->ged_result_str, "bad pipe attribute - %s", attribute);
	    return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}


int
_ged_pipe_append_pnt_common(struct ged *gedp, int argc, const char *argv[], struct wdb_pipe_pnt *(*func)(struct rt_pipe_internal *, struct wdb_pipe_pnt *, const point_t))
{
    struct directory *dp;
    static const char *usage = "pipe pt";
    struct rt_db_internal intern;
    struct rt_pipe_internal *pipeip;
    mat_t mat;
    point_t view_ps_pt;
    point_t view_pp_coord;
    point_t ps_pt;
    struct wdb_pipe_pnt *prevpp;
    double scan[3];
    char *last;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    if ((last = strrchr(argv[1], '/')) == NULL)
	last = (char *)argv[1];
    else
	++last;

    if (last[0] == '\0') {
	bu_vls_printf(gedp->ged_result_str, "%s: illegal input - %s", argv[0], argv[1]);
	return BRLCAD_ERROR;
    }

    dp = db_lookup(gedp->dbip, last, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: failed to find %s", argv[0], argv[1]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[2], "%lf %lf %lf", &scan[X], &scan[Y], &scan[Z]) != 3) {
	bu_vls_printf(gedp->ged_result_str, "%s: bad point - %s", argv[0], argv[2]);
	return BRLCAD_ERROR;
    }
    /* convert from double to fastf_t */
    VMOVE(view_ps_pt, scan);

    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    if (wdb_import_from_path2(gedp->ged_result_str, &intern, argv[1], wdbp, mat) & BRLCAD_ERROR) {
	return BRLCAD_ERROR;
    }

    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD ||
	intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_PIPE) {
	bu_vls_printf(gedp->ged_result_str, "Object not a PIPE");
	rt_db_free_internal(&intern);

	return BRLCAD_ERROR;
    }

    pipeip = (struct rt_pipe_internal *)intern.idb_ptr;

    /* use the view z from the first or last pipe point, depending on whether we're appending or prepending */
    if (func == rt_pipe_add_pnt)
	prevpp = BU_LIST_LAST(wdb_pipe_pnt, &pipeip->pipe_segs_head);
    else
	prevpp = BU_LIST_FIRST(wdb_pipe_pnt, &pipeip->pipe_segs_head);

    MAT4X3PNT(view_pp_coord, gedp->ged_gvp->gv_model2view, prevpp->pp_coord);
    view_ps_pt[Z] = view_pp_coord[Z];
    MAT4X3PNT(ps_pt, gedp->ged_gvp->gv_view2model, view_ps_pt);

    if ((*func)(pipeip, (struct wdb_pipe_pnt *)NULL, ps_pt) == (struct wdb_pipe_pnt *)NULL) {
	rt_db_free_internal(&intern);
	bu_vls_printf(gedp->ged_result_str, "%s: cannot move point there", argv[0]);
	return BRLCAD_ERROR;
    }

    {
	mat_t invmat;
	struct wdb_pipe_pnt *curr_ps;
	point_t curr_pt;

	bn_mat_inv(invmat, mat);
	for (BU_LIST_FOR(curr_ps, wdb_pipe_pnt, &pipeip->pipe_segs_head)) {
	    MAT4X3PNT(curr_pt, invmat, curr_ps->pp_coord);
	    VMOVE(curr_ps->pp_coord, curr_pt);
	}

	GED_DB_PUT_INTERNAL(gedp, dp, &intern, &rt_uniresource, BRLCAD_ERROR);
    }

    rt_db_free_internal(&intern);
    return BRLCAD_OK;
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
