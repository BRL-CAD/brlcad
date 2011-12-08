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
/** @file mged/edpipe.c
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

#include "./mged.h"
#include "./sedit.h"
#include "./mged_dm.h"


void
split_pipept(struct bu_list *UNUSED(pipe_hd), struct wdb_pipept *UNUSED(ps), fastf_t *UNUSED(new_pt))
{
    bu_log("WARNING: pipe splitting unimplemented\n");
}


void
pipe_scale_od(struct rt_db_internal *db_int, fastf_t scale)
{
    struct wdb_pipept *ps;
    struct rt_pipe_internal *pipeip=(struct rt_pipe_internal *)db_int->idb_ptr;

    RT_PIPE_CK_MAGIC(pipeip);

    /* check that this can be done */
    for (BU_LIST_FOR(ps, wdb_pipept, &pipeip->pipe_segs_head)) {
	fastf_t tmp_od;

	if (scale < 0.0)
	    tmp_od = (-scale);
	else
	    tmp_od = ps->pp_od*scale;
	if (ps->pp_id > tmp_od) {
	    Tcl_AppendResult(INTERP, "Cannot make OD less than ID\n", (char *)NULL);
	    return;
	}
	if (tmp_od > 2.0*ps->pp_bendradius) {
	    Tcl_AppendResult(INTERP, "Cannot make outer radius greater than bend radius\n", (char *)NULL);
	    return;
	}
    }

    for (BU_LIST_FOR(ps, wdb_pipept, &pipeip->pipe_segs_head))
	ps->pp_od *= scale;
}


void
pipe_scale_id(struct rt_db_internal *db_int, fastf_t scale)
{
    struct wdb_pipept *ps;
    struct rt_pipe_internal *pipeip=(struct rt_pipe_internal *)db_int->idb_ptr;
    fastf_t tmp_id;

    RT_PIPE_CK_MAGIC(pipeip);

    /* check that this can be done */
    for (BU_LIST_FOR(ps, wdb_pipept, &pipeip->pipe_segs_head)) {
	if (scale > 0.0)
	    tmp_id = ps->pp_id*scale;
	else
	    tmp_id = (-scale);
	if (ps->pp_od < tmp_id) {
	    Tcl_AppendResult(INTERP, "Cannot make ID greater than OD\n", (char *)NULL);
	    return;
	}
	if (tmp_id > 2.0*ps->pp_bendradius) {
	    Tcl_AppendResult(INTERP, "Cannot make inner radius greater than bend radius\n", (char *)NULL);
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
	Tcl_AppendResult(INTERP, "Cannot make OD smaller than ID\n", (char *)NULL);
	return;
    }
    if (tmp_od > 2.0*ps->pp_bendradius) {
	Tcl_AppendResult(INTERP, "Cannot make outer radius greater than bend radius\n", (char *)NULL);
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
	Tcl_AppendResult(INTERP, "Cannot make ID greater than OD\n", (char *)NULL);
	return;
    }
    if (tmp_id > 2.0*ps->pp_bendradius) {
	Tcl_AppendResult(INTERP, "Cannot make inner radius greater than bend radius\n", (char *)NULL);
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
	Tcl_AppendResult(INTERP, "Cannot make bend radius less than pipe outer radius\n", (char *)NULL);
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
pipe_scale_radius(struct rt_db_internal *db_int, fastf_t scale)
{
    struct bu_list head;
    struct wdb_pipept *old_ps, *new_ps;
    struct rt_pipe_internal *pipeip=(struct rt_pipe_internal *)db_int->idb_ptr;

    RT_CK_DB_INTERNAL(db_int);
    RT_PIPE_CK_MAGIC(pipeip);

    /* make a quick check for minimum bend radius */
    for (BU_LIST_FOR(old_ps, wdb_pipept, &pipeip->pipe_segs_head)) {
	if (scale < 0.0) {
	    if ((-scale) < old_ps->pp_od * 0.5) {
		Tcl_AppendResult(INTERP, "Cannot make bend radius less than pipe outer radius\n", (char *)NULL);
		return;
	    }
	} else {
	    if (old_ps->pp_bendradius * scale < old_ps->pp_od * 0.5) {
		Tcl_AppendResult(INTERP, "Cannot make bend radius less than pipe outer radius\n", (char *)NULL);
		return;
	    }
	}
    }

    /* make temporary copy of this pipe solid */
    BU_LIST_INIT(&head);
    for (BU_LIST_FOR(old_ps, wdb_pipept, &pipeip->pipe_segs_head)) {
	BU_GET(new_ps, struct wdb_pipept);
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
find_pipept_nearest_pt(const struct bu_list *pipe_hd, const point_t pt)
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
    VSET(work, 0.0, 0.0, 1.0)
	MAT4X3VEC(dir, view_state->vs_gvp->gv_view2model, work)

	for (BU_LIST_FOR(ps, wdb_pipept, pipe_hd)) {
	    fastf_t dist;

	    dist = bn_dist_line3_pt3(pt, dir, ps->pp_coord);
	    if (dist < min_dist) {
		min_dist = dist;
		nearest = ps;
	    }
	}
    return nearest;
}


struct wdb_pipept *
add_pipept(struct rt_pipe_internal *pipeip, struct wdb_pipept *pp, const point_t new_pt)
{
    struct wdb_pipept *last;
    struct wdb_pipept *new;

    RT_PIPE_CK_MAGIC(pipeip);
    if (pp)
	BU_CKMAG(pp, WDB_PIPESEG_MAGIC, "pipe point");

    if (pp)
	last = pp;
    else {
	/* add new point to end of pipe solid */
	last = BU_LIST_LAST(wdb_pipept, &pipeip->pipe_segs_head);
	if (last->l.magic == BU_LIST_HEAD_MAGIC) {
	    BU_GET(new, struct wdb_pipept);
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
    BU_GET(new, struct wdb_pipept);
    new->l.magic = WDB_PIPESEG_MAGIC;
    new->pp_od = last->pp_od;
    new->pp_id = last->pp_id;
    new->pp_bendradius = last->pp_bendradius;
    VMOVE(new->pp_coord, new_pt);

    if (!pp)	/* add to end of pipe solid */
	BU_LIST_INSERT(&pipeip->pipe_segs_head, &new->l)
	    else		/* append after current point */
		BU_LIST_APPEND(&pp->l, &new->l)

		    if (rt_pipe_ck(&pipeip->pipe_segs_head)) {
			/* won't work here, so refuse to do it */
			BU_LIST_DEQUEUE(&new->l);
			bu_free((genptr_t)new, "add_pipept: new ");
			return pp;
		    } else
			return new;
}


void
ins_pipept(struct rt_pipe_internal *pipeip, struct wdb_pipept *pp, const point_t new_pt)
{
    struct wdb_pipept *first;
    struct wdb_pipept *new;

    RT_PIPE_CK_MAGIC(pipeip);
    if (pp)
	BU_CKMAG(pp, WDB_PIPESEG_MAGIC, "pipe point");

    if (pp)
	first = pp;
    else {
	/* insert new point at start of pipe solid */
	first = BU_LIST_FIRST(wdb_pipept, &pipeip->pipe_segs_head);
	if (first->l.magic == BU_LIST_HEAD_MAGIC) {
	    BU_GET(new, struct wdb_pipept);
	    new->l.magic = WDB_PIPESEG_MAGIC;
	    new->pp_od = 30.0;
	    new->pp_id = 0.0;
	    new->pp_bendradius = 40.0;
	    VMOVE(new->pp_coord, new_pt);
	    BU_LIST_APPEND(&pipeip->pipe_segs_head, &new->l);
	    return;
	}
    }

    /* build new point */
    BU_GET(new, struct wdb_pipept);
    new->l.magic = WDB_PIPESEG_MAGIC;
    new->pp_od = first->pp_od;
    new->pp_id = first->pp_id;
    new->pp_bendradius = first->pp_bendradius;
    VMOVE(new->pp_coord, new_pt);

    if (!pp)	/* add to start of pipe */
	BU_LIST_APPEND(&pipeip->pipe_segs_head, &new->l)
	    else		/* insert before current point */
		BU_LIST_INSERT(&pp->l, &new->l)

		    if (rt_pipe_ck(&pipeip->pipe_segs_head)) {
			/* won't work here, so refuse to do it */
			BU_LIST_DEQUEUE(&new->l);
			bu_free((genptr_t)new, "ins_pipept: new ");
		    }
}


struct wdb_pipept *
del_pipept(struct wdb_pipept *ps)
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
	Tcl_AppendResult(INTERP, "Cannot delete last point in pipe\n", (char *)NULL);
	return ps;
    }

    BU_LIST_DEQUEUE(&ps->l);

    if (rt_pipe_ck(&head->l)) {
	Tcl_AppendResult(INTERP, "Cannot delete this point, it will result in an illegal pipe\n", (char *)NULL);
	if (next)
	    BU_LIST_INSERT(&next->l, &ps->l)
		else if (prev)
		    BU_LIST_APPEND(&prev->l, &ps->l)
			else
			    BU_LIST_INSERT(&head->l, &ps->l)

				return ps;
    } else
	bu_free((genptr_t)ps, "del_pipept: ps");

    if (prev)
	return prev;
    else
	return next;

}


void
move_pipept(struct rt_pipe_internal *pipeip, struct wdb_pipept *ps, const point_t new_pt)
{
    point_t old_pt;

    RT_PIPE_CK_MAGIC(pipeip);
    BU_CKMAG(ps, WDB_PIPESEG_MAGIC, "pipe segment");

    VMOVE(old_pt, ps->pp_coord);

    VMOVE(ps->pp_coord, new_pt);
    if (rt_pipe_ck(&pipeip->pipe_segs_head)) {
	Tcl_AppendResult(INTERP, "Cannot move point there\n", (char *)NULL);
	VMOVE(ps->pp_coord, old_pt);
    }
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
