/*                        E D P I P E . C
 * BRL-CAD
 *
 * Copyright (c) 1995-2011 United States Government as represented by
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
 * split_pipept - split a pipe segment at a given point
 *
 * find_pipept_nearest_pt - find which segment of a pipe is nearest
 * the ray from "pt" in the viewing direction (for segment selection
 * in MGED)
 */

#include "common.h"

#include <stdio.h>
#include <math.h>
#include <string.h>

#include "bio.h"
#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "ged.h"
#include "nurb.h"
#include "wdb.h"


/*
 * Returns the index for the pipe segment matching ps.
 */
int
_ged_get_pipe_i_seg(struct rt_pipe_internal *pipeip, struct wdb_pipept *ps)
{
    struct wdb_pipept *curr_ps;
    int seg_i = 0;

    for (BU_LIST_FOR(curr_ps, wdb_pipept, &pipeip->pipe_segs_head)) {
	if (curr_ps == ps)
	    return seg_i;

	++seg_i;
    }

    return -1;
}


/*
 * Returns segment seg_i.
 */
struct wdb_pipept *
_ged_get_pipe_seg_i(struct rt_pipe_internal *pipeip, int seg_i)
{
    int i = 0;
    struct wdb_pipept *curr_ps;

    for (BU_LIST_FOR(curr_ps, wdb_pipept, &pipeip->pipe_segs_head)) {
	if (i == seg_i)
	    return curr_ps;

	++i;
    }

    return (struct wdb_pipept *)NULL;
}


void
split_pipept(struct bu_list *UNUSED(pipe_hd), struct wdb_pipept *UNUSED(ps), fastf_t *UNUSED(new_pt))
{
    bu_log("WARNING: pipe splitting unimplemented\n");
}


void
pipe_scale_od(struct rt_pipe_internal *pipeip, fastf_t scale)
{
    struct wdb_pipept *ps;

    RT_PIPE_CK_MAGIC(pipeip);

    /* check that this can be done */
    for (BU_LIST_FOR(ps, wdb_pipept, &pipeip->pipe_segs_head)) {
	fastf_t tmp_od;

	if (scale < 0.0)
	    tmp_od = (-scale);
	else
	    tmp_od = ps->pp_od*scale;
	if (ps->pp_id > tmp_od) {
	    /* Silently ignore */
	    return;
	}
	if (tmp_od > 2.0*ps->pp_bendradius) {
	    /* Silently ignore */
	    return;
	}
    }

    for (BU_LIST_FOR(ps, wdb_pipept, &pipeip->pipe_segs_head))
	ps->pp_od *= scale;
}


void
pipe_scale_id(struct rt_pipe_internal *pipeip, fastf_t scale)
{
    struct wdb_pipept *ps;

    RT_PIPE_CK_MAGIC(pipeip);

    /* check that this can be done */
    for (BU_LIST_FOR(ps, wdb_pipept, &pipeip->pipe_segs_head)) {
	fastf_t tmp_id;

	if (scale > 0.0)
	    tmp_id = ps->pp_id*scale;
	else
	    tmp_id = (-scale);
	if (ps->pp_od < tmp_id) {
	    /* Silently ignore */
	    return;
	}
	if (tmp_id > 2.0*ps->pp_bendradius) {
	    /* Silently ignore */
	    return;
	}
    }

    for (BU_LIST_FOR(ps, wdb_pipept, &pipeip->pipe_segs_head)) {
	if (scale > 0.0)
	    ps->pp_id *= scale;
	else
	    ps->pp_id = (-scale);
    }
}


void
pipe_seg_scale_od(struct wdb_pipept *ps, fastf_t scale)
{
    fastf_t tmp_od;

    BU_CKMAG(ps, WDB_PIPESEG_MAGIC, "pipe segment");

    /* need to check that the new OD is not less than ID
     * of any affected segment.
     */
    if (scale < 0.0)
	tmp_od = (-scale);
    else
	tmp_od = scale*ps->pp_od;
    if (ps->pp_id > tmp_od) {
#if 0
	Tcl_AppendResult(INTERP, "Cannot make OD smaller than ID\n", (char *)NULL);
#endif
	return;
    }
    if (tmp_od > 2.0*ps->pp_bendradius) {
#if 0
	Tcl_AppendResult(INTERP, "Cannot make outer radius greater than bend radius\n", (char *)NULL);
#endif
	return;
    }

    if (scale > 0.0)
	ps->pp_od *= scale;
    else
	ps->pp_od = (-scale);
}


void
pipe_seg_scale_id(struct wdb_pipept *ps, fastf_t scale)
{
    fastf_t tmp_id;

    BU_CKMAG(ps, WDB_PIPESEG_MAGIC, "pipe segment");

    /* need to check that the new ID is not greater than OD */
    if (scale > 0.0)
	tmp_id = scale*ps->pp_id;
    else
	tmp_id = (-scale);
    if (ps->pp_od < tmp_id) {
#if 0
	Tcl_AppendResult(INTERP, "Cannot make ID greater than OD\n", (char *)NULL);
#endif
	return;
    }
    if (tmp_id > 2.0*ps->pp_bendradius) {
#if 0
	Tcl_AppendResult(INTERP, "Cannot make inner radius greater than bend radius\n", (char *)NULL);
#endif
	return;
    }

    if (scale > 0.0)
	ps->pp_id *= scale;
    else
	ps->pp_id = (-scale);
}


void
pipe_seg_scale_radius(struct wdb_pipept *ps, fastf_t scale)
{
    fastf_t old_radius;
    struct wdb_pipept *head;

    BU_CKMAG(ps, WDB_PIPESEG_MAGIC, "pipe point");

    head = ps;
    while (head->l.magic != BU_LIST_HEAD_MAGIC)
	head = BU_LIST_NEXT(wdb_pipept, &head->l);

    /* make sure we can make this change */
    old_radius = ps->pp_bendradius;
    if (scale > 0.0)
	ps->pp_bendradius *= scale;
    else
	ps->pp_bendradius = (-scale);

    if (ps->pp_bendradius < ps->pp_od * 0.5) {
#if 0
	Tcl_AppendResult(INTERP, "Cannot make bend radius less than pipe outer radius\n", (char *)NULL);
#endif
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
    struct wdb_pipept *old_ps, *new_ps;

    RT_PIPE_CK_MAGIC(pipeip);

    /* make a quick check for minimum bend radius */
    for (BU_LIST_FOR(old_ps, wdb_pipept, &pipeip->pipe_segs_head)) {
	if (scale < 0.0) {
	    if ((-scale) < old_ps->pp_od * 0.5) {
#if 0
		Tcl_AppendResult(INTERP, "Cannot make bend radius less than pipe outer radius\n", (char *)NULL);
#endif
		return;
	    }
	} else {
	    if (old_ps->pp_bendradius * scale < old_ps->pp_od * 0.5) {
#if 0
		Tcl_AppendResult(INTERP, "Cannot make bend radius less than pipe outer radius\n", (char *)NULL);
#endif
		return;
	    }
	}
    }

    /* make temporary copy of this pipe solid */
    BU_LIST_INIT(&head);
    for (BU_LIST_FOR(old_ps, wdb_pipept, &pipeip->pipe_segs_head)) {
	BU_GETSTRUCT(new_ps, wdb_pipept);
	*new_ps = (*old_ps);
	BU_LIST_APPEND(&head, &new_ps->l);
    }

    /* make the desired editing changes to the copy */
    for (BU_LIST_FOR(new_ps, wdb_pipept, &head)) {
	if (scale < 0.0)
	    new_ps->pp_bendradius = (-scale);
	else
	    new_ps->pp_bendradius *= scale;
    }

    /* check if the copy is O.K. */
    if (rt_pipe_ck(&head)) {
	/* won't work, go back to original */
	while (BU_LIST_NON_EMPTY(&head)) {
	    new_ps = BU_LIST_FIRST(wdb_pipept, &head);
	    BU_LIST_DEQUEUE(&new_ps->l);
	    bu_free((genptr_t)new_ps, "pipe_scale_radius: new_ps");
	}
	return;
    }

    /* free temporary pipe solid */
    while (BU_LIST_NON_EMPTY(&head)) {
	new_ps = BU_LIST_FIRST(wdb_pipept, &head);
	BU_LIST_DEQUEUE(&new_ps->l);
	bu_free((genptr_t)new_ps, "pipe_scale_radius: new_ps");
    }

    /* make changes to the original */
    for (BU_LIST_FOR(old_ps, wdb_pipept, &pipeip->pipe_segs_head)) {
	if (scale < 0.0)
	    old_ps->pp_bendradius = (-scale);
	else
	    old_ps->pp_bendradius *= scale;
    }
}


struct wdb_pipept *
find_pipept_nearest_pt(const struct bu_list *pipe_hd, const point_t model_pt, matp_t view2model)
{
    struct wdb_pipept *ps;
    struct wdb_pipept *nearest=(struct wdb_pipept *)NULL;
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

    for (BU_LIST_FOR(ps, wdb_pipept, pipe_hd)) {
	fastf_t dist;

	dist = bn_dist_line3_pt3(model_pt, dir, ps->pp_coord);
	if (dist < min_dist) {
	    min_dist = dist;
	    nearest = ps;
	}
    }

    return nearest;
}


struct wdb_pipept *
_ged_add_pipept(struct rt_pipe_internal *pipeip, struct wdb_pipept *pp, const point_t new_pt)
{
    struct wdb_pipept *last;
    struct wdb_pipept *new;

    RT_PIPE_CK_MAGIC(pipeip);

    if (pp) {
	BU_CKMAG(pp, WDB_PIPESEG_MAGIC, "pipe point");
	last = pp;
    } else {
	/* add new point to end of pipe solid */
	last = BU_LIST_LAST(wdb_pipept, &pipeip->pipe_segs_head);

	if (last->l.magic == BU_LIST_HEAD_MAGIC) {
	    BU_GETSTRUCT(new, wdb_pipept);
	    new->l.magic = WDB_PIPESEG_MAGIC;
	    new->pp_od = 30.0;
	    new->pp_id = 0.0;
	    new->pp_bendradius = 40.0;
	    VMOVE(new->pp_coord, new_pt);
	    BU_LIST_INSERT(&pipeip->pipe_segs_head, &new->l);
	    return new;
	}
    }

    /* build new point */
    BU_GETSTRUCT(new, wdb_pipept);
    new->l.magic = WDB_PIPESEG_MAGIC;
    new->pp_od = last->pp_od;
    new->pp_id = last->pp_id;
    new->pp_bendradius = last->pp_bendradius;
    VMOVE(new->pp_coord, new_pt);

    if (pp) { /* append after current point */
	BU_LIST_APPEND(&pp->l, &new->l);
    } else { /* add to end of pipe solid */
	BU_LIST_INSERT(&pipeip->pipe_segs_head, &new->l);
    }

    if (rt_pipe_ck(&pipeip->pipe_segs_head)) {
	/* won't work here, so refuse to do it */
	BU_LIST_DEQUEUE(&new->l);
	bu_free((genptr_t)new, "add_pipept: new ");
	return pp;
    }

    return new;
}


struct wdb_pipept *
_ged_ins_pipept(struct rt_pipe_internal *pipeip, struct wdb_pipept *pp, const point_t new_pt)
{
    struct wdb_pipept *first;
    struct wdb_pipept *new;

    RT_PIPE_CK_MAGIC(pipeip);

    if (pp) {
	BU_CKMAG(pp, WDB_PIPESEG_MAGIC, "pipe point");
	first = pp;
    } else {
	/* insert new point at start of pipe solid */
	first = BU_LIST_FIRST(wdb_pipept, &pipeip->pipe_segs_head);

	if (first->l.magic == BU_LIST_HEAD_MAGIC) {
	    BU_GETSTRUCT(new, wdb_pipept);
	    new->l.magic = WDB_PIPESEG_MAGIC;
	    new->pp_od = 30.0;
	    new->pp_id = 0.0;
	    new->pp_bendradius = 40.0;
	    VMOVE(new->pp_coord, new_pt);
	    BU_LIST_APPEND(&pipeip->pipe_segs_head, &new->l);
	    return new;
	}
    }

    /* build new point */
    BU_GETSTRUCT(new, wdb_pipept);
    new->l.magic = WDB_PIPESEG_MAGIC;
    new->pp_od = first->pp_od;
    new->pp_id = first->pp_id;
    new->pp_bendradius = first->pp_bendradius;
    VMOVE(new->pp_coord, new_pt);

    if (pp) { /* insert before current point */
	BU_LIST_INSERT(&pp->l, &new->l);
    } else { /* add to start of pipe */
	BU_LIST_APPEND(&pipeip->pipe_segs_head, &new->l);
    }

    if (rt_pipe_ck(&pipeip->pipe_segs_head)) {
	/* won't work here, so refuse to do it */
	BU_LIST_DEQUEUE(&new->l);
	bu_free((genptr_t)new, "ins_pipept: new ");
	return pp;
    }

    return new;
}


struct wdb_pipept *
_ged_delete_pipept(struct wdb_pipept *ps)
{
    struct wdb_pipept *next;
    struct wdb_pipept *prev;
    struct wdb_pipept *head;

    BU_CKMAG(ps, WDB_PIPESEG_MAGIC, "pipe segment");

    head = ps;
    while (head->l.magic != BU_LIST_HEAD_MAGIC)
	head = BU_LIST_NEXT(wdb_pipept, &head->l);

    next = BU_LIST_NEXT(wdb_pipept, &ps->l);
    if (next->l.magic == BU_LIST_HEAD_MAGIC)
	next = (struct wdb_pipept *)NULL;

    prev = BU_LIST_PREV(wdb_pipept, &ps->l);
    if (prev->l.magic == BU_LIST_HEAD_MAGIC)
	prev = (struct wdb_pipept *)NULL;

    if (!prev && !next) {
#if 0
	Tcl_AppendResult(INTERP, "Cannot delete last point in pipe\n", (char *)NULL);
#endif
	return ps;
    }

    BU_LIST_DEQUEUE(&ps->l);

    if (rt_pipe_ck(&head->l)) {
#if 0
	Tcl_AppendResult(INTERP, "Cannot delete this point, it will result in an illegal pipe\n", (char *)NULL);
#endif
	if (next)
	    BU_LIST_INSERT(&next->l, &ps->l)
		else if (prev)
		    BU_LIST_APPEND(&prev->l, &ps->l)
			else
			    BU_LIST_INSERT(&head->l, &ps->l)

				return ps;
    } else
	bu_free((genptr_t)ps, "_ged_delete_pipept: ps");

    if (prev)
	return prev;
    else
	return next;

}


int
_ged_move_pipept(struct rt_pipe_internal *pipeip, struct wdb_pipept *ps, const point_t new_pt)
{
    point_t old_pt;

    RT_PIPE_CK_MAGIC(pipeip);
    BU_CKMAG(ps, WDB_PIPESEG_MAGIC, "pipe segment");

    VMOVE(old_pt, ps->pp_coord);

    VMOVE(ps->pp_coord, new_pt);
    if (rt_pipe_ck(&pipeip->pipe_segs_head)) {
#if 0
	Tcl_AppendResult(INTERP, "Cannot move point there\n", (char *)NULL);
#endif
	VMOVE(ps->pp_coord, old_pt);
	return 1;
    }

    return 0;
}


int
_ged_scale_pipe(struct ged *gedp, struct rt_pipe_internal *pipeip, const char *attribute, fastf_t sf, int rflag)
{
    int seg_i;
    struct wdb_pipept *ps;

    RT_PIPE_CK_MAGIC(pipeip);

    if (!rflag && sf > 0)
	sf *= -1.0;

    switch (attribute[0]) {
	case 'b':
	case 'r':
	    if (sscanf(attribute+1, "%d", &seg_i) != 1)
		seg_i = 0;

	    if ((ps = _ged_get_pipe_seg_i(pipeip, seg_i)) == (struct wdb_pipept *)NULL)
		return GED_ERROR;

	    pipe_seg_scale_radius(ps, sf);
	    break;
	case 'B':
	case 'R':
	    pipe_scale_radius(pipeip, sf);
	    break;
	case 'i':
	    if (sscanf(attribute+1, "%d", &seg_i) != 1)
		seg_i = 0;

	    if ((ps = _ged_get_pipe_seg_i(pipeip, seg_i)) == (struct wdb_pipept *)NULL)
		return GED_ERROR;

	    pipe_seg_scale_id(ps, sf);
	    break;
	case 'I':
	    pipe_scale_id(pipeip, sf);
	    break;
	case 'o':
	    if (sscanf(attribute+1, "%d", &seg_i) != 1)
		seg_i = 0;

	    if ((ps = _ged_get_pipe_seg_i(pipeip, seg_i)) == (struct wdb_pipept *)NULL)
		return GED_ERROR;

	    pipe_seg_scale_od(ps, sf);
	    break;
	case 'O':
	    pipe_scale_od(pipeip, sf);
	    break;
	default:
	    bu_vls_printf(gedp->ged_result_str, "bad pipe attribute - %s", attribute);
	    return GED_ERROR;
    }

    return GED_OK;
}


int
ged_append_pipept(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    static const char *usage = "pipe pt";
    struct rt_db_internal intern;
    struct rt_pipe_internal *pipeip;
    mat_t mat;
    point_t ps_pt;
    char *last;

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
	last = (char *)argv[1];
    else
	++last;

    if (last[0] == '\0') {
	bu_vls_printf(gedp->ged_result_str, "%s: illegal input - %s", argv[0], argv[1]);
	return GED_ERROR;
    }

    if ((dp = db_lookup(gedp->ged_wdbp->dbip, last, LOOKUP_QUIET)) == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: failed to find %s", argv[0], argv[1]);
	return GED_ERROR;
    }

    if (sscanf(argv[2], "%lf %lf %lf", &ps_pt[X], &ps_pt[Y], &ps_pt[Z]) != 3) {
	bu_vls_printf(gedp->ged_result_str, "%s: bad point - %s", argv[0], argv[2]);
	return GED_ERROR;
    }

    if (wdb_import_from_path2(gedp->ged_result_str, &intern, argv[1], gedp->ged_wdbp, mat) == GED_ERROR)
	return GED_ERROR;

    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD ||
	intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_PIPE) {
	bu_vls_printf(gedp->ged_result_str, "Object not a PIPE");
	rt_db_free_internal(&intern);

	return GED_ERROR;
    }

    pipeip = (struct rt_pipe_internal *)intern.idb_ptr;
    if (_ged_add_pipept(pipeip, (struct wdb_pipept *)NULL, ps_pt) == (struct wdb_pipept *)NULL) {
	rt_db_free_internal(&intern);
	bu_vls_printf(gedp->ged_result_str, "%s: cannot move point there", argv[0]);
	return GED_ERROR;
    }

    {
	mat_t invmat;
	struct wdb_pipept *curr_ps;
	point_t curr_pt;

	bn_mat_inv(invmat, mat);
	for (BU_LIST_FOR(curr_ps, wdb_pipept, &pipeip->pipe_segs_head)) {
	    MAT4X3PNT(curr_pt, invmat, curr_ps->pp_coord);
	    VMOVE(curr_ps->pp_coord, curr_pt);
	}

	GED_DB_PUT_INTERNAL(gedp, dp, &intern, &rt_uniresource, GED_ERROR);
    }

    rt_db_free_internal(&intern);
    return GED_OK;
}


int
ged_delete_pipept(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    static const char *usage = "pipe seg_i";
    struct rt_db_internal intern;
    struct wdb_pipept *ps;
    struct rt_pipe_internal *pipeip;
    int seg_i;
    char *last;

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
	last = (char *)argv[1];
    else
	++last;

    if (last[0] == '\0') {
	bu_vls_printf(gedp->ged_result_str, "%s: illegal input - %s", argv[0], argv[1]);
	return GED_ERROR;
    }

    if ((dp = db_lookup(gedp->ged_wdbp->dbip, last, LOOKUP_QUIET)) == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: failed to find %s", argv[0], argv[1]);
	return GED_ERROR;
    }

    if (sscanf(argv[2], "%d", &seg_i) != 1) {
	bu_vls_printf(gedp->ged_result_str, "%s: bad pipe segment index - %s", argv[0], argv[3]);
	return GED_ERROR;
    }

    if (rt_db_get_internal(&intern, dp, gedp->ged_wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
	bu_vls_printf(gedp->ged_result_str, "%s: failed to get internal for %s", argv[0], argv[1]);
	return GED_ERROR;
    }

    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD ||
	intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_PIPE) {
	bu_vls_printf(gedp->ged_result_str, "%s is not a PIPE", argv[1]);
	rt_db_free_internal(&intern);

	return GED_ERROR;
    }

    pipeip = (struct rt_pipe_internal *)intern.idb_ptr;
    if ((ps = _ged_get_pipe_seg_i(pipeip, seg_i)) == (struct wdb_pipept *)NULL) {
	rt_db_free_internal(&intern);
	bu_vls_printf(gedp->ged_result_str, "%s: bad pipe segment index - %s", argv[0], argv[2]);
	return GED_ERROR;
    }

    if (_ged_delete_pipept(ps) == ps) {
	rt_db_free_internal(&intern);
	bu_vls_printf(gedp->ged_result_str, "%s: cannot delete pipe segment %d", argv[0], seg_i);
	return GED_ERROR;
    }

    GED_DB_PUT_INTERNAL(gedp, dp, &intern, &rt_uniresource, GED_ERROR);

    rt_db_free_internal(&intern);
    return GED_OK;
}


int
ged_find_pipept_nearest_pt(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    static const char *usage = "pipe x y z";
    struct rt_db_internal intern;
    struct wdb_pipept *nearest;
    point_t model_pt;
    mat_t mat;
    int seg_i;
    char *last;

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
	last = (char *)argv[1];
    else
	++last;

    if (last[0] == '\0') {
	bu_vls_printf(gedp->ged_result_str, "%s: illegal input - %s", argv[0], argv[1]);
	return GED_ERROR;
    }

    if ((dp = db_lookup(gedp->ged_wdbp->dbip, last, LOOKUP_QUIET)) == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: failed to find %s", argv[0], argv[1]);
	return GED_ERROR;
    }

    if (argc == 3) {
	if (sscanf(argv[2], "%lf %lf %lf", &model_pt[X], &model_pt[Y], &model_pt[Z]) != 3) {
	    bu_vls_printf(gedp->ged_result_str, "%s: bad point - %s", argv[0], argv[2]);
	    return GED_ERROR;
	}
    } else if (sscanf(argv[2], "%lf", &model_pt[X]) != 1 ||
	       sscanf(argv[3], "%lf", &model_pt[Y]) != 1 ||
	       sscanf(argv[4], "%lf", &model_pt[Z]) != 1) {
	bu_vls_printf(gedp->ged_result_str, "%s: bad X, Y or Z", argv[0]);
	return GED_ERROR;
    }

    if (wdb_import_from_path2(gedp->ged_result_str, &intern, argv[1], gedp->ged_wdbp, mat) == GED_ERROR)
	return GED_ERROR;

    nearest = find_pipept_nearest_pt(&((struct rt_pipe_internal *)intern.idb_ptr)->pipe_segs_head,
				     model_pt, gedp->ged_gvp->gv_view2model);
    seg_i = _ged_get_pipe_i_seg((struct rt_pipe_internal *)intern.idb_ptr, nearest);
    rt_db_free_internal(&intern);

    if (seg_i < 0) {
	bu_vls_printf(gedp->ged_result_str, "%s: failed to find segment for %s", argv[0], argv[1]);
	return GED_ERROR;
    }

    bu_vls_printf(gedp->ged_result_str, "%d", seg_i);
    return GED_OK;
}


int
ged_move_pipept(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    static const char *usage = "[-r] pipe seg_i pt";
    struct rt_db_internal intern;
    struct wdb_pipept *ps;
    struct rt_pipe_internal *pipeip;
    mat_t mat;
    point_t ps_pt;
    int seg_i;
    int rflag = 0;
    char *last;

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
	last = (char *)argv[1];
    else
	++last;

    if (last[0] == '\0') {
	bu_vls_printf(gedp->ged_result_str, "%s: illegal input - %s", argv[0], argv[1]);
	return GED_ERROR;
    }

    if ((dp = db_lookup(gedp->ged_wdbp->dbip, last, LOOKUP_QUIET)) == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: failed to find %s", argv[0], argv[1]);
	return GED_ERROR;
    }

    if (sscanf(argv[2], "%d", &seg_i) != 1) {
	bu_vls_printf(gedp->ged_result_str, "%s: bad pipe segment index - %s", argv[0], argv[3]);
	return GED_ERROR;
    }

    /*XXXX At the moment, ps_pt is expected to be in base units. Might need to change to local units for the "p" command. */
    if (sscanf(argv[3], "%lf %lf %lf", &ps_pt[X], &ps_pt[Y], &ps_pt[Z]) != 3) {
	bu_vls_printf(gedp->ged_result_str, "%s: bad point - %s", argv[0], argv[3]);
	return GED_ERROR;
    }

    if (wdb_import_from_path2(gedp->ged_result_str, &intern, argv[1], gedp->ged_wdbp, mat) == GED_ERROR)
	return GED_ERROR;

    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD ||
	intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_PIPE) {
	bu_vls_printf(gedp->ged_result_str, "Object not a PIPE");
	rt_db_free_internal(&intern);

	return GED_ERROR;
    }

    pipeip = (struct rt_pipe_internal *)intern.idb_ptr;
    if ((ps = _ged_get_pipe_seg_i(pipeip, seg_i)) == (struct wdb_pipept *)NULL) {
	rt_db_free_internal(&intern);
	bu_vls_printf(gedp->ged_result_str, "%s: bad pipe segment index - %s", argv[0], argv[2]);
	return GED_ERROR;
    }

    if (rflag) {
	VADD2(ps_pt, ps_pt, ps->pp_coord);
    }

    if (_ged_move_pipept(pipeip, ps, ps_pt)) {
	rt_db_free_internal(&intern);
	bu_vls_printf(gedp->ged_result_str, "%s: cannot move point there", argv[0]);
	return GED_ERROR;
    }

    {
	mat_t invmat;
	struct wdb_pipept *curr_ps;
	point_t curr_pt;

	bn_mat_inv(invmat, mat);
	for (BU_LIST_FOR(curr_ps, wdb_pipept, &pipeip->pipe_segs_head)) {
	    MAT4X3PNT(curr_pt, invmat, curr_ps->pp_coord);
	    VMOVE(curr_ps->pp_coord, curr_pt);
	}

	GED_DB_PUT_INTERNAL(gedp, dp, &intern, &rt_uniresource, GED_ERROR);
    }

    rt_db_free_internal(&intern);
    return GED_OK;
}


int
ged_prepend_pipept(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    static const char *usage = "pipe pt";
    struct rt_db_internal intern;
    struct rt_pipe_internal *pipeip;
    mat_t mat;
    point_t ps_pt;
    char *last;

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
	last = (char *)argv[1];
    else
	++last;

    if (last[0] == '\0') {
	bu_vls_printf(gedp->ged_result_str, "%s: illegal input - %s", argv[0], argv[1]);
	return GED_ERROR;
    }

    if ((dp = db_lookup(gedp->ged_wdbp->dbip, last, LOOKUP_QUIET)) == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: failed to find %s", argv[0], argv[1]);
	return GED_ERROR;
    }

    if (sscanf(argv[2], "%lf %lf %lf", &ps_pt[X], &ps_pt[Y], &ps_pt[Z]) != 3) {
	bu_vls_printf(gedp->ged_result_str, "%s: bad point - %s", argv[0], argv[2]);
	return GED_ERROR;
    }

    if (wdb_import_from_path2(gedp->ged_result_str, &intern, argv[1], gedp->ged_wdbp, mat) == GED_ERROR)
	return GED_ERROR;

    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD ||
	intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_PIPE) {
	bu_vls_printf(gedp->ged_result_str, "Object not a PIPE");
	rt_db_free_internal(&intern);

	return GED_ERROR;
    }

    pipeip = (struct rt_pipe_internal *)intern.idb_ptr;
    if (_ged_ins_pipept(pipeip, (struct wdb_pipept *)NULL, ps_pt) == (struct wdb_pipept *)NULL) {
	rt_db_free_internal(&intern);
	bu_vls_printf(gedp->ged_result_str, "%s: cannot move point there", argv[0]);
	return GED_ERROR;
    }

    {
	mat_t invmat;
	struct wdb_pipept *curr_ps;
	point_t curr_pt;

	bn_mat_inv(invmat, mat);
	for (BU_LIST_FOR(curr_ps, wdb_pipept, &pipeip->pipe_segs_head)) {
	    MAT4X3PNT(curr_pt, invmat, curr_ps->pp_coord);
	    VMOVE(curr_ps->pp_coord, curr_pt);
	}

	GED_DB_PUT_INTERNAL(gedp, dp, &intern, &rt_uniresource, GED_ERROR);
    }

    rt_db_free_internal(&intern);
    return GED_OK;
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
