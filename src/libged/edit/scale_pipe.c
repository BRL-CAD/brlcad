/*                   S C A L E _ P I P E . C
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
/** @file libged/edit/scale_pipe.c
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

#include "../ged_private.h"
#include "./ged_edit.h"

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


static void
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


static void
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


static void
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


static void
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


static void
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


static void
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


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
