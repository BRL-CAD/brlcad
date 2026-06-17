/*                         S N A P . C
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
/** @file libbv/snap.c
 *
 * Logic for snapping points to visual elements.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>

#include "bu/opt.h"
#include "bu/ptbl.h"
#include "bu/vls.h"
#include "bn/tol.h"
#include "bg/lseg.h"
#include "bsg/defines.h"
#include "bsg/feature.h"
#include "bsg/geometry.h"
#include "bsg/snap.h"
#include "bsg/util.h"
#include "bsg/view_state.h"
#include "bsg/scene_object.h"
#include "feature_private.h"
#include "field_private.h"
#include "view_state_private.h"
#include "node_private.h"
#include "payload_private.h"
#include "payload_typed_private.h"

struct bsg_cp_info {
    double ctol_sq; // square of the distance that defines "close to a line"
    point_t cp;  // closest point on closest line
    double dsq;  // squared distance to closest line
    point_t cp2;  // closest point on closest line
    double dsq2; // squared distance to 2nd closest line
};
#define BSG_CP_INFO_INIT {BN_TOL_DIST, VINIT_ZERO, DBL_MAX, VINIT_ZERO, DBL_MAX}

/* Tcl data lines and screen data lines are stored as view-scoped line
 * features and snapped through the same source-record path used for other
 * view-scoped line features. */

static int
_find_closest_segment(struct bsg_cp_info *s, point_t *p, const point_t a, const point_t b)
{
    point_t c;
    double dsq = bg_distsq_lseg3_pt(&c, a, b, *p);
    if (dsq > s->ctol_sq)
	return 0;
    if (s->dsq > dsq) {
	VMOVE(s->cp2, s->cp);
	s->dsq2 = s->dsq;
	VMOVE(s->cp, c);
	s->dsq = dsq;
	return 1;
    }
    if (s->dsq2 > dsq) {
	VMOVE(s->cp2, c);
	s->dsq2 = dsq;
	return 2;
    }
    return 0;
}

static struct bsg_payload_line_set *
_node_payload_line_set(struct bsg_node *node)
{
    struct bsg_payload *payload = bsg_node_get_payload(node);
    if (!payload)
	return NULL;
    if (payload->pl_type == BSG_PL_LINE_SET)
	return bsg_payload_line_set_get(payload);
    if (payload->pl_type == BSG_PL_SKETCH) {
	struct bsg_payload *geometry = bsg_payload_sketch_geometry(payload);
	return bsg_payload_line_set_get(geometry);
    }
    return NULL;
}

static int
_find_closest_line_set_point(struct bsg_cp_info *s,
			     point_t *p,
			     const struct bsg_payload_line_set *ls)
{
    int ret = 0;
    point_t last = VINIT_ZERO;
    int have_last = 0;

    if (!s || !p || !ls || !ls->points)
	return 0;

    for (size_t i = 0; i < ls->point_cnt; i++) {
	int cmd = ls->cmds ? ls->cmds[i] :
	    ((i % 2) ? BSG_GEOMETRY_LINE_DRAW : BSG_GEOMETRY_LINE_MOVE);
	if (cmd == BSG_GEOMETRY_LINE_MOVE) {
	    VMOVE(last, ls->points[i]);
	    have_last = 1;
	    continue;
	}
	if (cmd == BSG_GEOMETRY_LINE_DRAW) {
	    if (have_last) {
		int seg_ret = _find_closest_segment(s, p, last, ls->points[i]);
		if (seg_ret > ret)
		    ret = seg_ret;
	    }
	    VMOVE(last, ls->points[i]);
	    have_last = 1;
	}
    }

    return ret;
}

static int
_find_closest_obj_point(struct bsg_cp_info *s, point_t *p, struct bsg_node *o)
{
    int ret = 0;
    if (!s || !p || !o)
	return 0;

    int kind = BSG_GEOMETRY_NODE_NONE;
    if (bsg_field_get_enum(bsg_node_field_ref(o, BSG_FIELD_GEOMETRY_KIND), &kind) &&
	    (kind == BSG_GEOMETRY_NODE_LINE_SET ||
	     kind == BSG_GEOMETRY_NODE_INDEXED_LINE_SET)) {
	bsg_field_ref coords = bsg_node_field_ref(o, BSG_FIELD_GEOMETRY_COORDINATES);
	size_t point_count = bsg_field_multi_count(coords);
	if (kind == BSG_GEOMETRY_NODE_INDEXED_LINE_SET) {
	    bsg_field_ref indices = bsg_node_field_ref(o, BSG_FIELD_GEOMETRY_INDICES);
	    size_t index_count = bsg_field_multi_count(indices);
	    point_t last = VINIT_ZERO;
	    point_t cur = VINIT_ZERO;
	    int have_last = 0;
	    for (size_t i = 0; i < index_count; i++) {
		int idx = -1;
		if (!bsg_field_multi_int_at(indices, i, &idx))
		    continue;
		if (idx < 0) {
		    have_last = 0;
		    continue;
		}
		if ((size_t)idx >= point_count)
		    continue;
		if (!bsg_field_multi_point_at(coords, (size_t)idx, cur))
		    continue;
		if (have_last) {
		    int seg_ret = _find_closest_segment(s, p, last, cur);
		    if (seg_ret > ret)
			ret = seg_ret;
		}
		VMOVE(last, cur);
		have_last = 1;
	    }
	    return ret;
	}

	bsg_field_ref cmds = bsg_node_field_ref(o, BSG_FIELD_GEOMETRY_PRIMITIVE_SETS);
	point_t last = VINIT_ZERO;
	point_t cur = VINIT_ZERO;
	int have_last = 0;
	for (size_t i = 0; i < point_count; i++) {
	    int cmd = (i % 2) ? BSG_GEOMETRY_LINE_DRAW : BSG_GEOMETRY_LINE_MOVE;
	    if (!bsg_field_multi_point_at(coords, i, cur))
		continue;
	    (void)bsg_field_multi_int_at(cmds, i, &cmd);
	    switch (cmd) {
		case BSG_GEOMETRY_LINE_MOVE:
		    VMOVE(last, cur);
		    have_last = 1;
		    break;
		case BSG_GEOMETRY_LINE_DRAW:
		    if (have_last) {
			int seg_ret = _find_closest_segment(s, p, last, cur);
			if (seg_ret > ret)
			    ret = seg_ret;
		    }
		    VMOVE(last, cur);
		    have_last = 1;
		    break;
		default:
		    break;
	    }
	}
	return ret;
    }

    struct bsg_payload_line_set *ls = _node_payload_line_set(o);
    if (ls && ls->point_cnt)
	return _find_closest_line_set_point(s, p, ls);

    return ret;
}

static double
line_tol_sq(struct bsg_view *v, int lwidth)
{
    if (!v || lwidth <= 0)
	return 100*100;

    // NOTE - make sure calling applications update these values from
    // the display manager info before command execution.
    int width = v->gv_width;
    int height = v->gv_height;

    if (!width || !height)
	return 100*100;

    double lavg = ((double)width + (double)height) * 0.5;
    double lratio = ((double)lwidth)/lavg;

    double lrsize = v->gv_size * lratio * bsg_view_snap_tolerance_factor(v);

    return lrsize*lrsize;
}

/* Context for database-source snap traversal. */
struct _bsg_snap_db_ctx {
    struct bsg_cp_info *s;
    struct bsg_view *v;
    bsg_feature_ref exclude_feature;
    point_t *p;
    int *ret;
};

static int
_bsg_snap_feature_excluded(const struct _bsg_snap_db_ctx *ctx, struct bsg_node *so)
{
    if (!ctx || !so || bsg_feature_ref_is_null(ctx->exclude_feature))
	return 0;

    bsg_feature_ref ref = bsg_feature_ref_from_node(so, BSG_FEATURE_UNKNOWN);
    return (ref.token && ref.token == ctx->exclude_feature.token);
}

static int
_bsg_snap_db_obj_cb(struct bsg_node *so, void *data)
{
    struct _bsg_snap_db_ctx *ctx = (struct _bsg_snap_db_ctx *)data;
    if (_bsg_snap_feature_excluded(ctx, so))
	return 1;
    *ctx->ret += _find_closest_obj_point(ctx->s, ctx->p, so);
    return 1;
}

/* Callback for snap_lines feature-scope iteration. */
static int
_bsg_snap_view_obj_cb(struct bsg_node *so, void *data)
{
    struct _bsg_snap_db_ctx *ctx = (struct _bsg_snap_db_ctx *)data;
    if (_bsg_snap_feature_excluded(ctx, so))
	return 1;
    *ctx->ret += _find_closest_obj_point(ctx->s, ctx->p, so);
    return 1;
}

int
bsg_snap_lines_3d(point_t *out_pt, struct bsg_view *v, point_t *p)
{
    int ret = 0;
    struct bsg_cp_info cpinfo = BSG_CP_INFO_INIT;

    if (!p || !v) return 0;

    int snap_source_flags = bsg_view_snap_source_flags(v);
    bsg_feature_ref exclude_feature = BSG_FEATURE_REF_NULL_INIT;
    (void)bsg_view_snap_exclude_feature_get(v, &exclude_feature);

    // If we're not in Tcl mode only, we are looking at objects - either
    // all of them, or a specified subset
    if (snap_source_flags != BSG_SNAP_TCL) {
	struct bsg_cp_info *s = &cpinfo;
	s->ctol_sq = line_tol_sq(v, 1);
	if (!snap_source_flags || (snap_source_flags & BSG_SNAP_DB)) {
		/* Traverse retained database sources when available; otherwise
		 * the helper uses the view's source tables. */
		struct _bsg_snap_db_ctx snap_ctx;
		snap_ctx.s = s;
		snap_ctx.v = v;
		snap_ctx.exclude_feature = exclude_feature;
		snap_ctx.p = p;
		snap_ctx.ret = &ret;
		bsg_scene_visit_db(v, _bsg_snap_db_obj_cb, &snap_ctx);
	}
	if (!snap_source_flags || (snap_source_flags & BSG_SNAP_VIEW)) {
		int scope_mask = 0;
		if (!snap_source_flags || (snap_source_flags & BSG_SNAP_SHARED))
		    scope_mask |= BSG_FEATURE_SCOPE_SHARED;
		if (!snap_source_flags || (snap_source_flags & BSG_SNAP_LOCAL))
		    scope_mask |= BSG_FEATURE_SCOPE_LOCAL;
		if (scope_mask) {
		    struct _bsg_snap_db_ctx snap_ctx;
		    snap_ctx.s = s;
		    snap_ctx.v = v;
		    snap_ctx.exclude_feature = exclude_feature;
		    snap_ctx.p = p;
		    snap_ctx.ret = &ret;
		    bsg_feature_visit_nodes(v, scope_mask, _bsg_snap_view_obj_cb, &snap_ctx);
		}
	}
    }

    // There are some issues with line snapping that don't come up with grid
    // snapping - in particular, when are we "close enough" to a line to snap,
    // and how do we handle snapping when close enough to multiple lines?  We
    // probably want to prefer intersections between lines to closest line
    // point if we are close to multiple lines...
    //
    // Tcl data_lines / sdata_lines snap state is mirrored into BSG view-scope objects
    // (`_tcl_data_lines`, `_tcl_sdata_lines`), which the BSG_SNAP_VIEW
    // branch above already snaps against via the feature-scope walk.  The
    // BSG_SNAP_TCL flag is now equivalent to BSG_SNAP_VIEW and is retained
    // only for caller backward-compatibility.
    if (snap_source_flags == BSG_SNAP_TCL) {
	int scope_mask = BSG_FEATURE_SCOPE_ALL;
	struct _bsg_snap_db_ctx snap_ctx;
	snap_ctx.s = &cpinfo;
	snap_ctx.v = v;
	snap_ctx.exclude_feature = exclude_feature;
	snap_ctx.p = p;
	snap_ctx.ret = &ret;
	bsg_feature_visit_nodes(v, scope_mask, _bsg_snap_view_obj_cb, &snap_ctx);
    }

    // If we found something, we can snap
    if (ret) {
	VMOVE(*out_pt, cpinfo.cp);
	return 1;
    }

    return 0;
}

int
bsg_snap_lines_2d(struct bsg_view *v, fastf_t *vx, fastf_t *vy)
{
    if (!v || !vx || !vy) return 0;

    point2d_t p2d = {0.0, 0.0};
    V2SET(p2d, *vx, *vy);
    point_t vp = VINIT_ZERO;
    VSET(vp, p2d[0], p2d[1], 0);
    point_t p = VINIT_ZERO;
    MAT4X3PNT(p, v->gv_view2model, vp);
    point_t out_pt = VINIT_ZERO;
    if (bsg_snap_lines_3d(&out_pt, v, &p) == 1) {
	MAT4X3PNT(vp, v->gv_model2view, out_pt);
	(*vx) = vp[0];
	(*vy) = vp[1];
	return 1;
    }

    return 0;
}

void
bsg_view_center_linesnap(struct bsg_view *v)
{
    point_t view_pt;
    point_t model_pt;

    MAT_DELTAS_GET_NEG(model_pt, v->gv_center);
    MAT4X3PNT(view_pt, v->gv_model2view, model_pt);
    bsg_snap_lines_2d(v, &view_pt[X], &view_pt[Y]);
    MAT4X3PNT(model_pt, v->gv_view2model, view_pt);
    MAT_DELTAS_VEC_NEG(v->gv_center, model_pt);
    bsg_update(v);
}

int
bsg_snap_grid_2d(struct bsg_view *v, fastf_t *vx, fastf_t *vy)
{
    point_t view_pt;
    point_t grid_origin;
    fastf_t inv_grid_res_h, inv_grid_res_v;

    if (!v || !vx || !vy)
	return 0;

    struct bsg_grid_state *grid = bsg_view_grid(v);
    if (!grid)
	return 0;

    if (ZERO(grid->res_h) ||
	ZERO(grid->res_v))
	return 0;

    inv_grid_res_h = 1/(grid->res_h * v->gv_base2local);
    inv_grid_res_v = 1/(grid->res_v * v->gv_base2local);

    VSET(view_pt, *vx, *vy, 0.0);
    VSCALE(view_pt, view_pt, v->gv_scale);
    MAT4X3PNT(grid_origin, v->gv_model2view, grid->anchor);
    VSCALE(grid_origin, grid_origin, v->gv_scale);

    fastf_t grid_units_h = (view_pt[X] - grid_origin[X]) * inv_grid_res_h;
    fastf_t grid_units_v = (view_pt[Y] - grid_origin[Y]) * inv_grid_res_v;
    int nh, nv;		/* whole grid units */
    nh = floor(view_pt[X]);
    nv = floor(view_pt[Y]);
    grid_units_h -= nh;		/* now contains only the fraction part */
    grid_units_v -= nv;		/* now contains only the fraction part */
    int hstep = round(grid_units_h);
    int vstep = round(grid_units_v);
    fastf_t fnh = nh + hstep + grid_origin[X];
    fastf_t fnv = nv + vstep + grid_origin[Y];
    VSET(view_pt, fnh * grid->res_h * v->gv_base2local, fnv * grid->res_v * v->gv_base2local, 0.0);
    VSCALE(view_pt, view_pt, 1.0/v->gv_scale);

    *vx = view_pt[X];
    *vy = view_pt[Y];

    return 1;
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
