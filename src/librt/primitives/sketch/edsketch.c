/*                      E D S K E T C H . C
 * BRL-CAD
 *
 * Copyright (c) 2025-2026 United States Government as represented by
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
/** @file primitives/sketch/edsketch.c
 *
 * Editing support for the 2-D sketch primitive (ID_SKETCH).
 *
 * Vertex/segment indices are carried in e_para[0] (and e_para[1] for
 * the second index of a line segment).  UV coordinates and deltas are
 * carried in e_para[0] and e_para[1].  See the ECMD descriptions below.
 */

#include "common.h"

#include <math.h>
#include <string.h>

#include "vmath.h"
#include "bu/malloc.h"
#include "nmg.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "rt/edit.h"
#include "rt/primitives/sketch.h"
#include "wdb.h"

#include "../edit_private.h"

/*
 * ECMD numbers for sketch editing.  ID_SKETCH = 26, so we use 26nnn.
 * These defines are the authoritative source; rt_ecmd_scanner.cpp reads
 * this file at build time to generate include/rt/rt_ecmds.h.
 */

/** Select a vertex by index (e_para[0] = vertex index) or by mouse
 *  proximity when invoked after ft_edit_xy sets v_pos. */
#define ECMD_SKETCH_PICK_VERTEX    26001
/** Move the currently selected vertex (e_para[0..1] = new UV coords in mm). */
#define ECMD_SKETCH_MOVE_VERTEX    26002
/** Select a curve segment by index (e_para[0] = segment index). */
#define ECMD_SKETCH_PICK_SEGMENT   26003
/** Translate the currently selected segment (e_para[0..1] = UV delta in mm). */
#define ECMD_SKETCH_MOVE_SEGMENT   26004
/** Append a line segment (e_para[0] = start vert index, e_para[1] = end vert index). */
#define ECMD_SKETCH_APPEND_LINE    26005
/**
 * Append a circular arc segment.
 * e_para[0] = start vert index
 * e_para[1] = end   vert index
 * e_para[2] = radius (mm; negative → full circle, "end" is centre)
 * e_para[3] = center_is_left flag (non-zero = center to left of start→end)
 * e_para[4] = orientation flag (0 = ccw, non-zero = cw)
 * e_inpara must be 5.  Uses RT_EDIT_MAXPARA parameter slots.
 */
#define ECMD_SKETCH_APPEND_ARC     26006
/**
 * Append a Bezier curve segment.
 * e_para[0..e_inpara-1] are vertex indices of the (e_inpara) control points.
 * degree = e_inpara - 1.  Requires e_inpara >= 2.
 * Up to RT_EDIT_MAXPARA control points are supported (degree 0..RT_EDIT_MAXPARA-1).
 */
#define ECMD_SKETCH_APPEND_BEZIER  26007
/** Delete the currently selected vertex (only if not used by any segment). */
#define ECMD_SKETCH_DELETE_VERTEX  26008
/** Delete the currently selected curve segment. */
#define ECMD_SKETCH_DELETE_SEGMENT 26009
/**
 * Move a list of vertices by a common UV delta.
 *
 * e_para[0]            = U delta (mm)
 * e_para[1]            = V delta (mm)
 * e_para[2..e_inpara-1] = vertex indices (0-based)
 * e_inpara             >= 3 (at least one vertex index)
 *
 * Each listed vertex is shifted by (e_para[0], e_para[1]).
 * Out-of-range indices are silently skipped.
 */
#define ECMD_SKETCH_MOVE_VERTEX_LIST 26010
/**
 * Split the currently selected segment at parameter t.
 *
 * e_para[0] = segment index (0-based)
 * e_para[1] = split parameter t in the open interval (0, 1)
 * e_inpara  = 2
 *
 * Supported segment types: LINE, CARC (non-full-circle), BEZIER.
 * Full-circle CARCs and NURB segments are not supported.
 *
 * On success the segment at e_para[0] is replaced by two segments:
 *   [si]   — the portion from the original start to the split point
 *   [si+1] — the portion from the split point to the original end
 * A new vertex is added for the split point (and de Casteljau
 * intermediate points for BEZIER).  curr_seg is reset to -1.
 */
#define ECMD_SKETCH_SPLIT_SEGMENT  26011
/**
 * Append a non-rational NURB curve segment with an automatically
 * generated clamped-uniform knot vector.
 *
 * e_para[0]              = order (integer >= 2; order = degree + 1, so
 *                          order=3 → quadratic, order=4 → cubic, …)
 * e_para[1..e_inpara-1]  = control point vertex indices (c_size = e_inpara-1)
 * e_inpara               >= 1 + order  (c_size must be >= order)
 *
 * pt_type is always set to RT_NURB_MAKE_PT_TYPE(2, RT_NURB_PT_XY,
 * RT_NURB_PT_NONRAT) — a non-rational 2-D point type appropriate for
 * sketch curves.  Use ECMD_SKETCH_NURB_EDIT_WEIGHTS to add weights
 * (making the segment rational) afterwards.
 *
 * Knot vector: clamped uniform over [0, c_size - order + 1]:
 *   knots[0..order-1]                = 0          (order repeated zeros)
 *   knots[order..c_size-1]           = 1, 2, ...  (interior, uniformly spaced)
 *   knots[c_size..c_size+order-1]    = c_size - order + 1 (repeated end value)
 */
#define ECMD_SKETCH_APPEND_NURB    26012
/**
 * Replace the knot vector of the currently selected NURB segment.
 *
 * e_para[0]              = k_size (number of knot values; must equal
 *                          nsg->order + nsg->c_size for the selected segment)
 * e_para[1..e_inpara-1]  = new knot values (ascending non-decreasing order)
 * e_inpara               = 1 + k_size
 *
 * curr_seg must be set to the index of a NURB segment before calling.
 * The operation rejects knot vectors where k_size != order + c_size.
 */
#define ECMD_SKETCH_NURB_EDIT_KV   26013
/**
 * Set or replace the weight array of the currently selected NURB segment,
 * making it rational (or updating weights if already rational).
 *
 * e_para[0]              = segment index (must reference a NURB segment)
 * e_para[1]              = c_size expected (must match nsg->c_size)
 * e_para[2..e_inpara-1]  = per-control-point weight values (> 0)
 * e_inpara               = 2 + c_size
 *
 * If the segment currently has no weights array it is allocated.  If it
 * already has one the values are replaced.  Setting all weights to 1 is
 * valid (produces a non-rational B-spline represented as rational).
 * pt_type is updated to the rational variant on first call.
 */
#define ECMD_SKETCH_NURB_EDIT_WEIGHTS 26014
/**
 * Add a new vertex at the given UV position (local units) and select it.
 * e_para[0] = U coordinate; e_para[1] = V coordinate.
 * e_inpara must be 2.  On success se->curr_vert is set to the new vertex index.
 */
#define ECMD_SKETCH_ADD_VERTEX        26015
/**
 * Toggle the center_is_left flag of the currently selected arc segment,
 * switching between the arc and its complement (the other arc sharing the
 * same start vertex, end vertex, and radius).
 *
 * No parameters needed (curr_seg must reference a non-full-circle CARC).
 * e_inpara = 0.
 * Returns BRLCAD_ERROR if no CARC is selected or if it is a full circle
 * (radius < 0), since full circles have no complement.
 */
#define ECMD_SKETCH_TOGGLE_ARC_ORIENT 26016
/**
 * Set the radius of the currently selected arc or full-circle segment.
 *
 * e_para[0] = new radius in local units.
 *             Positive  → circular arc (partial).
 *             Negative  → full circle (end vertex is the centre).
 * e_inpara  = 1.
 * curr_seg must point to a CARC segment.
 */
#define ECMD_SKETCH_SET_ARC_RADIUS    26017
/**
 * Make the currently selected CARC arc tangent to an adjacent segment at
 * their shared vertex.
 *
 * The operation implements the MGED skt_ed.tcl "Set Tangency" workflow:
 *   1. Both segments must share exactly one vertex.
 *   2. The tangent direction of the *adjacent* segment at the shared vertex
 *      is computed (chord direction for a line; radius-perpendicular for arcs).
 *   3. Optionally, the tangent is rotated by a user-supplied angle so the
 *      arc can be tangent at an oblique angle to the adjacent segment.
 *   4. The radius and center_is_left / orientation flags of curr_seg are
 *      updated so the arc starts or ends tangentially.
 *
 * e_para layout:
 *   e_para[0]  = index of the *adjacent* segment (the one to be tangent to)
 *   e_para[1]  = tangency angle in radians (0 = direct tangency, i.e. smooth join)
 * e_inpara    = 2
 * curr_seg    must be the CARC segment to be modified (se->curr_seg >= 0).
 *
 * Returns BRLCAD_OK on success, BRLCAD_ERROR with a message if the geometry
 * is degenerate (no shared vertex, zero-length chord, etc.).
 */
#define ECMD_SKETCH_SET_TANGENCY      26018
/**
 * Edit the sketch plane parameters: origin (V), u-direction vector (A),
 * and v-direction vector (B).
 *
 * e_para layout (all values in model base units / radians as appropriate):
 *   e_para[0..2]  = new V (3-D origin in model space)
 *   e_para[3..5]  = new A (u_vec, will be normalised)
 *   e_para[6..8]  = new B (v_vec, will be normalised)
 * e_inpara       = 9.
 *
 * The function normalises A and B and ensures they are mutually
 * perpendicular (B is re-orthogonalised against A via Gram-Schmidt).
 * Returns BRLCAD_ERROR if A has zero length.
 */
#define ECMD_SKETCH_SET_PLANE         26019
/**
 * Toggle the reverse flag for the currently selected segment.
 *
 * The sketch curve stores a `reverse` array (one byte per segment) that
 * reverses the traversal direction of each segment.  This is used when
 * assembling closed contours so that segments can be shared between curves
 * drawn in opposite directions.
 *
 * e_inpara = 0; curr_seg must be >= 0.
 * Returns BRLCAD_OK on success, BRLCAD_ERROR if no segment is selected.
 */
#define ECMD_SKETCH_TOGGLE_SEGMENT_REVERSE 26020


/* ------------------------------------------------------------------ */
/* ipe_ptr lifecycle                                                   */
/* ------------------------------------------------------------------ */

void *
rt_edit_sketch_prim_edit_create(struct rt_edit *UNUSED(s))
{
    struct rt_sketch_edit *se;
    BU_GET(se, struct rt_sketch_edit);
    se->curr_vert   = -1;
    se->curr_seg    = -1;
    VSETALL(se->v_pos, 0.0);
    se->v_pos_valid = 0;
    return (void *)se;
}

void
rt_edit_sketch_prim_edit_destroy(struct rt_sketch_edit *se)
{
    if (!se)
	return;
    BU_PUT(se, struct rt_sketch_edit);
}

void
rt_edit_sketch_prim_edit_reset(struct rt_edit *s)
{
    struct rt_sketch_edit *se = (struct rt_sketch_edit *)s->ipe_ptr;
    if (!se)
	return;
    se->curr_vert   = -1;
    se->curr_seg    = -1;
    se->v_pos_valid = 0;
}


/* ------------------------------------------------------------------ */
/* set_edit_mode                                                       */
/* ------------------------------------------------------------------ */

void
rt_edit_sketch_set_edit_mode(struct rt_edit *s, int mode)
{
    rt_edit_set_edflag(s, mode);

    switch (mode) {
	case ECMD_SKETCH_MOVE_VERTEX:
	case ECMD_SKETCH_MOVE_SEGMENT:
	case ECMD_SKETCH_MOVE_VERTEX_LIST:
	    s->edit_mode = RT_PARAMS_EDIT_TRANS;
	    break;
	case ECMD_SKETCH_PICK_VERTEX:
	case ECMD_SKETCH_PICK_SEGMENT:
	    s->edit_mode = RT_PARAMS_EDIT_PICK;
	    break;
	default:
	    break;
    }

    rt_edit_process(s);
}

static void
sketch_ed(struct rt_edit *s, int arg, int UNUSED(a), int UNUSED(b), void *UNUSED(data))
{
    rt_edit_sketch_set_edit_mode(s, arg);
}


/* ------------------------------------------------------------------ */
/* Menu                                                                */
/* ------------------------------------------------------------------ */

struct rt_edit_menu_item sketch_menu[] = {
    { "SKETCH MENU",         NULL,      0 },
    { "Pick Vertex",         sketch_ed, ECMD_SKETCH_PICK_VERTEX },
    { "Move Vertex",         sketch_ed, ECMD_SKETCH_MOVE_VERTEX },
    { "Move Vertex List",    sketch_ed, ECMD_SKETCH_MOVE_VERTEX_LIST },
    { "Pick Segment",        sketch_ed, ECMD_SKETCH_PICK_SEGMENT },
    { "Move Segment",        sketch_ed, ECMD_SKETCH_MOVE_SEGMENT },
    { "Append Line",         sketch_ed, ECMD_SKETCH_APPEND_LINE },
    { "Append Arc",          sketch_ed, ECMD_SKETCH_APPEND_ARC },
    { "Append Bezier",       sketch_ed, ECMD_SKETCH_APPEND_BEZIER },
    { "Delete Vertex",       sketch_ed, ECMD_SKETCH_DELETE_VERTEX },
    { "Delete Segment",      sketch_ed, ECMD_SKETCH_DELETE_SEGMENT },
    { "Split Segment",       sketch_ed, ECMD_SKETCH_SPLIT_SEGMENT },
    { "Append NURB",         sketch_ed, ECMD_SKETCH_APPEND_NURB },
    { "NURB Edit KV",        sketch_ed, ECMD_SKETCH_NURB_EDIT_KV },
    { "NURB Edit Weights",   sketch_ed, ECMD_SKETCH_NURB_EDIT_WEIGHTS },
    { "Add Vertex",          sketch_ed, ECMD_SKETCH_ADD_VERTEX },
    { "Toggle Arc Orient",   sketch_ed, ECMD_SKETCH_TOGGLE_ARC_ORIENT },
    { "Set Arc Radius",      sketch_ed, ECMD_SKETCH_SET_ARC_RADIUS },
    { "Set Arc Tangency",    sketch_ed, ECMD_SKETCH_SET_TANGENCY },
    { "Set Sketch Plane",    sketch_ed, ECMD_SKETCH_SET_PLANE },
    { "Toggle Seg Reverse",  sketch_ed, ECMD_SKETCH_TOGGLE_SEGMENT_REVERSE },
    { "", NULL, 0 }
};

struct rt_edit_menu_item *
rt_edit_sketch_menu_item(const struct bn_tol *UNUSED(tol))
{
    return sketch_menu;
}

static const struct rt_edit_param_desc sketch_idx_param[] = {
    { "index", "Index", RT_EDIT_PARAM_INTEGER, 0,
      0.0, RT_EDIT_PARAM_NO_LIMIT, "count", 0, NULL, NULL, NULL }
};

static const struct rt_edit_param_desc sketch_point_param[] = {
    { "point", "Point", RT_EDIT_PARAM_POINT, 0,
      RT_EDIT_PARAM_NO_LIMIT, RT_EDIT_PARAM_NO_LIMIT, "length", 0, NULL, NULL, NULL }
};

static const struct rt_edit_cmd_desc sketch_cmds[] = {
    { ECMD_SKETCH_PICK_VERTEX,           "Pick Vertex",          "selection", 1, sketch_idx_param,   1, 10 },
    { ECMD_SKETCH_MOVE_VERTEX,           "Move Vertex",          "movement",  1, sketch_point_param, 1, 20 },
    { ECMD_SKETCH_PICK_SEGMENT,          "Pick Segment",         "selection", 1, sketch_idx_param,   1, 30 },
    { ECMD_SKETCH_MOVE_SEGMENT,          "Move Segment",         "movement",  1, sketch_point_param, 1, 40 },
    { ECMD_SKETCH_APPEND_LINE,           "Append Line",          "topology",  0, NULL,               1, 50 },
    { ECMD_SKETCH_APPEND_ARC,            "Append Arc",           "topology",  0, NULL,               1, 60 },
    { ECMD_SKETCH_APPEND_BEZIER,         "Append Bezier",        "topology",  0, NULL,               1, 70 },
    { ECMD_SKETCH_DELETE_VERTEX,         "Delete Vertex",        "topology",  1, sketch_idx_param,   1, 80 },
    { ECMD_SKETCH_DELETE_SEGMENT,        "Delete Segment",       "topology",  1, sketch_idx_param,   1, 90 },
    { ECMD_SKETCH_MOVE_VERTEX_LIST,      "Move Vertex List",     "movement",  1, sketch_point_param, 1, 100 },
    { ECMD_SKETCH_SPLIT_SEGMENT,         "Split Segment",        "topology",  1, sketch_idx_param,   1, 110 },
    { ECMD_SKETCH_APPEND_NURB,           "Append NURB",          "topology",  0, NULL,               1, 120 },
    { ECMD_SKETCH_NURB_EDIT_KV,          "NURB Edit KV",         "topology",  0, NULL,               1, 130 },
    { ECMD_SKETCH_NURB_EDIT_WEIGHTS,     "NURB Edit Weights",    "topology",  0, NULL,               1, 140 },
    { ECMD_SKETCH_ADD_VERTEX,            "Add Vertex",           "topology",  1, sketch_point_param, 1, 150 },
    { ECMD_SKETCH_TOGGLE_ARC_ORIENT,     "Toggle Arc Orient",    "topology",  1, sketch_idx_param,   1, 160 },
    { ECMD_SKETCH_SET_ARC_RADIUS,        "Set Arc Radius",       "topology",  1, sketch_idx_param,   1, 170 },
    { ECMD_SKETCH_SET_TANGENCY,          "Set Arc Tangency",     "topology",  0, NULL,               1, 180 },
    { ECMD_SKETCH_SET_PLANE,             "Set Sketch Plane",     "geometry",  0, NULL,               1, 190 },
    { ECMD_SKETCH_TOGGLE_SEGMENT_REVERSE,"Toggle Seg Reverse",   "topology",  1, sketch_idx_param,   1, 200 }
};

static const struct rt_edit_prim_desc sketch_prim_desc = {
    "sketch", "Sketch", 20, sketch_cmds
};

const struct rt_edit_prim_desc *
rt_edit_sketch_edit_desc(void)
{
    return &sketch_prim_desc;
}


/* ------------------------------------------------------------------ */
/* Helper: check if a vertex is referenced by any segment             */
/* ------------------------------------------------------------------ */

static int
sketch_vert_is_used(const struct rt_sketch_internal *skt, int vi)
{
    size_t i;
    for (i = 0; i < skt->curve.count; i++) {
	void *seg = skt->curve.segment[i];
	if (!seg)
	    continue;
	uint32_t magic = *(uint32_t *)seg;
	if (magic == CURVE_LSEG_MAGIC) {
	    struct line_seg *ls = (struct line_seg *)seg;
	    if (ls->start == vi || ls->end == vi)
		return 1;
	} else if (magic == CURVE_CARC_MAGIC) {
	    struct carc_seg *cs = (struct carc_seg *)seg;
	    if (cs->start == vi || cs->end == vi)
		return 1;
	} else if (magic == CURVE_BEZIER_MAGIC) {
	    struct bezier_seg *bs = (struct bezier_seg *)seg;
	    int j;
	    for (j = 0; j <= bs->degree; j++) {
		if (bs->ctl_points[j] == vi)
		    return 1;
	    }
	} else if (magic == CURVE_NURB_MAGIC) {
	    struct nurb_seg *ns = (struct nurb_seg *)seg;
	    int j;
	    for (j = 0; j < ns->c_size; j++) {
		if (ns->ctl_points[j] == vi)
		    return 1;
	    }
	}
    }
    return 0;
}


/* ------------------------------------------------------------------ */
/* Edit operation implementations                                      */
/* ------------------------------------------------------------------ */

/*
 * Find the sketch vertex whose 3-D position projects closest to the
 * view-space reference point ref_view.  Each 2-D vertex (u, v) maps to
 * 3-D world-space as:
 *   P = skt->V  +  u * skt->u_vec  +  v * skt->v_vec
 * The 3-D point is then projected to view space via model2objview and
 * the XY distance to ref_view is minimised.
 * Returns the index of the closest vertex, or -1 if no vertices exist.
 */
static int
sketch_closest_vertex(const struct rt_sketch_internal *skt,
		      const mat_t model2objview,
		      const point_t ref_view)
{
    int c_vi = -1;
    fastf_t c_dist = INFINITY;

    for (size_t i = 0; i < skt->vert_count; i++) {
	point_t p3d;
	VJOIN2(p3d, skt->V,
	       skt->verts[i][0], skt->u_vec,
	       skt->verts[i][1], skt->v_vec);
	point_t p_view;
	MAT4X3PNT(p_view, model2objview, p3d);
	fastf_t dx = p_view[X] - ref_view[X];
	fastf_t dy = p_view[Y] - ref_view[Y];
	fastf_t d2 = dx*dx + dy*dy;
	if (d2 < c_dist) {
	    c_dist = d2;
	    c_vi = (int)i;
	}
    }
    return c_vi;
}

static int
ecmd_sketch_pick_vertex(struct rt_edit *s)
{
    struct rt_sketch_edit *se = (struct rt_sketch_edit *)s->ipe_ptr;
    struct rt_sketch_internal *skt =
	(struct rt_sketch_internal *)s->es_int.idb_ptr;
    RT_SKETCH_CK_MAGIC(skt);

    /* Mouse-proximity path: view cursor was stored by ft_edit_xy */
    if (!s->e_inpara && se->v_pos_valid) {
	if (!s->vp) {
	    bu_vls_printf(s->log_str,
		    "ERROR: ECMD_SKETCH_PICK_VERTEX proximity: no view attached\n");
	    se->v_pos_valid = 0;
	    return BRLCAD_ERROR;
	}
	if (skt->vert_count == 0) {
	    bu_vls_printf(s->log_str,
		    "ERROR: ECMD_SKETCH_PICK_VERTEX proximity: sketch has no vertices\n");
	    se->v_pos_valid = 0;
	    return BRLCAD_ERROR;
	}
	mat_t model2objview;
	bn_mat_mul(model2objview, s->vp->gv_model2view, s->model_changes);

	int vi = sketch_closest_vertex(skt, model2objview, se->v_pos);
	se->v_pos_valid = 0;
	if (vi < 0) {
	    bu_vls_printf(s->log_str,
		    "ERROR: ECMD_SKETCH_PICK_VERTEX proximity: failed\n");
	    return BRLCAD_ERROR;
	}
	se->curr_vert = vi;
	return 0;
    }

    /* Index path: caller supplied vertex index in e_para[0] */
    if (!s->e_inpara) {
	bu_vls_printf(s->log_str, "ERROR: vertex index required\n");
	return BRLCAD_ERROR;
    }

    int vi = (int)s->e_para[0];
    if (vi < 0 || (size_t)vi >= skt->vert_count) {
	bu_vls_printf(s->log_str,
		      "ERROR: vertex index %d out of range [0, %zu)\n",
		      vi, skt->vert_count);
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }

    se->curr_vert = vi;
    s->e_inpara = 0;
    return 0;
}

static int
ecmd_sketch_move_vertex(struct rt_edit *s)
{
    struct rt_sketch_edit *se = (struct rt_sketch_edit *)s->ipe_ptr;
    struct rt_sketch_internal *skt =
	(struct rt_sketch_internal *)s->es_int.idb_ptr;
    RT_SKETCH_CK_MAGIC(skt);

    if (se->curr_vert < 0) {
	bu_vls_printf(s->log_str, "ERROR: no vertex selected (use ECMD_SKETCH_PICK_VERTEX first)\n");
	return BRLCAD_ERROR;
    }
    if (!s->e_inpara || s->e_inpara < 2) {
	bu_vls_printf(s->log_str, "ERROR: two parameters required (U V in mm)\n");
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }

    skt->verts[se->curr_vert][0] = s->e_para[0] * s->local2base;
    skt->verts[se->curr_vert][1] = s->e_para[1] * s->local2base;
    rt_edit_snap_point(skt->verts[se->curr_vert], s);
    s->e_inpara = 0;
    return 0;
}

/* Move a list of vertices by a common UV delta.
 * e_para[0..1] = U, V delta in local units
 * e_para[2..e_inpara-1] = vertex indices
 * e_inpara must be >= 3 */
static int
ecmd_sketch_move_vertex_list(struct rt_edit *s)
{
    struct rt_sketch_internal *skt =
	(struct rt_sketch_internal *)s->es_int.idb_ptr;
    RT_SKETCH_CK_MAGIC(skt);

    if (!s->e_inpara || s->e_inpara < 3) {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_SKETCH_MOVE_VERTEX_LIST: need U-delta, V-delta, "
		"and at least one vertex index (e_inpara=%d)\n", s->e_inpara);
	return BRLCAD_ERROR;
    }

    fastf_t du = s->e_para[0] * s->local2base;
    fastf_t dv = s->e_para[1] * s->local2base;

    int n_moved = 0;
    for (int k = 2; k < s->e_inpara; k++) {
	int vi = (int)s->e_para[k];
	if (vi < 0 || (size_t)vi >= skt->vert_count)
	    continue;  /* skip out-of-range silently */
	skt->verts[vi][0] += du;
	skt->verts[vi][1] += dv;
	rt_edit_snap_point(skt->verts[vi], s);
	n_moved++;
    }

    s->e_inpara = 0;
    if (!n_moved) {
	bu_vls_printf(s->log_str,
		"WARNING: ECMD_SKETCH_MOVE_VERTEX_LIST: no valid vertices moved\n");
    }
    return 0;
}

static int
ecmd_sketch_pick_segment(struct rt_edit *s)
{
    struct rt_sketch_edit *se = (struct rt_sketch_edit *)s->ipe_ptr;
    struct rt_sketch_internal *skt =
	(struct rt_sketch_internal *)s->es_int.idb_ptr;
    RT_SKETCH_CK_MAGIC(skt);

    if (!s->e_inpara) {
	bu_vls_printf(s->log_str, "ERROR: segment index required\n");
	return BRLCAD_ERROR;
    }

    int si = (int)s->e_para[0];
    if (si < 0 || (size_t)si >= skt->curve.count) {
	bu_vls_printf(s->log_str,
		      "ERROR: segment index %d out of range [0, %zu)\n",
		      si, skt->curve.count);
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }

    se->curr_seg = si;
    s->e_inpara = 0;
    return 0;
}

/* Collect the set of vertex indices referenced by segment[si] */
static void
sketch_seg_verts(const struct rt_sketch_internal *skt, int si,
		 int *verts_out, int *count_out)
{
    *count_out = 0;
    void *seg = skt->curve.segment[si];
    if (!seg) return;
    uint32_t magic = *(uint32_t *)seg;
    if (magic == CURVE_LSEG_MAGIC) {
	struct line_seg *ls = (struct line_seg *)seg;
	verts_out[(*count_out)++] = ls->start;
	verts_out[(*count_out)++] = ls->end;
    } else if (magic == CURVE_CARC_MAGIC) {
	struct carc_seg *cs = (struct carc_seg *)seg;
	verts_out[(*count_out)++] = cs->start;
	verts_out[(*count_out)++] = cs->end;
    } else if (magic == CURVE_BEZIER_MAGIC) {
	struct bezier_seg *bs = (struct bezier_seg *)seg;
	int j;
	for (j = 0; j <= bs->degree; j++)
	    verts_out[(*count_out)++] = bs->ctl_points[j];
    } else if (magic == CURVE_NURB_MAGIC) {
	struct nurb_seg *ns = (struct nurb_seg *)seg;
	int j;
	for (j = 0; j < ns->c_size; j++)
	    verts_out[(*count_out)++] = ns->ctl_points[j];
    }
}

static int
ecmd_sketch_move_segment(struct rt_edit *s)
{
    struct rt_sketch_edit *se = (struct rt_sketch_edit *)s->ipe_ptr;
    struct rt_sketch_internal *skt =
	(struct rt_sketch_internal *)s->es_int.idb_ptr;
    RT_SKETCH_CK_MAGIC(skt);

    if (se->curr_seg < 0) {
	bu_vls_printf(s->log_str, "ERROR: no segment selected (use ECMD_SKETCH_PICK_SEGMENT first)\n");
	return BRLCAD_ERROR;
    }
    if (!s->e_inpara || s->e_inpara < 2) {
	bu_vls_printf(s->log_str, "ERROR: two parameters required (dU dV in mm)\n");
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }

    fastf_t du = s->e_para[0] * s->local2base;
    fastf_t dv = s->e_para[1] * s->local2base;

    int verts[64]; /* bezier degree <= 63 is more than enough */
    int nv = 0;
    sketch_seg_verts(skt, se->curr_seg, verts, &nv);
    int j;
    for (j = 0; j < nv; j++) {
	int vi = verts[j];
	skt->verts[vi][0] += du;
	skt->verts[vi][1] += dv;
    }

    s->e_inpara = 0;
    return 0;
}

static int
ecmd_sketch_append_line(struct rt_edit *s)
{
    struct rt_sketch_internal *skt =
	(struct rt_sketch_internal *)s->es_int.idb_ptr;
    RT_SKETCH_CK_MAGIC(skt);

    if (!s->e_inpara || s->e_inpara < 2) {
	bu_vls_printf(s->log_str, "ERROR: two vertex indices required (start end)\n");
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }

    int v0 = (int)s->e_para[0];
    int v1 = (int)s->e_para[1];
    if (v0 < 0 || (size_t)v0 >= skt->vert_count ||
	v1 < 0 || (size_t)v1 >= skt->vert_count) {
	bu_vls_printf(s->log_str,
		      "ERROR: vertex index out of range (have %zu verts)\n",
		      skt->vert_count);
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }

    struct line_seg *ls;
    BU_ALLOC(ls, struct line_seg);
    ls->magic = CURVE_LSEG_MAGIC;
    ls->start = v0;
    ls->end   = v1;

    size_t old_count = skt->curve.count;
    skt->curve.count++;
    skt->curve.segment = (void **)bu_realloc(
	    skt->curve.segment,
	    skt->curve.count * sizeof(void *),
	    "sketch curve segments");
    skt->curve.reverse = (int *)bu_realloc(
	    skt->curve.reverse,
	    skt->curve.count * sizeof(int),
	    "sketch curve reverse");
    skt->curve.segment[old_count] = (void *)ls;
    skt->curve.reverse[old_count] = 0;

    s->e_inpara = 0;
    return 0;
}

static int
ecmd_sketch_append_arc(struct rt_edit *s)
{
    struct rt_sketch_internal *skt =
	(struct rt_sketch_internal *)s->es_int.idb_ptr;
    RT_SKETCH_CK_MAGIC(skt);

    /* e_para: [0]=start_vi [1]=end_vi [2]=radius_mm [3]=center_is_left [4]=orientation
     * e_inpara must be 5. */
    if (!s->e_inpara || s->e_inpara < 5) {
	bu_vls_printf(s->log_str,
		"ERROR: 5 parameters required "
		"(start_vi end_vi radius_mm center_is_left orientation)\n");
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }

    int v0  = (int)s->e_para[0];
    int v1  = (int)s->e_para[1];
    if (v0 < 0 || (size_t)v0 >= skt->vert_count ||
	v1 < 0 || (size_t)v1 >= skt->vert_count) {
	bu_vls_printf(s->log_str,
		      "ERROR: vertex index out of range (have %zu verts)\n",
		      skt->vert_count);
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }

    struct carc_seg *cs;
    BU_ALLOC(cs, struct carc_seg);
    cs->magic          = CURVE_CARC_MAGIC;
    cs->start          = v0;
    cs->end            = v1;
    cs->radius         = s->e_para[2] * s->local2base;
    cs->center_is_left = (int)s->e_para[3];
    cs->orientation    = (int)s->e_para[4];
    cs->center         = -1; /* computed during sketch tessellation */

    size_t old_count = skt->curve.count;
    skt->curve.count++;
    skt->curve.segment = (void **)bu_realloc(
	    skt->curve.segment,
	    skt->curve.count * sizeof(void *),
	    "sketch curve segments");
    skt->curve.reverse = (int *)bu_realloc(
	    skt->curve.reverse,
	    skt->curve.count * sizeof(int),
	    "sketch curve reverse");
    skt->curve.segment[old_count] = (void *)cs;
    skt->curve.reverse[old_count] = 0;

    s->e_inpara = 0;
    return 0;
}

static int
ecmd_sketch_append_bezier(struct rt_edit *s)
{
    struct rt_sketch_internal *skt =
	(struct rt_sketch_internal *)s->es_int.idb_ptr;
    RT_SKETCH_CK_MAGIC(skt);

    /* All e_inpara values are control point vertex indices.
     * degree = e_inpara - 1.
     * e_inpara must be >= 2 (at least linear bezier). */
    if (!s->e_inpara || s->e_inpara < 2) {
	bu_vls_printf(s->log_str,
		"ERROR: at least 2 vertex indices required "
		"(e_inpara = degree+1 control points)\n");
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }

    int degree = s->e_inpara - 1;

    struct bezier_seg *bs;
    BU_ALLOC(bs, struct bezier_seg);
    bs->magic  = CURVE_BEZIER_MAGIC;
    bs->degree = degree;
    bs->ctl_points = (int *)bu_malloc((degree + 1) * sizeof(int), "bezier ctl_points");
    int j;
    for (j = 0; j <= degree; j++) {
	int vi = (int)s->e_para[j];
	if (vi < 0 || (size_t)vi >= skt->vert_count) {
	    bu_vls_printf(s->log_str,
			  "ERROR: control point index %d out of range\n", vi);
	    bu_free(bs->ctl_points, "bezier ctl_points");
	    BU_FREE(bs, struct bezier_seg);
	    s->e_inpara = 0;
	    return BRLCAD_ERROR;
	}
	bs->ctl_points[j] = vi;
    }

    size_t old_count = skt->curve.count;
    skt->curve.count++;
    skt->curve.segment = (void **)bu_realloc(
	    skt->curve.segment,
	    skt->curve.count * sizeof(void *),
	    "sketch curve segments");
    skt->curve.reverse = (int *)bu_realloc(
	    skt->curve.reverse,
	    skt->curve.count * sizeof(int),
	    "sketch curve reverse");
    skt->curve.segment[old_count] = (void *)bs;
    skt->curve.reverse[old_count] = 0;

    s->e_inpara = 0;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Segment-split helpers                                               */
/* ------------------------------------------------------------------ */

/* Append one vertex to the sketch; returns the new vertex index. */
static int
sketch_add_vertex(struct rt_sketch_internal *skt, fastf_t u, fastf_t v)
{
    int vi = (int)skt->vert_count;
    skt->verts = (point2d_t *)bu_realloc(skt->verts,
	    (skt->vert_count + 1) * sizeof(point2d_t), "sketch verts");
    V2SET(skt->verts[vi], u, v);
    skt->vert_count++;
    return vi;
}

/* Insert a new segment pointer at position pos, shifting higher indices up. */
static void
sketch_insert_segment(struct rt_sketch_internal *skt, size_t pos, void *new_seg)
{
    skt->curve.count++;
    skt->curve.segment = (void **)bu_realloc(skt->curve.segment,
	    skt->curve.count * sizeof(void *), "sketch curve segments");
    skt->curve.reverse = (int *)bu_realloc(skt->curve.reverse,
	    skt->curve.count * sizeof(int), "sketch curve reverse");
    for (size_t i = skt->curve.count - 1; i > pos; i--) {
	skt->curve.segment[i] = skt->curve.segment[i - 1];
	skt->curve.reverse[i] = skt->curve.reverse[i - 1];
    }
    skt->curve.segment[pos] = new_seg;
    skt->curve.reverse[pos] = 0;
}

/*
 * Compute which side of the directed line from (sx,sy)→(ex,ey) the
 * center (cx,cy) lies on.  Returns 1 if the center is to the LEFT
 * (cross product > 0), 0 otherwise.  Matches the convention used by
 * sketch.c seg_to_vlist() and skt_ed.tcl find_arc_center.
 */
static int
arc_center_is_left(fastf_t sx, fastf_t sy,
		   fastf_t ex, fastf_t ey,
		   fastf_t cx, fastf_t cy)
{
    /* cross product z-component of (end-start) × (center-start) */
    fastf_t cross_z = (ex - sx) * (cy - sy) - (ey - sy) * (cx - sx);
    return (cross_z > 0.0) ? 1 : 0;
}

static int
ecmd_sketch_split_segment(struct rt_edit *s)
{
    struct rt_sketch_edit *se = (struct rt_sketch_edit *)s->ipe_ptr;
    struct rt_sketch_internal *skt =
	(struct rt_sketch_internal *)s->es_int.idb_ptr;
    RT_SKETCH_CK_MAGIC(skt);

    if (!s->e_inpara || s->e_inpara < 2) {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_SKETCH_SPLIT_SEGMENT: segment index and t "
		"parameter required (e_inpara=%d)\n", s->e_inpara);
	return BRLCAD_ERROR;
    }

    int si     = (int)s->e_para[0];
    fastf_t t  = s->e_para[1];

    if (si < 0 || (size_t)si >= skt->curve.count) {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_SKETCH_SPLIT_SEGMENT: segment index %d "
		"out of range [0, %zu)\n", si, skt->curve.count);
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }
    if (t <= 0.0 || t >= 1.0) {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_SKETCH_SPLIT_SEGMENT: t=%g must be in (0,1)\n", t);
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }

    void *seg = skt->curve.segment[si];
    uint32_t magic = *(uint32_t *)seg;

    if (magic == CURVE_LSEG_MAGIC) {
	/*
	 * Linear segment: split point = (1-t)*P0 + t*P1.
	 * Replace original with line start→split; insert line split→end.
	 */
	struct line_seg *ls = (struct line_seg *)seg;
	fastf_t pu = (1.0 - t) * skt->verts[ls->start][0]
		   + t           * skt->verts[ls->end][0];
	fastf_t pv = (1.0 - t) * skt->verts[ls->start][1]
		   + t           * skt->verts[ls->end][1];
	int new_vi  = sketch_add_vertex(skt, pu, pv);
	int old_end = ls->end;
	ls->end = new_vi;

	struct line_seg *ls2;
	BU_ALLOC(ls2, struct line_seg);
	ls2->magic = CURVE_LSEG_MAGIC;
	ls2->start = new_vi;
	ls2->end   = old_end;
	sketch_insert_segment(skt, (size_t)si + 1, (void *)ls2);

    } else if (magic == CURVE_CARC_MAGIC) {
	/*
	 * Circular arc.  Full-circle arcs (radius < 0) are not supported.
	 *
	 * Center computation replicates skt_ed.tcl find_arc_center and
	 * sketch.c seg_to_vlist so that center_is_left is set correctly
	 * for both sub-arcs.  "Center is to the left" is defined as the
	 * cross product (end-start) × (center-start) > 0.
	 */
	struct carc_seg *cs = (struct carc_seg *)seg;

	if (cs->radius < 0.0) {
	    bu_vls_printf(s->log_str,
		    "ERROR: ECMD_SKETCH_SPLIT_SEGMENT: "
		    "cannot split a full-circle arc\n");
	    s->e_inpara = 0;
	    return BRLCAD_ERROR;
	}

	fastf_t sx = skt->verts[cs->start][0];
	fastf_t sy = skt->verts[cs->start][1];
	fastf_t ex = skt->verts[cs->end][0];
	fastf_t ey = skt->verts[cs->end][1];
	fastf_t r  = cs->radius;

	/* Perpendicular-bisector centre computation (same as sketch.c) */
	fastf_t midx = (sx + ex) * 0.5;
	fastf_t midy = (sy + ey) * 0.5;
	fastf_t s2mx = midx - sx;
	fastf_t s2my = midy - sy;
	fastf_t s2m_len_sq = s2mx*s2mx + s2my*s2my;
	if (s2m_len_sq <= SMALL_FASTF) {
	    bu_vls_printf(s->log_str,
		    "ERROR: ECMD_SKETCH_SPLIT_SEGMENT: arc start/end too close\n");
	    s->e_inpara = 0;
	    return BRLCAD_ERROR;
	}
	fastf_t len_sq = r*r - s2m_len_sq;
	if (len_sq < 0.0) {
	    bu_vls_printf(s->log_str,
		    "ERROR: ECMD_SKETCH_SPLIT_SEGMENT: arc radius too small\n");
	    s->e_inpara = 0;
	    return BRLCAD_ERROR;
	}
	/* dir = perpendicular to start→end (normalized) */
	fastf_t dirx = -s2my;
	fastf_t diry =  s2mx;
	fastf_t dir_len = sqrt(dirx*dirx + diry*diry);
	dirx /= dir_len;
	diry /= dir_len;
	fastf_t tmp_len = sqrt(len_sq);
	fastf_t cx = midx + tmp_len * dirx;
	fastf_t cy = midy + tmp_len * diry;
	/* Choose the side that matches center_is_left */
	fastf_t cross_z = (ex - sx) * (cy - sy) - (ey - sy) * (cx - sx);
	if (!(cross_z > 0.0 && cs->center_is_left))
	    { cx = midx - tmp_len * dirx; cy = midy - tmp_len * diry; }

	/* Angular span for the arc (matches sketch.c orientation logic) */
	fastf_t start_ang = atan2(sy - cy, sx - cx);
	fastf_t end_ang   = atan2(ey - cy, ex - cx);
	if (cs->orientation) {
	    while (end_ang > start_ang) end_ang -= M_2PI;
	} else {
	    while (end_ang < start_ang) end_ang += M_2PI;
	}

	/* Split point at parameter t */
	fastf_t split_ang = start_ang + t * (end_ang - start_ang);
	fastf_t spu = cx + r * cos(split_ang);
	fastf_t spv = cy + r * sin(split_ang);
	int new_vi = sketch_add_vertex(skt, spu, spv);

	/* center_is_left for first sub-arc (start → split) */
	int cil1 = arc_center_is_left(sx, sy, spu, spv, cx, cy);
	/* center_is_left for second sub-arc (split → end) */
	int cil2 = arc_center_is_left(spu, spv, ex, ey, cx, cy);

	int old_end = cs->end;
	cs->end            = new_vi;
	cs->center_is_left = cil1;
	cs->center         = -1;

	struct carc_seg *cs2;
	BU_ALLOC(cs2, struct carc_seg);
	cs2->magic          = CURVE_CARC_MAGIC;
	cs2->start          = new_vi;
	cs2->end            = old_end;
	cs2->radius         = r;
	cs2->center_is_left = cil2;
	cs2->orientation    = cs->orientation;
	cs2->center         = -1;
	sketch_insert_segment(skt, (size_t)si + 1, (void *)cs2);

    } else if (magic == CURVE_BEZIER_MAGIC) {
	/*
	 * Bezier segment: de Casteljau subdivision at parameter t.
	 *
	 * Replicates the calc_bezier logic from skt_ed.tcl.  Given
	 * degree-d control points P[0..d], we build the full triangle
	 * Q[k][j] = (1-t)*Q[k-1][j] + t*Q[k-1][j+1].
	 * Left  control points: L[k] = Q[k][0]   (k = 0..d)
	 * Right control points: R[k] = Q[d-k][k] (k = 0..d)
	 *
	 * New vertices are added for L[1..d] (the last one is the split
	 * point) and for R[1..d-1].  L[0] = P[0] and R[d] = P[d] reuse
	 * existing vertex indices.
	 */
	struct bezier_seg *bs = (struct bezier_seg *)seg;
	int d = bs->degree;

	/* Build de Casteljau triangle in UV space (max degree 19) */
	fastf_t Qu[20][20], Qv[20][20];
	for (int j = 0; j <= d; j++) {
	    Qu[0][j] = skt->verts[bs->ctl_points[j]][0];
	    Qv[0][j] = skt->verts[bs->ctl_points[j]][1];
	}
	for (int k = 1; k <= d; k++) {
	    for (int j = 0; j <= d - k; j++) {
		Qu[k][j] = (1.0 - t) * Qu[k-1][j] + t * Qu[k-1][j+1];
		Qv[k][j] = (1.0 - t) * Qv[k-1][j] + t * Qv[k-1][j+1];
	    }
	}

	/* Left control points: L[k] = Q[k][0] */
	int left_verts[20];
	left_verts[0] = bs->ctl_points[0]; /* P[0] — existing */
	for (int k = 1; k <= d; k++)
	    left_verts[k] = sketch_add_vertex(skt, Qu[k][0], Qv[k][0]);

	/* Right control points: R[k] = Q[d-k][k] */
	int right_verts[20];
	right_verts[0] = left_verts[d];    /* split vertex */
	for (int k = 1; k <= d - 1; k++)
	    right_verts[k] = sketch_add_vertex(skt, Qu[d-k][k], Qv[d-k][k]);
	right_verts[d] = bs->ctl_points[d]; /* P[d] — existing */

	/* Create the right Bezier segment */
	struct bezier_seg *bs2;
	BU_ALLOC(bs2, struct bezier_seg);
	bs2->magic      = CURVE_BEZIER_MAGIC;
	bs2->degree     = d;
	bs2->ctl_points = (int *)bu_malloc((d + 1) * sizeof(int),
					   "bezier ctl_points");
	for (int k = 0; k <= d; k++)
	    bs2->ctl_points[k] = right_verts[k];

	/* Update original to be the left Bezier segment */
	for (int k = 0; k <= d; k++)
	    bs->ctl_points[k] = left_verts[k];

	sketch_insert_segment(skt, (size_t)si + 1, (void *)bs2);

    } else {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_SKETCH_SPLIT_SEGMENT: segment type not supported "
		"(only LINE, CARC, BEZIER)\n");
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }

    se->curr_seg = -1;
    s->e_inpara  = 0;
    return 0;
}

static int
ecmd_sketch_delete_vertex(struct rt_edit *s)
{
    struct rt_sketch_edit *se = (struct rt_sketch_edit *)s->ipe_ptr;
    struct rt_sketch_internal *skt =
	(struct rt_sketch_internal *)s->es_int.idb_ptr;
    RT_SKETCH_CK_MAGIC(skt);

    if (se->curr_vert < 0) {
	bu_vls_printf(s->log_str, "ERROR: no vertex selected\n");
	return BRLCAD_ERROR;
    }
    int vi = se->curr_vert;
    if (sketch_vert_is_used(skt, vi)) {
	bu_vls_printf(s->log_str,
		      "ERROR: vertex %d is referenced by a segment; delete the segment first\n",
		      vi);
	return BRLCAD_ERROR;
    }

    /* Remove vertex vi by collapsing the array */
    size_t i;
    for (i = vi; i + 1 < skt->vert_count; i++) {
	V2MOVE(skt->verts[i], skt->verts[i + 1]);
    }
    skt->vert_count--;

    /* Fix up all segment vertex references > vi */
    for (i = 0; i < skt->curve.count; i++) {
	void *seg = skt->curve.segment[i];
	if (!seg) continue;
	uint32_t magic = *(uint32_t *)seg;
	if (magic == CURVE_LSEG_MAGIC) {
	    struct line_seg *ls = (struct line_seg *)seg;
	    if (ls->start > vi) ls->start--;
	    if (ls->end   > vi) ls->end--;
	} else if (magic == CURVE_CARC_MAGIC) {
	    struct carc_seg *cs = (struct carc_seg *)seg;
	    if (cs->start > vi) cs->start--;
	    if (cs->end   > vi) cs->end--;
	} else if (magic == CURVE_BEZIER_MAGIC) {
	    struct bezier_seg *bs = (struct bezier_seg *)seg;
	    int j;
	    for (j = 0; j <= bs->degree; j++)
		if (bs->ctl_points[j] > vi) bs->ctl_points[j]--;
	} else if (magic == CURVE_NURB_MAGIC) {
	    struct nurb_seg *ns = (struct nurb_seg *)seg;
	    int j;
	    for (j = 0; j < ns->c_size; j++)
		if (ns->ctl_points[j] > vi) ns->ctl_points[j]--;
	}
    }

    se->curr_vert = -1;
    s->e_inpara = 0;
    return 0;
}

static int
ecmd_sketch_delete_segment(struct rt_edit *s)
{
    struct rt_sketch_edit *se = (struct rt_sketch_edit *)s->ipe_ptr;
    struct rt_sketch_internal *skt =
	(struct rt_sketch_internal *)s->es_int.idb_ptr;
    RT_SKETCH_CK_MAGIC(skt);

    if (se->curr_seg < 0) {
	bu_vls_printf(s->log_str, "ERROR: no segment selected\n");
	return BRLCAD_ERROR;
    }

    int si = se->curr_seg;

    /* Free the segment data */
    void *seg = skt->curve.segment[si];
    if (seg) {
	uint32_t magic = *(uint32_t *)seg;
	if (magic == CURVE_BEZIER_MAGIC) {
	    struct bezier_seg *bs = (struct bezier_seg *)seg;
	    bu_free(bs->ctl_points, "bezier ctl_points");
	} else if (magic == CURVE_NURB_MAGIC) {
	    struct nurb_seg *ns = (struct nurb_seg *)seg;
	    bu_free(ns->ctl_points, "nurb ctl_points");
	    bu_free(ns->k.knots,    "nurb knots");
	    if (ns->weights)
		bu_free(ns->weights, "nurb weights");
	}
	bu_free(seg, "sketch segment");
    }

    /* Collapse the segment array */
    size_t i;
    for (i = si; i + 1 < skt->curve.count; i++) {
	skt->curve.segment[i] = skt->curve.segment[i + 1];
	skt->curve.reverse[i] = skt->curve.reverse[i + 1];
    }
    skt->curve.count--;

    se->curr_seg = -1;
    s->e_inpara = 0;
    return 0;
}


/* ------------------------------------------------------------------ */
/* NURB add / edit operations                                          */
/* ------------------------------------------------------------------ */

/*
 * Build a clamped uniform knot vector for a B-spline of given order and
 * c_size control points.  The caller must supply a `knots` array with at
 * least (order + c_size) allocated elements.  The resulting k_size is
 * order + c_size.
 *
 * The domain is [0, c_size - order + 1]:
 *   knots[0 .. order-1]              = 0
 *   knots[order .. c_size-1]         = 1, 2, … (interior, integer steps)
 *   knots[c_size .. k_size-1]        = c_size - order + 1 (end value)
 *
 * This produces the standard clamped uniform B-spline knot vector used
 * by the sketch primitive's NURB evaluation code (which calls
 * nmg_nurb_c_eval over [knots[0], knots[k_size-1]]).
 */
static void
sketch_make_clamped_uniform_kv(fastf_t *knots, int order, int c_size)
{
    int k_size = order + c_size;
    int end_val = c_size - order + 1;  /* total interior spans + 1 */
    int i;

    /* Clamped start */
    for (i = 0; i < order; i++)
	knots[i] = 0.0;

    /* Interior knots: uniformly spaced at integer values 1, 2, … */
    for (i = order; i < c_size; i++)
	knots[i] = (fastf_t)(i - order + 1);

    /* Clamped end */
    for (i = c_size; i < k_size; i++)
	knots[i] = (fastf_t)end_val;
}

static int
ecmd_sketch_append_nurb(struct rt_edit *s)
{
    struct rt_sketch_internal *skt =
	(struct rt_sketch_internal *)s->es_int.idb_ptr;
    RT_SKETCH_CK_MAGIC(skt);

    /*
     * e_para[0]            = order (integer ≥ 2)
     * e_para[1..e_inpara-1]= control point vertex indices (c_size = e_inpara-1)
     * e_inpara             ≥ 1 + order  (need at least c_size ≥ order)
     */
    if (!s->e_inpara || s->e_inpara < 2) {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_SKETCH_APPEND_NURB: at least order + 1 control points required "
		"(e_para[0]=order, e_para[1..]=vert_indices)\n");
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }

    int order  = (int)s->e_para[0];
    int c_size = s->e_inpara - 1;

    if (order < 2) {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_SKETCH_APPEND_NURB: order must be >= 2 (got %d)\n", order);
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }
    if (c_size < order) {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_SKETCH_APPEND_NURB: c_size (%d) must be >= order (%d)\n",
		c_size, order);
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }

    /* Validate all control point indices */
    int j;
    for (j = 0; j < c_size; j++) {
	int vi = (int)s->e_para[1 + j];
	if (vi < 0 || (size_t)vi >= skt->vert_count) {
	    bu_vls_printf(s->log_str,
		    "ERROR: ECMD_SKETCH_APPEND_NURB: control point index %d "
		    "out of range [0, %zu)\n", vi, skt->vert_count);
	    s->e_inpara = 0;
	    return BRLCAD_ERROR;
	}
    }

    int k_size = order + c_size;

    struct nurb_seg *ns;
    BU_ALLOC(ns, struct nurb_seg);
    ns->magic   = CURVE_NURB_MAGIC;
    ns->order   = order;
    /* Non-rational 2-D point type — the correct type for 2-D sketch curves.
     * RT_NURB_MAKE_PT_TYPE(n_coords, point_type, rational):
     *   n_coords   = 2 (U, V plane coordinates)
     *   point_type = RT_NURB_PT_XY (= 1)
     *   rational   = RT_NURB_PT_NONRAT (= 0) */
    ns->pt_type = RT_NURB_MAKE_PT_TYPE(2, RT_NURB_PT_XY, RT_NURB_PT_NONRAT);
    ns->c_size  = c_size;
    ns->weights = (fastf_t *)NULL;

    ns->k.k_size = k_size;
    ns->k.knots  = (fastf_t *)bu_malloc(k_size * sizeof(fastf_t), "nurb knots");
    sketch_make_clamped_uniform_kv(ns->k.knots, order, c_size);

    ns->ctl_points = (int *)bu_malloc(c_size * sizeof(int), "nurb ctl_points");
    for (j = 0; j < c_size; j++)
	ns->ctl_points[j] = (int)s->e_para[1 + j];

    size_t old_count = skt->curve.count;
    skt->curve.count++;
    skt->curve.segment = (void **)bu_realloc(
	    skt->curve.segment,
	    skt->curve.count * sizeof(void *),
	    "sketch curve segments");
    skt->curve.reverse = (int *)bu_realloc(
	    skt->curve.reverse,
	    skt->curve.count * sizeof(int),
	    "sketch curve reverse");
    skt->curve.segment[old_count] = (void *)ns;
    skt->curve.reverse[old_count] = 0;

    s->e_inpara = 0;
    return 0;
}

static int
ecmd_sketch_nurb_edit_kv(struct rt_edit *s)
{
    struct rt_sketch_edit *se = (struct rt_sketch_edit *)s->ipe_ptr;
    struct rt_sketch_internal *skt =
	(struct rt_sketch_internal *)s->es_int.idb_ptr;
    RT_SKETCH_CK_MAGIC(skt);

    if (se->curr_seg < 0) {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_SKETCH_NURB_EDIT_KV: no segment selected\n");
	return BRLCAD_ERROR;
    }

    void *seg = skt->curve.segment[se->curr_seg];
    if (!seg || *(uint32_t *)seg != CURVE_NURB_MAGIC) {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_SKETCH_NURB_EDIT_KV: selected segment is not a NURB\n");
	return BRLCAD_ERROR;
    }
    struct nurb_seg *ns = (struct nurb_seg *)seg;

    /*
     * e_para[0]            = k_size (must equal ns->order + ns->c_size)
     * e_para[1..e_inpara-1]= new knot values
     * e_inpara             = 1 + k_size
     */
    if (!s->e_inpara || s->e_inpara < 2) {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_SKETCH_NURB_EDIT_KV: k_size and knot values required\n");
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }

    int k_size_in = (int)s->e_para[0];
    int expected  = ns->order + ns->c_size;
    if (k_size_in != expected) {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_SKETCH_NURB_EDIT_KV: k_size %d does not match "
		"order+c_size = %d+%d = %d\n",
		k_size_in, ns->order, ns->c_size, expected);
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }
    if (s->e_inpara != 1 + k_size_in) {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_SKETCH_NURB_EDIT_KV: e_inpara (%d) must equal 1 + k_size (%d)\n",
		s->e_inpara, 1 + k_size_in);
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }

    /* Verify non-decreasing order */
    int i;
    for (i = 0; i < k_size_in - 1; i++) {
	if (s->e_para[1 + i + 1] < s->e_para[1 + i]) {
	    bu_vls_printf(s->log_str,
		    "ERROR: ECMD_SKETCH_NURB_EDIT_KV: knot vector must be "
		    "non-decreasing (knot[%d]=%g > knot[%d]=%g)\n",
		    i, s->e_para[1 + i], i + 1, s->e_para[1 + i + 1]);
	    s->e_inpara = 0;
	    return BRLCAD_ERROR;
	}
    }

    /* Replace the knot array */
    bu_free(ns->k.knots, "nurb knots");
    ns->k.knots = (fastf_t *)bu_malloc(k_size_in * sizeof(fastf_t), "nurb knots");
    for (i = 0; i < k_size_in; i++)
	ns->k.knots[i] = s->e_para[1 + i];
    ns->k.k_size = k_size_in;

    s->e_inpara = 0;
    return 0;
}

static int
ecmd_sketch_nurb_edit_weights(struct rt_edit *s)
{
    struct rt_sketch_internal *skt =
	(struct rt_sketch_internal *)s->es_int.idb_ptr;
    RT_SKETCH_CK_MAGIC(skt);

    /*
     * e_para[0]            = segment index (must be a NURB)
     * e_para[1]            = c_size_expected (must match nsg->c_size)
     * e_para[2..e_inpara-1]= weight values (one per control point)
     * e_inpara             = 2 + c_size
     */
    if (!s->e_inpara || s->e_inpara < 3) {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_SKETCH_NURB_EDIT_WEIGHTS: seg_index, c_size, and "
		"at least one weight required\n");
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }

    int si = (int)s->e_para[0];
    if (si < 0 || (size_t)si >= skt->curve.count) {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_SKETCH_NURB_EDIT_WEIGHTS: segment index %d "
		"out of range [0, %zu)\n", si, skt->curve.count);
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }

    void *seg = skt->curve.segment[si];
    if (!seg || *(uint32_t *)seg != CURVE_NURB_MAGIC) {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_SKETCH_NURB_EDIT_WEIGHTS: segment %d is not a NURB\n", si);
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }
    struct nurb_seg *ns = (struct nurb_seg *)seg;

    int c_size_expected = (int)s->e_para[1];
    if (c_size_expected != ns->c_size) {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_SKETCH_NURB_EDIT_WEIGHTS: c_size mismatch "
		"(expected %d, segment has %d)\n",
		c_size_expected, ns->c_size);
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }
    if (s->e_inpara != 2 + ns->c_size) {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_SKETCH_NURB_EDIT_WEIGHTS: e_inpara (%d) must equal "
		"2 + c_size (%d)\n", s->e_inpara, 2 + ns->c_size);
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }

    /* Validate all weights > 0 */
    int i;
    for (i = 0; i < ns->c_size; i++) {
	if (s->e_para[2 + i] <= 0.0) {
	    bu_vls_printf(s->log_str,
		    "ERROR: ECMD_SKETCH_NURB_EDIT_WEIGHTS: weight[%d] = %g "
		    "must be > 0\n", i, s->e_para[2 + i]);
	    s->e_inpara = 0;
	    return BRLCAD_ERROR;
	}
    }

    /* Allocate or reuse weights array */
    if (!ns->weights)
	ns->weights = (fastf_t *)bu_malloc(ns->c_size * sizeof(fastf_t), "nurb weights");

    for (i = 0; i < ns->c_size; i++)
	ns->weights[i] = s->e_para[2 + i];

    /* Update pt_type to rational 2-D if not already */
    ns->pt_type = RT_NURB_MAKE_PT_TYPE(2, RT_NURB_PT_XY, RT_NURB_PT_RATIONAL);

    /* Update curr_seg so callers can chain with ECMD_SKETCH_NURB_EDIT_KV */
    struct rt_sketch_edit *se = (struct rt_sketch_edit *)s->ipe_ptr;
    se->curr_seg = si;

    s->e_inpara = 0;
    return 0;
}

static int
ecmd_sketch_add_vertex(struct rt_edit *s)
{
    struct rt_sketch_edit *se = (struct rt_sketch_edit *)s->ipe_ptr;
    struct rt_sketch_internal *skt =
	(struct rt_sketch_internal *)s->es_int.idb_ptr;
    RT_SKETCH_CK_MAGIC(skt);

    if (!s->e_inpara || s->e_inpara < 2) {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_SKETCH_ADD_VERTEX: two parameters required "
		"(U V in local units)\n");
	return BRLCAD_ERROR;
    }

    /* Grow the vertex array by one */
    size_t new_count = skt->vert_count + 1;
    skt->verts = (point2d_t *)bu_realloc(skt->verts,
	    new_count * sizeof(point2d_t), "sketch verts");

    fastf_t u = s->e_para[0] * s->local2base;
    fastf_t v = s->e_para[1] * s->local2base;
    skt->verts[skt->vert_count][0] = u;
    skt->verts[skt->vert_count][1] = v;
    rt_edit_snap_point(skt->verts[skt->vert_count], s);

    se->curr_vert = (int)skt->vert_count;
    skt->vert_count = new_count;

    s->e_inpara = 0;
    return 0;
}

static int
ecmd_sketch_toggle_arc_orient(struct rt_edit *s)
{
    struct rt_sketch_edit *se = (struct rt_sketch_edit *)s->ipe_ptr;
    struct rt_sketch_internal *skt =
	(struct rt_sketch_internal *)s->es_int.idb_ptr;
    RT_SKETCH_CK_MAGIC(skt);

    if (se->curr_seg < 0) {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_SKETCH_TOGGLE_ARC_ORIENT: no segment selected\n");
	return BRLCAD_ERROR;
    }

    void *seg = skt->curve.segment[se->curr_seg];
    if (!seg || *(uint32_t *)seg != CURVE_CARC_MAGIC) {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_SKETCH_TOGGLE_ARC_ORIENT: "
		"selected segment is not a CARC\n");
	return BRLCAD_ERROR;
    }

    struct carc_seg *cs = (struct carc_seg *)seg;
    if (cs->radius < 0.0) {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_SKETCH_TOGGLE_ARC_ORIENT: "
		"cannot toggle orientation of a full-circle arc "
		"(radius < 0)\n");
	return BRLCAD_ERROR;
    }

    cs->center_is_left = !cs->center_is_left;
    cs->center = -1;  /* force recompute during tessellation */
    return 0;
}

static int
ecmd_sketch_set_arc_radius(struct rt_edit *s)
{
    struct rt_sketch_edit *se = (struct rt_sketch_edit *)s->ipe_ptr;
    struct rt_sketch_internal *skt =
	(struct rt_sketch_internal *)s->es_int.idb_ptr;
    RT_SKETCH_CK_MAGIC(skt);

    if (se->curr_seg < 0) {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_SKETCH_SET_ARC_RADIUS: no segment selected\n");
	return BRLCAD_ERROR;
    }

    if (!s->e_inpara || s->e_inpara < 1) {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_SKETCH_SET_ARC_RADIUS: "
		"radius parameter required (e_para[0] in local units)\n");
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }

    void *seg = skt->curve.segment[se->curr_seg];
    if (!seg || *(uint32_t *)seg != CURVE_CARC_MAGIC) {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_SKETCH_SET_ARC_RADIUS: "
		"selected segment is not a CARC\n");
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }

    struct carc_seg *cs = (struct carc_seg *)seg;
    fastf_t new_r = s->e_para[0] * s->local2base;

    cs->radius = new_r;
    cs->center = -1;  /* force recompute during tessellation */

    s->e_inpara = 0;
    return 0;
}

/*
 * Helper: compute the tangent direction of segment[seg_idx] at vertex
 * vert_idx in the sketch.  Result is a unit vector in (tx_out, ty_out).
 * Returns BRLCAD_OK or BRLCAD_ERROR if degenerate.
 *
 * Line segments: chord direction toward the OTHER vertex.
 * CARC arcs:    radius-perpendicular at the given end (uses
 *               the perpendicular-bisector centre calculation, matching the
 *               Tcl get_tangent_at_vertex implementation).
 * Other types:  returns BRLCAD_ERROR (not supported for tangency).
 */
static int
sketch_tangent_at_vertex(const struct rt_sketch_internal *skt,
			 int seg_idx, int vert_idx,
			 fastf_t *tx_out, fastf_t *ty_out,
			 struct bu_vls *log_str)
{
    void *seg = skt->curve.segment[seg_idx];
    if (!seg) {
	bu_vls_printf(log_str,
		"sketch_tangent_at_vertex: segment[%d] is NULL\n", seg_idx);
	return BRLCAD_ERROR;
    }

    uint32_t magic = *(uint32_t *)seg;

    if (magic == CURVE_LSEG_MAGIC) {
	struct line_seg *ls = (struct line_seg *)seg;
	/* Tangent = direction from vert_idx toward the other end */
	int other = (vert_idx == ls->start) ? ls->end : ls->start;
	fastf_t dx = skt->verts[other][0] - skt->verts[vert_idx][0];
	fastf_t dy = skt->verts[other][1] - skt->verts[vert_idx][1];
	fastf_t len = sqrt(dx*dx + dy*dy);
	if (len < SMALL_FASTF) {
	    bu_vls_printf(log_str,
		    "sketch_tangent_at_vertex: zero-length line segment %d\n",
		    seg_idx);
	    return BRLCAD_ERROR;
	}
	*tx_out = dx / len;
	*ty_out = dy / len;
	return BRLCAD_OK;
    }

    if (magic == CURVE_CARC_MAGIC) {
	struct carc_seg *cs = (struct carc_seg *)seg;
	fastf_t sx = skt->verts[cs->start][0];
	fastf_t sy = skt->verts[cs->start][1];
	fastf_t ex = skt->verts[cs->end  ][0];
	fastf_t ey = skt->verts[cs->end  ][1];

	if (cs->radius < 0.0) {
	    /* Full circle: tangent at start is perpendicular to start→centre */
	    if (vert_idx != cs->start) {
		bu_vls_printf(log_str,
			"sketch_tangent_at_vertex: full circle has no tangent at end\n");
		return BRLCAD_ERROR;
	    }
	    fastf_t dx = ex - sx;  /* centre - start */
	    fastf_t dy = ey - sy;
	    fastf_t len = sqrt(dx*dx + dy*dy);
	    if (len < SMALL_FASTF) {
		bu_vls_printf(log_str,
			"sketch_tangent_at_vertex: full circle degenerate\n");
		return BRLCAD_ERROR;
	    }
	    *tx_out = -dy / len;
	    *ty_out =  dx / len;
	    return BRLCAD_OK;
	}

	/* Partial arc: find centre via perpendicular bisector */
	fastf_t midx = (sx + ex) * 0.5;
	fastf_t midy = (sy + ey) * 0.5;
	fastf_t s2mx = midx - sx, s2my = midy - sy;
	fastf_t s2m_sq = s2mx*s2mx + s2my*s2my;
	if (s2m_sq < SMALL_FASTF) {
	    bu_vls_printf(log_str,
		    "sketch_tangent_at_vertex: arc endpoints coincide\n");
	    return BRLCAD_ERROR;
	}
	fastf_t r = cs->radius;
	fastf_t len_sq = r*r - s2m_sq;
	if (len_sq < 0.0) len_sq = 0.0;
	fastf_t tmp_len = sqrt(len_sq);

	fastf_t dirx = -s2my, diry = s2mx;
	fastf_t dir_len = sqrt(dirx*dirx + diry*diry);
	dirx /= dir_len; diry /= dir_len;

	fastf_t cx = midx + tmp_len * dirx;
	fastf_t cy = midy + tmp_len * diry;
	fastf_t cross_z = (ex - sx)*(cy - sy) - (ey - sy)*(cx - sx);
	if ((cross_z <= 0.0) || !cs->center_is_left) {
	    cx = midx - tmp_len * dirx;
	    cy = midy - tmp_len * diry;
	}

	/* Tangent at vert_idx = perpendicular to (vert - centre),
	 * oriented according to cs->orientation (CW vs CCW) */
	fastf_t vx, vy;
	if (vert_idx == cs->start) {
	    vx = sx - cx; vy = sy - cy;
	} else {
	    vx = ex - cx; vy = ey - cy;
	}
	fastf_t vlen = sqrt(vx*vx + vy*vy);
	if (vlen < SMALL_FASTF) {
	    bu_vls_printf(log_str,
		    "sketch_tangent_at_vertex: arc vertex at centre\n");
	    return BRLCAD_ERROR;
	}
	vx /= vlen; vy /= vlen;

	if (cs->orientation) {
	    *tx_out =  vy; *ty_out = -vx;   /* CW: rotate -90° */
	} else {
	    *tx_out = -vy; *ty_out =  vx;   /* CCW: rotate +90° */
	}
	/* The perpendicular-to-radius formula above gives a vector that points
	 * "outward" from the arc in the direction of travel for the END vertex.
	 * At the START vertex, the arc enters from the opposite direction, so
	 * we flip it to obtain the tangent pointing into (rather than out of)
	 * the arc.  No flip is needed for the end vertex. */
	if (vert_idx == cs->start) {
	    *tx_out = -*tx_out; *ty_out = -*ty_out;
	}
	return BRLCAD_OK;
    }

    bu_vls_printf(log_str,
	    "sketch_tangent_at_vertex: unsupported segment type (magic 0x%08x)\n",
	    magic);
    return BRLCAD_ERROR;
}

static int
ecmd_sketch_set_tangency(struct rt_edit *s)
{
    struct rt_sketch_edit *se = (struct rt_sketch_edit *)s->ipe_ptr;
    struct rt_sketch_internal *skt =
	(struct rt_sketch_internal *)s->es_int.idb_ptr;
    RT_SKETCH_CK_MAGIC(skt);

    if (se->curr_seg < 0) {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_SKETCH_SET_TANGENCY: no segment selected\n");
	return BRLCAD_ERROR;
    }

    if (!s->e_inpara || s->e_inpara < 2) {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_SKETCH_SET_TANGENCY: "
		"e_para[0]=adj_seg, e_para[1]=angle required\n");
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }

    void *arc_seg = skt->curve.segment[se->curr_seg];
    if (!arc_seg || *(uint32_t *)arc_seg != CURVE_CARC_MAGIC) {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_SKETCH_SET_TANGENCY: "
		"selected segment is not a CARC\n");
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }
    struct carc_seg *cs = (struct carc_seg *)arc_seg;

    if (cs->radius < 0.0) {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_SKETCH_SET_TANGENCY: "
		"cannot set tangency on a full-circle arc\n");
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }

    int adj_seg = (int)s->e_para[0];
    fastf_t angle_rad = s->e_para[1];

    if (adj_seg < 0 || (size_t)adj_seg >= skt->curve.count) {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_SKETCH_SET_TANGENCY: "
		"adjacent segment index %d out of range [0,%zu)\n",
		adj_seg, skt->curve.count);
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }
    if (adj_seg == se->curr_seg) {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_SKETCH_SET_TANGENCY: "
		"adjacent segment is the same as curr_seg\n");
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }

    /* Find the shared vertex between the two segments */
    int v0[2], v1[2], nv0, nv1;
    {
	void *adj = skt->curve.segment[adj_seg];
	uint32_t am = adj ? *(uint32_t *)adj : 0;
	if (am == CURVE_LSEG_MAGIC) {
	    struct line_seg *ls = (struct line_seg *)adj;
	    v1[0] = ls->start; v1[1] = ls->end; nv1 = 2;
	} else if (am == CURVE_CARC_MAGIC) {
	    struct carc_seg *acs = (struct carc_seg *)adj;
	    v1[0] = acs->start; v1[1] = acs->end; nv1 = 2;
	} else {
	    bu_vls_printf(s->log_str,
		    "ERROR: ECMD_SKETCH_SET_TANGENCY: "
		    "adjacent segment type unsupported\n");
	    s->e_inpara = 0;
	    return BRLCAD_ERROR;
	}
	v0[0] = cs->start; v0[1] = cs->end; nv0 = 2;
    }

    int common_vertex = -1;
    for (int i = 0; i < nv0 && common_vertex < 0; i++) {
	for (int j = 0; j < nv1 && common_vertex < 0; j++) {
	    if (v0[i] == v1[j])
		common_vertex = v0[i];
	}
    }
    if (common_vertex < 0) {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_SKETCH_SET_TANGENCY: "
		"segments do not share a vertex\n");
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }

    /* Tangent direction of the adjacent segment at the common vertex */
    fastf_t tx = 0.0, ty = 0.0;
    if (sketch_tangent_at_vertex(skt, adj_seg, common_vertex,
				  &tx, &ty, s->log_str) != BRLCAD_OK) {
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }

    /* Optionally rotate by tangency_angle */
    if (fabs(angle_rad) > 1.0e-10) {
	fastf_t ca = cos(angle_rad), sa = sin(angle_rad);
	fastf_t ntx =  tx * ca - ty * sa;
	fastf_t nty =  tx * sa + ty * ca;
	tx = ntx; ty = nty;
    }

    /* Convert tangent → radius direction (rotate -90°) */
    fastf_t rdx = -ty, rdy = tx;

    /* Compute the chord (v2 - v1) for the arc's two vertices */
    fastf_t ax = skt->verts[cs->start][0], ay = skt->verts[cs->start][1];
    fastf_t bx = skt->verts[cs->end  ][0], by = skt->verts[cs->end  ][1];
    fastf_t diffx = ax - bx, diffy = ay - by;

    fastf_t dot = diffx * rdx + diffy * rdy;
    if (fabs(dot) < 1.0e-10) {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_SKETCH_SET_TANGENCY: "
		"impossible tangency (perpendicular to chord — would give a straight line)\n");
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }

    fastf_t diff_sq = diffx*diffx + diffy*diffy;
    fastf_t new_r = diff_sq / (-2.0 * dot);
    if (new_r < 0.0) {
	new_r = -new_r;
	rdx = -rdx; rdy = -rdy;
    }

    /* Compute candidate centres on both sides and pick the closer one */
    fastf_t vcomx = skt->verts[common_vertex][0];
    fastf_t vcomy = skt->verts[common_vertex][1];
    fastf_t cax = vcomx + new_r * rdx, cay = vcomy + new_r * rdy;
    fastf_t cbx = vcomx - new_r * rdx, cby = vcomy - new_r * rdy;

    int other_v = (common_vertex == cs->start) ? cs->end : cs->start;
    fastf_t ovx = skt->verts[other_v][0], ovy = skt->verts[other_v][1];

    fastf_t da = fabs(sqrt((cax - ovx)*(cax - ovx) + (cay - ovy)*(cay - ovy)) - new_r);
    fastf_t db = fabs(sqrt((cbx - ovx)*(cbx - ovx) + (cby - ovy)*(cby - ovy)) - new_r);

    fastf_t cx = (da < db) ? cax : cbx;
    fastf_t cy = (da < db) ? cay : cby;

    /* Determine center_is_left and orientation from centre position */
    fastf_t diff2x = cx - bx, diff2y = cy - by;
    fastf_t cross_cl = diff2x * rdy - diff2y * rdx;
    cs->center_is_left = (cross_cl > 0.0) ? 1 : 0;

    fastf_t diff1x = ax - cx, diff1y = ay - cy;
    /* A point one unit along the tangent from the start vertex (ax, ay).
     * Used to determine orientation (CW vs CCW) via a 2-D cross product:
     * if the cross product of (start→centre) with (start→tangent_point) is
     * positive the arc turns counter-clockwise (orientation = 0). */
    fastf_t tangent_point_x = ax + tx, tangent_point_y = ay + ty;
    fastf_t diff3x = tangent_point_x - cx, diff3y = tangent_point_y - cy;
    fastf_t cross_or = diff1x * diff3y - diff1y * diff3x;
    cs->orientation = (cross_or > 0.0) ? 0 : 1;

    cs->radius = new_r;
    cs->center = -1;  /* force recompute */

    s->e_inpara = 0;
    return BRLCAD_OK;
}

static int
ecmd_sketch_set_plane(struct rt_edit *s)
{
    struct rt_sketch_internal *skt =
	(struct rt_sketch_internal *)s->es_int.idb_ptr;
    RT_SKETCH_CK_MAGIC(skt);

    if (!s->e_inpara || s->e_inpara < 9) {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_SKETCH_SET_PLANE: "
		"9 parameters required (V[3] A[3] B[3] in base units)\n");
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }

    VSET(skt->V, s->e_para[0], s->e_para[1], s->e_para[2]);

    vect_t a, b;
    VSET(a, s->e_para[3], s->e_para[4], s->e_para[5]);
    VSET(b, s->e_para[6], s->e_para[7], s->e_para[8]);

    fastf_t a_len = MAGNITUDE(a);
    if (a_len < SMALL_FASTF) {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_SKETCH_SET_PLANE: u_vec (A) has zero length\n");
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }
    VSCALE(a, a, 1.0 / a_len);

    /* Gram-Schmidt: orthogonalise B against A */
    fastf_t bdota = VDOT(b, a);
    VJOIN1(b, b, -bdota, a);
    fastf_t b_len = MAGNITUDE(b);
    if (b_len < SMALL_FASTF) {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_SKETCH_SET_PLANE: v_vec (B) is parallel to u_vec (A)\n");
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }
    VSCALE(b, b, 1.0 / b_len);

    VMOVE(skt->u_vec, a);
    VMOVE(skt->v_vec, b);

    s->e_inpara = 0;
    return BRLCAD_OK;
}

static int
ecmd_sketch_toggle_segment_reverse(struct rt_edit *s)
{
    struct rt_sketch_edit *se = (struct rt_sketch_edit *)s->ipe_ptr;
    struct rt_sketch_internal *skt =
	(struct rt_sketch_internal *)s->es_int.idb_ptr;
    RT_SKETCH_CK_MAGIC(skt);

    if (se->curr_seg < 0 || (size_t)se->curr_seg >= skt->curve.count) {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_SKETCH_TOGGLE_SEGMENT_REVERSE: "
		"no segment selected\n");
	return BRLCAD_ERROR;
    }

    /* Allocate the reverse array if it does not yet exist */
    if (!skt->curve.reverse) {
	skt->curve.reverse = (int *)bu_calloc(skt->curve.count,
					      sizeof(int),
					      "sketch reverse array");
    }

    skt->curve.reverse[se->curr_seg] ^= 1;

    return BRLCAD_OK;
}

int
rt_edit_sketch_edit(struct rt_edit *s)
{
    switch (s->edit_flag) {
	case RT_PARAMS_EDIT_SCALE:
	    return edit_sscale(s);
	case RT_PARAMS_EDIT_TRANS:
	    edit_stra(s);
	    break;
	case RT_PARAMS_EDIT_ROT:
	    edit_srot(s);
	    break;
	case ECMD_SKETCH_PICK_VERTEX:
	    return ecmd_sketch_pick_vertex(s);
	case ECMD_SKETCH_MOVE_VERTEX:
	    return ecmd_sketch_move_vertex(s);
	case ECMD_SKETCH_MOVE_VERTEX_LIST:
	    return ecmd_sketch_move_vertex_list(s);
	case ECMD_SKETCH_PICK_SEGMENT:
	    return ecmd_sketch_pick_segment(s);
	case ECMD_SKETCH_MOVE_SEGMENT:
	    return ecmd_sketch_move_segment(s);
	case ECMD_SKETCH_APPEND_LINE:
	    return ecmd_sketch_append_line(s);
	case ECMD_SKETCH_APPEND_ARC:
	    return ecmd_sketch_append_arc(s);
	case ECMD_SKETCH_APPEND_BEZIER:
	    return ecmd_sketch_append_bezier(s);
	case ECMD_SKETCH_DELETE_VERTEX:
	    return ecmd_sketch_delete_vertex(s);
	case ECMD_SKETCH_DELETE_SEGMENT:
	    return ecmd_sketch_delete_segment(s);
	case ECMD_SKETCH_SPLIT_SEGMENT:
	    return ecmd_sketch_split_segment(s);
	case ECMD_SKETCH_APPEND_NURB:
	    return ecmd_sketch_append_nurb(s);
	case ECMD_SKETCH_NURB_EDIT_KV:
	    return ecmd_sketch_nurb_edit_kv(s);
	case ECMD_SKETCH_NURB_EDIT_WEIGHTS:
	    return ecmd_sketch_nurb_edit_weights(s);
	case ECMD_SKETCH_ADD_VERTEX:
	    return ecmd_sketch_add_vertex(s);
	case ECMD_SKETCH_TOGGLE_ARC_ORIENT:
	    return ecmd_sketch_toggle_arc_orient(s);
	case ECMD_SKETCH_SET_ARC_RADIUS:
	    return ecmd_sketch_set_arc_radius(s);
	case ECMD_SKETCH_SET_TANGENCY:
	    return ecmd_sketch_set_tangency(s);
	case ECMD_SKETCH_SET_PLANE:
	    return ecmd_sketch_set_plane(s);
	case ECMD_SKETCH_TOGGLE_SEGMENT_REVERSE:
	    return ecmd_sketch_toggle_segment_reverse(s);
	default:
	    return edit_generic(s);
    }

    return 0;
}

int
rt_edit_sketch_edit_xy(struct rt_edit *s, const vect_t mousevec)
{
    struct rt_sketch_edit *se = (struct rt_sketch_edit *)s->ipe_ptr;
    vect_t pos_view = VINIT_ZERO;

    switch (s->edit_flag) {
	case RT_PARAMS_EDIT_SCALE:
	    edit_sscale_xy(s, mousevec);
	    return 0;
	case RT_PARAMS_EDIT_TRANS:
	    edit_stra_xy(&pos_view, s, mousevec);
	    break;
	case ECMD_SKETCH_PICK_VERTEX:
	    /* Store view-space cursor position; ft_edit will call the
	     * proximity search when e_inpara == 0 and v_pos_valid is set. */
	    VSET(se->v_pos, mousevec[X], mousevec[Y], 0.0);
	    se->v_pos_valid = 1;
	    return 0;
	default:
	    return edit_generic_xy(s, mousevec);
    }

    edit_abs_tra(s, pos_view);
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
