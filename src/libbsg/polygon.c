/*                     P O L Y G O N  . C
 * BRL-CAD
 *
 * Copyright (c) 2020-2026 United States Government as represented by
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
/** @file polygons.c
 *
 * Utility functions for working with polygons in a view context.
 *
 */

#include "common.h"
#include <string.h>
#include "vmath.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bn/mat.h"
#include "bn/tol.h"
#include "bsg/draw_source.h"
#include "bsg/feature.h"
#include "bsg/field.h"
#include "bsg/pick.h"
#include "bsg/util.h"
#include "bsg/node.h"
#include "bsg/scene_object.h"
#include "bg/lseg.h"
#include "bg/plane.h"
#include "bg/polygon.h"
#include "bsg/defines.h"
#include "bsg/identity.h"
#include "bsg/polygon.h"
#include "payload_typed_private.h"
#include "bsg/snap.h"
#include "appearance_private.h"
#include "draw_source_private.h"
#include "field_private.h"
#include "feature_private.h"
#include "geometry_private.h"
#include "identity_private.h"
#include "node_private.h"
#include "node_storage_private.h"
#include "object_private.h"
#include "payload_private.h"
#include "polygon_private.h"
#include "scene_object_private.h"

struct bsg_polygon *
bsg_node_polygon(const struct bsg_node *node)
{
    if (!node)
	return NULL;
    return bsg_payload_polygon_get(bsg_node_get_payload(node));
}

static bsg_polygon_ref
_bsg_polygon_ref(struct bsg_node *node)
{
    if (!bsg_node_polygon(node))
	return (bsg_polygon_ref)BSG_POLYGON_REF_NULL_INIT;
    return bsg_feature_ref_from_node(node, BSG_FEATURE_POLYGON);
}

int
bsg_polygon_ref_is_null(bsg_polygon_ref ref)
{
    return bsg_feature_ref_is_null(ref);
}

static struct bsg_node *
_bsg_polygon_node(bsg_polygon_ref ref)
{
    struct bsg_node *node = bsg_feature_node(ref);
    return bsg_node_polygon(node) ? node : NULL;
}

const struct bsg_polygon *
bsg_polygon_data(bsg_polygon_ref ref)
{
    return bsg_node_polygon(_bsg_polygon_node(ref));
}

int
bsg_polygon_record_get(bsg_polygon_ref ref, struct bsg_polygon_record *record)
{
    struct bsg_node *node = _bsg_polygon_node(ref);
    struct bsg_polygon *p = bsg_node_polygon(node);
    if (!node || !p || !record)
	return 0;

    memset(record, 0, sizeof(*record));
    record->ref = _bsg_polygon_ref(node);
    record->name = bsg_node_name(node);
    record->type = p->type;
    record->fill_flag = p->fill_flag;
    V2MOVE(record->fill_dir, p->fill_dir);
    record->fill_delta = p->fill_delta;
    BU_COLOR_CPY(&record->fill_color, &p->fill_color);
    bsg_node_get_color(node, &record->edge_color[0], &record->edge_color[1], &record->edge_color[2]);
    record->curr_contour_i = p->curr_contour_i;
    record->curr_point_i = p->curr_point_i;
    record->contour_count = p->polygon.num_contours;
    record->first_contour_open = (p->polygon.contour && p->polygon.num_contours) ? p->polygon.contour[0].open : 0;
    for (size_t i = 0; i < p->polygon.num_contours; i++)
	record->point_count += p->polygon.contour[i].num_points;
    VMOVE(record->origin_point, p->origin_point);
    HMOVE(record->vp, p->vp);
    record->vZ = p->vZ;
    record->user_data = p->u_data;
    return 1;
}

struct bsg_polygon_line_builder {
    point_t *points;
    int *commands;
    size_t count;
    size_t capacity;
};

static void
_bsg_polygon_line_builder_free(struct bsg_polygon_line_builder *builder)
{
    if (!builder)
	return;
    if (builder->points)
	bu_free(builder->points, "bsg polygon line points");
    if (builder->commands)
	bu_free(builder->commands, "bsg polygon line commands");
    memset(builder, 0, sizeof(*builder));
}

static int
_bsg_polygon_line_builder_append(struct bsg_polygon_line_builder *builder,
				 const point_t point,
				 int command)
{
    if (!builder || !point)
	return 0;

    if (builder->count >= builder->capacity) {
	size_t new_capacity = builder->capacity ? builder->capacity * 2 : 16;
	builder->points = (point_t *)bu_realloc(builder->points,
		new_capacity * sizeof(point_t), "bsg polygon line points");
	builder->commands = (int *)bu_realloc(builder->commands,
		new_capacity * sizeof(int), "bsg polygon line commands");
	builder->capacity = new_capacity;
    }

    VMOVE(builder->points[builder->count], point);
    builder->commands[builder->count] = command;
    builder->count++;
    return 1;
}

static int
_bsg_polygon_line_builder_apply(struct bsg_polygon_line_builder *builder,
				struct bsg_node *node)
{
    if (!node || !builder)
	return 0;

    if (!bsg_geometry_node_set_line_set_fields(node,
		(const point_t *)builder->points, builder->commands, builder->count))
	return 0;
    return 1;
}

static int
bsg_polygon_contour(struct bsg_polygon_line_builder *builder,
		    struct bg_poly_contour *c,
		    int curr_c,
		    int curr_i,
		    int do_pnt)
{
    if (!builder || !c)
	return 0;

    if (do_pnt) {
	return _bsg_polygon_line_builder_append(builder, c->point[0], BSG_GEOMETRY_POINT_DRAW);
    }

    if (!_bsg_polygon_line_builder_append(builder, c->point[0], BSG_GEOMETRY_LINE_MOVE))
	return 0;
    for (size_t i = 0; i < c->num_points; i++) {
	if (!_bsg_polygon_line_builder_append(builder, c->point[i], BSG_GEOMETRY_LINE_DRAW))
	    return 0;
    }
    if (!c->open) {
	if (!_bsg_polygon_line_builder_append(builder, c->point[0], BSG_GEOMETRY_LINE_DRAW))
	    return 0;
    }

    if (curr_c && curr_i >= 0) {
	if (!_bsg_polygon_line_builder_append(builder, c->point[curr_i], BSG_GEOMETRY_LINE_MOVE))
	    return 0;
	if (!_bsg_polygon_line_builder_append(builder, c->point[curr_i], BSG_GEOMETRY_POINT_DRAW))
	    return 0;
    }

    return 1;
}

void
bsg_fill_polygon(struct bsg_node *s)
{
    if (!s)
	return;

    // free old fill, if present
    struct bsg_node *fobj = bsg_scene_child_find(s, "*fill*");
    if (fobj)
	bsg_scene_node_release(fobj);

    struct bsg_polygon *p = bsg_node_polygon(s);

    if (!p || !p->polygon.num_contours)
	return;

    if (!p->polygon.contour || p->polygon.contour[0].open)
	return;

    if (p->fill_delta < BN_TOL_DIST)
	return;

    struct bg_polygon *fill = bsg_polygon_fill_segments(&p->polygon, &p->vp, p->fill_dir, p->fill_delta);
    if (!fill)
	return;

    // Got fill, create lines
    fobj = bsg_scene_node_create_child(s);
    bsg_node_set_name(fobj, ":fill");
    bsg_appearance_set_line_width(fobj, 1);
    bsg_appearance_set_line_style(fobj, 0);
    {
	unsigned char color[3] = {0, 0, 0};
	bu_color_to_rgb_chars(&p->fill_color, color);
	bsg_node_set_color(fobj, color[0], color[1], color[2]);
    }
    struct bsg_polygon_line_builder builder;
    memset(&builder, 0, sizeof(builder));
    for (size_t i = 0; i < fill->num_contours; i++) {
	if (!bsg_polygon_contour(&builder, &fill->contour[i], 0, -1, 0))
	    break;
    }
    (void)_bsg_polygon_line_builder_apply(&builder, fobj);
    _bsg_polygon_line_builder_free(&builder);
}

static void
_bsg_polygon_realize_lines(struct bsg_node *s)
{
    if (!s)
	return;

    struct bsg_polygon *p = bsg_node_polygon(s);
    if (!p)
	return;
    int type = p->type;
    struct bsg_polygon_line_builder builder;
    memset(&builder, 0, sizeof(builder));

    // Clear any old holes
    for (size_t i = 0; i < BU_PTBL_LEN(&s->children); i++) {
	struct bsg_node *s_c = (struct bsg_node *)BU_PTBL_GET(&s->children, i);
	bsg_scene_node_release(s_c);
    }

    for (size_t i = 0; i < p->polygon.num_contours; ++i) {
	/* Draw holes as child line nodes so their dashed style can differ from
	 * the outer polygon contour. */
	size_t pcnt = p->polygon.contour[i].num_points;
	int do_pnt = 0;
	if (pcnt == 1)
	    do_pnt = 1;
	if (type == BSG_POLYGON_CIRCLE && pcnt == 3)
	    do_pnt = 1;
	if (type == BSG_POLYGON_ELLIPSE && pcnt == 4)
	    do_pnt = 1;
	if (type == BSG_POLYGON_RECTANGLE) {
	    if (NEAR_ZERO(DIST_PNT_PNT_SQ(p->polygon.contour[0].point[0], p->polygon.contour[0].point[1]), SMALL_FASTF) &&
		    NEAR_ZERO(DIST_PNT_PNT_SQ(p->polygon.contour[0].point[0], p->polygon.contour[0].point[2]), SMALL_FASTF))
		do_pnt = 1;
	}
	if (type == BSG_POLYGON_SQUARE) {
	    if (NEAR_ZERO(DIST_PNT_PNT_SQ(p->polygon.contour[0].point[0], p->polygon.contour[0].point[1]), SMALL_FASTF) &&
		    NEAR_ZERO(DIST_PNT_PNT_SQ(p->polygon.contour[0].point[0], p->polygon.contour[0].point[2]), SMALL_FASTF))
		do_pnt = 1;
	}

	if (p->polygon.hole[i]) {
	    struct bsg_node *s_c = bsg_scene_node_create_child(s);
	    unsigned char color[3] = {0, 0, 0};
	    bsg_appearance_set_line_style(s_c, 1);
	    bsg_node_get_color(s, &color[0], &color[1], &color[2]);
	    bsg_node_set_color(s_c, color[0], color[1], color[2]);
	    bsg_node_set_view(s_c, bsg_node_get_view(s));
	    struct bsg_polygon_line_builder hole_builder;
	    memset(&hole_builder, 0, sizeof(hole_builder));
	    (void)bsg_polygon_contour(&hole_builder, &p->polygon.contour[i], ((int)i == p->curr_contour_i), p->curr_point_i, do_pnt);
	    (void)_bsg_polygon_line_builder_apply(&hole_builder, s_c);
	    _bsg_polygon_line_builder_free(&hole_builder);
	    continue;
	}

	(void)bsg_polygon_contour(&builder, &p->polygon.contour[i], ((int)i == p->curr_contour_i), p->curr_point_i, do_pnt);
    }

    (void)_bsg_polygon_line_builder_apply(&builder, s);
    _bsg_polygon_line_builder_free(&builder);

    if (p->fill_flag) {
	bsg_fill_polygon(s);
    } else {
	struct bsg_node *fobj = bsg_scene_child_find(s, "*fill*");
	if (fobj)
	    bsg_scene_node_release(fobj);

    }
}

int
bsg_polygon_realize(struct bsg_node *node)
{
    if (!bsg_node_polygon(node))
	return 0;

    _bsg_polygon_realize_lines(node);
    struct bsg_payload *pl = bsg_node_get_payload(node);
    if (pl)
	bsg_payload_bump_revision(pl);
    bsg_node_revision_bump(node);
    return 1;
}

/* pl_pick hook for BSG_PL_POLYGON payloads: find the nearest polygon vertex
 * to a model-coordinate sample point and fill a pick record for that vertex.
 */
static int
_bsg_polygon_pl_pick(struct bsg_payload *pl, const point_t sample,
		     struct bsg_pick_record *out)
{
    if (!pl || !out)
	return -1;
    struct bsg_polygon *p = pl->pl.polygon;
    if (!p || p->type != BSG_POLYGON_GENERAL)
	return -1;

    plane_t zpln;
    HMOVE(zpln, p->vp);
    zpln[3] += p->vZ;
    fastf_t fx, fy;
    point_t sample_pt;
    VMOVE(sample_pt, sample);
    bg_plane_closest_pt(&fx, &fy, &zpln, &sample_pt);
    point_t m_pt;
    bg_plane_pt_at(&m_pt, &zpln, fx, fy);

    double dist_min_sq = DBL_MAX;
    long closest_i = -1, closest_contour = -1;
    for (size_t j = 0; j < p->polygon.num_contours; j++) {
	struct bg_poly_contour *c = &p->polygon.contour[j];
	for (size_t i = 0; i < c->num_points; i++) {
	    double dcand = DIST_PNT_PNT_SQ(c->point[i], m_pt);
	    if (dcand < dist_min_sq) {
		closest_i = (long)i;
		closest_contour = (long)j;
		dist_min_sq = dcand;
	    }
	}
    }

    if (closest_i < 0)
	return 0;

    /* Fill the pick record with the closest vertex location.
     * pr_primitive_id encodes the contour index, pr_subelement_id the
     * point index within that contour so callers can unambiguously locate
     * the vertex.  pr_hit_dist carries the Euclidean distance. */
    out->pr_hit_dist = sqrt(dist_min_sq);
    out->pr_primitive_id = (int)closest_contour;
    out->pr_subelement_id = (int)closest_i;
    /* Scene/path fields must be filled by the caller from context. */
    out->pr_scene = (bsg_scene_ref)BSG_SCENE_REF_NULL_INIT;
    out->pr_feature = (bsg_feature_ref)BSG_FEATURE_REF_NULL_INIT;
    out->pr_valid = 0;
    bu_vls_init(&out->pr_source_path);
    bu_vls_init(&out->pr_instance_path);
    out->pr_screen_x = 0;
    out->pr_screen_y = 0;
    out->pr_view = NULL;
    return 1;
}

struct bsg_node *
bsg_create_polygon_obj(struct bsg_view *v, int flags, struct bsg_polygon *p)
{
    struct bsg_node *s = NULL;
    if (flags & BSG_SOURCE_VIEW) {
	/* View-scoped polygon producers attach directly under BSG view-scope
	 * nodes rather than relying on ptbl registration and bridge proxies. */
	s = bsg_feature_node(bsg_feature_create_polygon(v, NULL, (flags & BSG_SOURCE_LOCAL) ? 1 : 0));
    } else {
	s = bsg_scene_node_create_registered(v, flags);
    }
    if (!s)
	return NULL;
    bsg_node_set_object_type(s, bsg_geometry_type());
    bsg_node_add_geometry_roles(s, BSG_GEOMETRY_POLYGON_REGION |
	    BSG_GEOMETRY_VIEW_OVERLAY);

    // Construct the plane
    bsg_view_plane(&p->vp, v);

    bsg_appearance_set_line_width(s, 1);
    bsg_node_set_color(s, 255, 255, 0);
    {
	struct bsg_payload *_pl = bsg_payload_polygon_create(p);
	if (_pl)
	    _pl->pl_pick = _bsg_polygon_pl_pick;
	bsg_node_set_payload(s, _pl);
    }

    /* Have new polygon, now update typed line geometry. */
    _bsg_polygon_realize_lines(s);

    /* updated */
    bsg_node_revision_bump(s);

    return s;
}

struct bsg_node *
bsg_create_polygon(struct bsg_view *v, int flags, int type, point_t *fp)
{
    struct bsg_polygon *p;
    BU_GET(p, struct bsg_polygon);
    p->type = type;
    p->curr_contour_i = -1;
    p->curr_point_i = -1;

    // Set default fill color to blue
    unsigned char frgb[3] = {0, 0, 255};
    bu_color_from_rgb_chars(&p->fill_color, frgb);

    // Construct the plane
    bsg_view_plane(&p->vp, v);

    // Construct closest point to fp on plane
    fastf_t fx, fy;
    bg_plane_closest_pt(&fx, &fy, &p->vp, fp);
    point_t m_pt;
    bg_plane_pt_at(&m_pt, &p->vp, fx, fy);

    // This is now the origin point
    VMOVE(p->origin_point, m_pt);

    int pcnt = 1;
    if (type == BSG_POLYGON_CIRCLE)
	pcnt = 3;
    if (type == BSG_POLYGON_ELLIPSE)
	pcnt = 4;
    if (type == BSG_POLYGON_RECTANGLE)
	pcnt = 4;
    if (type == BSG_POLYGON_SQUARE)
	pcnt = 4;

    p->polygon.num_contours = 1;
    p->polygon.hole = (int *)bu_calloc(1, sizeof(int), "hole");
    p->polygon.contour = (struct bg_poly_contour *)bu_calloc(1, sizeof(struct bg_poly_contour), "contour");
    p->polygon.contour[0].num_points = pcnt;
    p->polygon.contour[0].open = 0;
    p->polygon.contour[0].point = (point_t *)bu_calloc(pcnt, sizeof(point_t), "point");
    p->polygon.hole[0] = 0;
    for (int i = 0; i < pcnt; i++) {
	VMOVE(p->polygon.contour[0].point[i], m_pt);
    }

    // Only the general polygon isn't closed out of the gate
    if (type == BSG_POLYGON_GENERAL)
	p->polygon.contour[0].open = 1;

    // Have polygon, now make shape node
    struct bsg_node *s = bsg_create_polygon_obj(v, flags, p);
    if (!s)
	BU_PUT(p, struct bsg_polygon);
    return s;
}

bsg_polygon_ref
bsg_create_polygon_ref(struct bsg_view *v, const char *name, int local, int type, point_t *fp)
{
    int flags = BSG_SOURCE_VIEW;
    if (local)
	flags |= BSG_SOURCE_LOCAL;
    struct bsg_node *node = bsg_create_polygon(v, flags, type, fp);
    struct bsg_polygon *p = bsg_node_polygon(node);
    if (p && p->type == BSG_POLYGON_GENERAL)
	p->curr_contour_i = 0;
    bsg_polygon_ref ref = _bsg_polygon_ref(node);
    if (!bsg_polygon_ref_is_null(ref) && name)
	(void)bsg_polygon_set_name(ref, name);
    return ref;
}

bsg_polygon_ref
bsg_polygon_ref_create_from_data(struct bsg_view *v, const char *name, int local, const struct bsg_polygon *data)
{
    if (!v || !data)
	return (bsg_polygon_ref)BSG_POLYGON_REF_NULL_INIT;

    struct bsg_polygon *copy;
    BU_GET(copy, struct bsg_polygon);
    memset(copy, 0, sizeof(*copy));
    bsg_polygon_cpy(copy, data);

    int flags = BSG_SOURCE_VIEW;
    if (local)
	flags |= BSG_SOURCE_LOCAL;

    struct bsg_node *node = bsg_create_polygon_obj(v, flags, copy);
    if (!node) {
	bg_polygon_free(&copy->polygon);
	BU_PUT(copy, struct bsg_polygon);
	return (bsg_polygon_ref)BSG_POLYGON_REF_NULL_INIT;
    }

    struct bsg_polygon *node_poly = bsg_node_polygon(node);
    if (node_poly) {
	bsg_polygon_cpy(node_poly, data);
	(void)bsg_polygon_realize(node);
    }

    bsg_polygon_ref ref = _bsg_polygon_ref(node);
    if (!bsg_polygon_ref_is_null(ref) && name)
	(void)bsg_polygon_set_name(ref, name);
    return ref;
}

bsg_polygon_ref
bsg_create_view_polygon_ref(struct bsg_view *v, int type, point_t *fp)
{
    return bsg_create_polygon_ref(v, NULL, 0, type, fp);
}

void
bsg_polygon_cpy(struct bsg_polygon *dest, const struct bsg_polygon *src)
{
    if (!src || !dest)
	return;

    dest->type = src->type;
    dest->fill_flag = src->fill_flag;
    V2MOVE(dest->fill_dir, src->fill_dir);
    dest->fill_delta = src->fill_delta;
    BU_COLOR_CPY(&dest->fill_color, &src->fill_color);
    dest->curr_contour_i = src->curr_contour_i;
    dest->curr_point_i = src->curr_point_i;
    VMOVE(dest->origin_point, src->origin_point);
    HMOVE(dest->vp, src->vp);
    dest->vZ = src->vZ;
    bg_polygon_free(&dest->polygon);
    bg_polygon_cpy(&dest->polygon, (struct bg_polygon *)&src->polygon);
    dest->u_data = src->u_data;
}

int
bsg_append_polygon_pt(struct bsg_node *s, point_t *np)
{
    struct bsg_polygon *p = bsg_node_polygon(s);
    if (p->type != BSG_POLYGON_GENERAL)
	return -1;

    if (p->curr_contour_i < 0)
	return -1;

    // Construct closest point to np on plane
    fastf_t fx, fy;
    bg_plane_closest_pt(&fx, &fy, &p->vp, np);
    point_t m_pt;
    bg_plane_pt_at(&m_pt, &p->vp, fx, fy);

    struct bg_poly_contour *c = &p->polygon.contour[p->curr_contour_i];
    c->num_points++;
    c->point = (point_t *)bu_realloc(c->point,c->num_points * sizeof(point_t), "realloc contour points");
    VMOVE(c->point[c->num_points-1], m_pt);

    /* Have new polygon point, now update typed line geometry. */
    _bsg_polygon_realize_lines(s);

    /* Updated */
    bsg_node_revision_bump(s);

    return 0;
}

// NOTE: This is a naive brute force search for the closest projected edge at
// the moment...  Would be better for repeated sampling of relatively static
// scenes to build an RTree first...
struct bsg_node *
bsg_select_polygon(struct bu_ptbl *objs, point_t *cp)
{
    if (!objs)
	return NULL;

    struct bsg_node *closest = NULL;
    double dist_min_sq = DBL_MAX;

    for (size_t i = 0; i < BU_PTBL_LEN(objs); i++) {
	struct bsg_node *s = (struct bsg_node *)BU_PTBL_GET(objs, i);
	if (bsg_node_has_geometry_role(s, BSG_GEOMETRY_POLYGON_REGION)) {
	    struct bsg_polygon *p = bsg_node_polygon(s);
	    // Because we're working in 2D orthogonal when processing polygons,
	    // the specific value of Z for each individual polygon isn't
	    // relevant - we want to find the closest edge in the projected
	    // view plane.  Accordingly, always construct the test point using
	    // whatever the current vZ is for the polygon being tested.
	    plane_t zpln;
	    HMOVE(zpln, p->vp);
	    zpln[3] += p->vZ;
	    fastf_t fx, fy;
	    bg_plane_closest_pt(&fx, &fy, &zpln, cp);
	    point_t m_pt;
	    bg_plane_pt_at(&m_pt, &zpln, fx, fy);

	    for (size_t j = 0; j < p->polygon.num_contours; j++) {
		struct bg_poly_contour *c = &p->polygon.contour[j];
		for (size_t k = 0; k < c->num_points; k++) {
		    double dcand;
		    if (k < c->num_points - 1) {
			dcand = bg_distsq_lseg3_pt(NULL, c->point[k], c->point[k+1], m_pt);
		    } else {
			dcand = bg_distsq_lseg3_pt(NULL, c->point[k], c->point[0], m_pt);
		    }
		    if (dcand < dist_min_sq) {
			dist_min_sq = dcand;
			closest = s;
		    }
		}
	    }
	}
    }

    return closest;
}

/* Typed version of bsg_select_polygon using feature-scope iteration internally
 * rather than a caller-supplied ptbl.  Walks all BSG view-scope nodes visible to
 * v, finds the polygon object whose edge is closest to cp, and returns it. */
struct _bsg_poly_select_ptbl {
    struct bu_ptbl objs;
};

struct _bsg_poly_visit_state {
    bsg_polygon_visit_cb cb;
    void *data;
    struct bu_ptbl *ptbl;
    struct bsg_node *exclude;
    size_t cnt;
    int cleared;
};

static int
_bsg_poly_collect_cb(struct bsg_node *obj, void *data)
{
    struct _bsg_poly_select_ptbl *s = (struct _bsg_poly_select_ptbl *)data;
    if (bsg_node_polygon(obj))
	bu_ptbl_ins(&s->objs, (long *)obj);
    return 1;
}

static int
_bsg_poly_visit_cb(struct bsg_node *obj, void *data)
{
    struct _bsg_poly_visit_state *s = (struct _bsg_poly_visit_state *)data;
    struct bsg_polygon *poly = bsg_node_polygon(obj);
    if (!s || !poly)
	return 1;
    return s->cb ? s->cb(obj, poly, s->data) : 1;
}

void
bsg_view_polygon_visit(struct bsg_view *v, bsg_polygon_visit_cb cb, void *data)
{
    if (!v || !cb)
	return;
    struct _bsg_poly_visit_state state;
    memset(&state, 0, sizeof(state));
    state.cb = cb;
    state.data = data;
    bsg_feature_visit_nodes(v, BSG_FEATURE_SCOPE_ALL, _bsg_poly_visit_cb, &state);
}

static int
_bsg_poly_snap_count_cb(struct bsg_node *node, struct bsg_polygon *UNUSED(poly), void *data)
{
    struct _bsg_poly_visit_state *s = (struct _bsg_poly_visit_state *)data;
    if (!s || node == s->exclude)
	return 1;
    s->cnt++;
    return 1;
}

size_t
bsg_view_polygon_snap_count(struct bsg_view *v, bsg_polygon_ref exclude)
{
    if (!v)
	return 0;
    struct _bsg_poly_visit_state state;
    memset(&state, 0, sizeof(state));
    state.exclude = _bsg_polygon_node(exclude);
    bsg_view_polygon_visit(v, _bsg_poly_snap_count_cb, &state);
    return state.cnt;
}

static int
_bsg_poly_clear_point_selection_cb(struct bsg_node *node, struct bsg_polygon *poly, void *data)
{
    struct _bsg_poly_visit_state *s = (struct _bsg_poly_visit_state *)data;
    if (!poly || poly->curr_point_i == -1)
	return 1;
    poly->curr_point_i = -1;
    poly->curr_contour_i = 0;
    bsg_update_polygon(node, node ? bsg_node_get_view(node) : NULL, BSG_POLYGON_UPDATE_PROPS_ONLY);
    if (s)
	s->cleared = 1;
    return 1;
}

int
bsg_view_polygon_clear_point_selection(struct bsg_view *v)
{
    struct _bsg_poly_visit_state state;
    memset(&state, 0, sizeof(state));
    bsg_view_polygon_visit(v, _bsg_poly_clear_point_selection_cb, &state);
    return state.cleared;
}

struct bsg_node *
bsg_view_select_polygon(struct bsg_view *v, point_t *cp)
{
    if (!v || !cp)
	return NULL;

    struct _bsg_poly_select_ptbl state;
    bu_ptbl_init(&state.objs, 8, "bsg_view_select_polygon objs");
    bsg_feature_visit_nodes(v, BSG_FEATURE_SCOPE_ALL, _bsg_poly_collect_cb, &state);
    struct bsg_node *result = bsg_select_polygon(&state.objs, cp);
    bu_ptbl_free(&state.objs);
    return result;
}

bsg_polygon_ref
bsg_view_select_polygon_ref(struct bsg_view *v, point_t *cp)
{
    return _bsg_polygon_ref(bsg_view_select_polygon(v, cp));
}

struct bsg_node *
bsg_view_polygon_find(struct bsg_view *v, const char *name)
{
    if (!v || !name)
	return NULL;
    struct bsg_node *node = bsg_feature_node(bsg_feature_find(v, name));
    return bsg_node_polygon(node) ? node : NULL;
}

bsg_polygon_ref
bsg_view_polygon_find_ref(struct bsg_view *v, const char *name)
{
    return _bsg_polygon_ref(bsg_view_polygon_find(v, name));
}

struct _bsg_polygon_find_scoped_state {
    const char *name;
    bsg_polygon_ref ref;
};

static int
_bsg_polygon_find_scoped_cb(struct bsg_node *node, void *data)
{
    struct _bsg_polygon_find_scoped_state *state = (struct _bsg_polygon_find_scoped_state *)data;
    if (!state || !bsg_polygon_ref_is_null(state->ref))
	return 0;
    if (!bsg_node_polygon(node))
	return 1;
    const char *node_name = bsg_node_name(node);
    if (node_name && BU_STR_EQUAL(node_name, state->name)) {
	state->ref = _bsg_polygon_ref(node);
	return 0;
    }
    return 1;
}

bsg_polygon_ref
bsg_view_polygon_find_scoped_ref(struct bsg_view *v, const char *name, int local_only)
{
    struct _bsg_polygon_find_scoped_state state;
    state.name = name;
    state.ref = (bsg_polygon_ref)BSG_POLYGON_REF_NULL_INIT;
    if (!v || !name || !strlen(name))
	return state.ref;
    bsg_feature_visit_nodes(v,
	    local_only ? BSG_FEATURE_SCOPE_LOCAL : BSG_FEATURE_SCOPE_ALL,
	    _bsg_polygon_find_scoped_cb, &state);
    return state.ref;
}

struct bsg_node *
bsg_view_polygon_dup(struct bsg_view *v, const char *name, const char *new_name)
{
    struct bsg_node *src = bsg_view_polygon_find(v, name);
    if (!src || !new_name)
	return NULL;
    return bsg_dup_view_polygon(new_name, src);
}

bsg_polygon_ref
bsg_view_polygon_dup_ref(struct bsg_view *v, const char *name, const char *new_name)
{
    return _bsg_polygon_ref(bsg_view_polygon_dup(v, name, new_name));
}

struct _bsg_poly_record_visit_state {
    bsg_polygon_record_visit_cb cb;
    void *data;
};

static int
_bsg_poly_record_visit_cb(struct bsg_node *node, struct bsg_polygon *UNUSED(poly), void *data)
{
    struct _bsg_poly_record_visit_state *s = (struct _bsg_poly_record_visit_state *)data;
    if (!s || !s->cb)
	return 1;
    struct bsg_polygon_record record;
    bsg_polygon_ref ref = _bsg_polygon_ref(node);
    if (!bsg_polygon_record_get(ref, &record))
	return 1;
    return s->cb(ref, &record, s->data);
}

void
bsg_view_polygon_visit_records(struct bsg_view *v, bsg_polygon_record_visit_cb cb, void *data)
{
    if (!v || !cb)
	return;
    struct _bsg_poly_record_visit_state state;
    state.cb = cb;
    state.data = data;
    bsg_view_polygon_visit(v, _bsg_poly_record_visit_cb, &state);
}

int
bsg_select_polygon_pt(struct bsg_node *s, point_t *cp)
{
    struct bsg_polygon *p = bsg_node_polygon(s);
    if (p->type != BSG_POLYGON_GENERAL)
	return -1;

    plane_t zpln;
    HMOVE(zpln, p->vp);
    zpln[3] += p->vZ;
    fastf_t fx, fy;
    bg_plane_closest_pt(&fx, &fy, &zpln, cp);
    point_t m_pt;
    bg_plane_pt_at(&m_pt, &zpln, fx, fy);

    // If a contour is selected, restrict our closest point candidates to
    // that contour's points
    double dist_min_sq = DBL_MAX;
    long closest_ind = -1;
    long closest_contour = -1;
    if (p->curr_contour_i >= 0) {
	struct bg_poly_contour *c = &p->polygon.contour[p->curr_contour_i];
	closest_contour = p->curr_contour_i;
	for (size_t i = 0; i < c->num_points; i++) {
	    double dcand = DIST_PNT_PNT_SQ(c->point[i], m_pt);
	    if (dcand < dist_min_sq) {
		closest_ind = (long)i;
		dist_min_sq = dcand;
	    }
	}
    } else {
	for (size_t j = 0; j < p->polygon.num_contours; j++) {
	    struct bg_poly_contour *c = &p->polygon.contour[j];
	    for (size_t i = 0; i < c->num_points; i++) {
		double dcand = DIST_PNT_PNT_SQ(c->point[i], m_pt);
		if (dcand < dist_min_sq) {
		    closest_ind = (long)i;
		    closest_contour = (long)j;
		    dist_min_sq = dcand;
		}
	    }
	}
    }

    p->curr_point_i = closest_ind;
    p->curr_contour_i = closest_contour;

    /* Have selected polygon point, now update typed line geometry. */
    _bsg_polygon_realize_lines(s);

    /* Updated */
    bsg_node_revision_bump(s);

    return 0;
}


void
bsg_select_clear_polygon_pt(struct bsg_node *s)
{
    if (!s)
	return;

    if (bsg_node_has_geometry_role(s, BSG_GEOMETRY_POLYGON_REGION)) {
	struct bsg_polygon *p = bsg_node_polygon(s);
	p->curr_point_i = -1;
	p->curr_contour_i = -1;
	_bsg_polygon_realize_lines(s);
	/* Updated */
	bsg_node_revision_bump(s);
    }
}


int
bsg_move_polygon(struct bsg_node *s, point_t *cp, point_t *prev_point)
{
    fastf_t pfx, pfy, fx, fy;
    struct bsg_polygon *p = bsg_node_polygon(s);

    plane_t zpln;
    HMOVE(zpln, p->vp);
    zpln[3] += p->vZ;
    bg_plane_closest_pt(&pfx, &pfy, &zpln, prev_point);
    bg_plane_closest_pt(&fx, &fy, &zpln, cp);
    point_t pm_pt, m_pt;
    bg_plane_pt_at(&pm_pt, &p->vp, pfx, pfy);
    bg_plane_pt_at(&m_pt, &p->vp, fx, fy);
    vect_t v_mv;
    VSUB2(v_mv, m_pt, pm_pt);

    for (size_t j = 0; j < p->polygon.num_contours; j++) {
	struct bg_poly_contour *c = &p->polygon.contour[j];
	for (size_t i = 0; i < c->num_points; i++) {
	    VADD2(c->point[i], c->point[i], v_mv);
	}
    }

    /* Polygon moved, now update typed line geometry. */
    _bsg_polygon_realize_lines(s);

    // Shift the origin point.
    VADD2(p->origin_point, p->origin_point, v_mv);

    /* Updated */
    bsg_node_revision_bump(s);

    return 0;
}

int
bsg_polygon_move(bsg_polygon_ref ref, point_t *cp, point_t *prev_point)
{
    return bsg_move_polygon(_bsg_polygon_node(ref), cp, prev_point);
}

int
bsg_polygon_set_current(bsg_polygon_ref ref, long contour_i, long point_i)
{
    struct bsg_node *node = _bsg_polygon_node(ref);
    struct bsg_polygon *p = bsg_node_polygon(node);
    if (!node || !p)
	return 0;
    if (contour_i >= (long)p->polygon.num_contours)
	return 0;
    if (contour_i >= 0 && point_i >= 0) {
	struct bg_poly_contour *c = &p->polygon.contour[contour_i];
	if (point_i >= (long)c->num_points)
	    return 0;
    }
    p->curr_contour_i = contour_i;
    p->curr_point_i = point_i;
    return 1;
}

int
bsg_move_polygon_pt(struct bsg_node *s, point_t *mp)
{
    struct bsg_polygon *p = bsg_node_polygon(s);
    if (p->type != BSG_POLYGON_GENERAL)
	return -1;

    // Need to have a point selected before we can move
    if (p->curr_point_i < 0 || p->curr_contour_i < 0)
	return -1;

    fastf_t fx, fy;
    plane_t zpln;
    HMOVE(zpln, p->vp);
    zpln[3] += p->vZ;
    bg_plane_closest_pt(&fx, &fy, &zpln, mp);
    point_t m_pt;
    bg_plane_pt_at(&m_pt, &zpln, fx, fy);

    struct bg_poly_contour *c = &p->polygon.contour[p->curr_contour_i];
    VMOVE(c->point[p->curr_point_i], m_pt);

    /* Polygon point moved, now update typed line geometry. */
    _bsg_polygon_realize_lines(s);

    /* Updated */
    bsg_node_revision_bump(s);

    return 0;
}

int
bsg_update_polygon_circle(struct bsg_node *s, point_t *cp, fastf_t pixel_size)
{
    struct bsg_polygon *p = bsg_node_polygon(s);

    fastf_t curr_fx, curr_fy;
    fastf_t r, arc;
    int nsegs, n;

    fastf_t pfx, pfy, fx, fy;
    plane_t zpln;
    HMOVE(zpln, p->vp);
    zpln[3] += p->vZ;
    bg_plane_closest_pt(&fx, &fy, &zpln, cp);
    bg_plane_closest_pt(&pfx, &pfy, &zpln, &p->origin_point);

    point_t pcp;
    bg_plane_pt_at(&pcp, &zpln, fx, fy);

    r = DIST_PNT_PNT(pcp, p->origin_point);

    /* use a variable number of segments based on the size of the
     * circle being created so small circles have few segments and
     * large ones are nice and smooth.
     */
    nsegs = M_PI_2 * r / pixel_size;
    if (nsegs < 32)
	nsegs = 32;

    struct bg_polygon gp;
    struct bg_polygon *gpp = &gp;
    gpp->num_contours = 1;
    gpp->hole = (int *)bu_calloc(1, sizeof(int), "hole");;
    gpp->contour = (struct bg_poly_contour *)bu_calloc(1, sizeof(struct bg_poly_contour), "contour");
    gpp->contour[0].num_points = nsegs;
    gpp->contour[0].open = 0;
    gpp->contour[0].point = (point_t *)bu_calloc(nsegs, sizeof(point_t), "point");

    arc = 360.0 / nsegs;
    for (n = 0; n < nsegs; ++n) {
	fastf_t ang = n * arc;

	curr_fx = cos(ang*DEG2RAD) * r + pfx;
	curr_fy = sin(ang*DEG2RAD) * r + pfy;
	point_t v_pt;
	bg_plane_pt_at(&v_pt, &p->vp, curr_fx, curr_fy);
	VMOVE(gpp->contour[0].point[n], v_pt);
    }

    bg_polygon_free(&p->polygon);

    /* Not doing a struct copy to avoid overwriting other properties. */
    p->polygon.num_contours = gp.num_contours;
    p->polygon.hole = gp.hole;
    p->polygon.contour = gp.contour;

    /* Circle polygon updated, now update typed line geometry. */
    _bsg_polygon_realize_lines(s);

    /* Updated */
    bsg_node_revision_bump(s);

    return 1;
}

int
bsg_update_polygon_ellipse(struct bsg_node *s, point_t *cp, fastf_t pixel_size)
{
    struct bsg_polygon *p = bsg_node_polygon(s);

    /* use a variable number of segments based on the size of the
     * circle being created so small circles have few segments and
     * large ones are nice and smooth.  select a chord length that
     * results in segments approximately 4 pixels in length.
     *
     * circumference / 4 = PI * diameter / 4
     *
     */

    fastf_t r = DIST_PNT_PNT(*cp, p->origin_point);

    /* use a variable number of segments based on the size of the
     * circle being created so small circles have few segments and
     * large ones are nice and smooth.
     */
    int nsegs = M_PI_2 * r / pixel_size;
    if (nsegs < 32)
	nsegs = 32;

    fastf_t pfx, pfy, fx, fy;
    plane_t zpln;
    HMOVE(zpln, p->vp);
    zpln[3] += p->vZ;
    bg_plane_closest_pt(&fx, &fy, &zpln, cp);
    bg_plane_closest_pt(&pfx, &pfy, &zpln, &p->origin_point);

    fastf_t a, b, arc;
    point_t pv_pt;
    point_t ellout;
    point_t A, B;

    VSET(pv_pt, pfx, pfy, 0);
    a = fx - pfx;
    b = fy - pfy;

    /*
     * For angle alpha, compute surface point as
     *
     * V + cos(alpha) * A + sin(alpha) * B
     *
     * note that sin(alpha) is cos(90-alpha).
     */

    VSET(A, a, 0, 0);
    VSET(B, 0, b, 0);

    struct bg_polygon gp;
    struct bg_polygon *gpp = &gp;
    gpp->num_contours = 1;
    gpp->hole = (int *)bu_calloc(1, sizeof(int), "hole");;
    gpp->contour = (struct bg_poly_contour *)bu_calloc(1, sizeof(struct bg_poly_contour), "contour");
    gpp->contour[0].num_points = nsegs;
    gpp->contour[0].open = 0;
    gpp->contour[0].point = (point_t *)bu_calloc(nsegs, sizeof(point_t), "point");

    arc = 360.0 / nsegs;
    for (int n = 0; n < nsegs; ++n) {
	fastf_t cosa = cos(n * arc * DEG2RAD);
	fastf_t sina = sin(n * arc * DEG2RAD);

	VJOIN2(ellout, pv_pt, cosa, A, sina, B);

	// Use the polygon's plane for actually adding the points
	bg_plane_pt_at(&gpp->contour[0].point[n], &zpln, ellout[0], ellout[1]);
    }

    bg_polygon_free(&p->polygon);

    /* Not doing a struct copy to avoid overwriting other properties. */
    p->polygon.num_contours = gp.num_contours;
    p->polygon.hole = gp.hole;
    p->polygon.contour = gp.contour;

    /* Ellipse polygon updated, now update typed line geometry. */
    _bsg_polygon_realize_lines(s);

    /* Updated */
    bsg_node_revision_bump(s);

    return 1;
}

int
bsg_update_polygon_rectangle(struct bsg_node *s, point_t *cp)
{
    struct bsg_polygon *p = bsg_node_polygon(s);

    fastf_t pfx, pfy, fx, fy;
    plane_t zpln;
    HMOVE(zpln, p->vp);
    zpln[3] += p->vZ;
    bg_plane_closest_pt(&pfx, &pfy, &zpln, &p->origin_point);
    bg_plane_closest_pt(&fx, &fy, &zpln, cp);

    // Use the polygon's plane for actually adjusting the points
    bg_plane_pt_at(&p->polygon.contour[0].point[0], &zpln, pfx, pfy);
    bg_plane_pt_at(&p->polygon.contour[0].point[1], &zpln, pfx, fy);
    bg_plane_pt_at(&p->polygon.contour[0].point[2], &zpln, fx, fy);
    bg_plane_pt_at(&p->polygon.contour[0].point[3], &zpln, fx, pfy);

    p->polygon.contour[0].open = 0;

    /* Polygon updated, now update typed line geometry. */
    _bsg_polygon_realize_lines(s);

    /* Updated */
    bsg_node_revision_bump(s);

    return 1;
}

int
bsg_update_polygon_square(struct bsg_node *s, point_t *cp)
{
    struct bsg_polygon *p = bsg_node_polygon(s);

    fastf_t pfx, pfy, fx, fy;
    plane_t zpln;
    HMOVE(zpln, p->vp);
    zpln[3] += p->vZ;
    bg_plane_closest_pt(&pfx, &pfy, &zpln, &p->origin_point);
    bg_plane_closest_pt(&fx, &fy, &zpln, cp);

    fastf_t dx = fx - pfx;
    fastf_t dy = fy - pfy;

    if (fabs(dx) > fabs(dy)) {
	if (dy < 0.0)
	    fy = pfy - fabs(dx);
	else
	    fy = pfy + fabs(dx);
    } else {
	if (dx < 0.0)
	    fx = pfx - fabs(dy);
	else
	    fx = pfx + fabs(dy);
    }


    // Use the polygon's plane for actually adjusting the points
    bg_plane_pt_at(&p->polygon.contour[0].point[0], &zpln, pfx, pfy);
    bg_plane_pt_at(&p->polygon.contour[0].point[1], &zpln, pfx, fy);
    bg_plane_pt_at(&p->polygon.contour[0].point[2], &zpln, fx, fy);
    bg_plane_pt_at(&p->polygon.contour[0].point[3], &zpln, fx, pfy);

    /* Polygon updated, now update typed line geometry. */
    _bsg_polygon_realize_lines(s);

    /* Updated */
    bsg_node_revision_bump(s);

    return 1;
}

int
bsg_update_general_polygon(struct bsg_node *s, int utype, point_t *cp)
{
    struct bsg_polygon *p = bsg_node_polygon(s);
    if (p->type != BSG_POLYGON_GENERAL)
	return 0;

    if (utype == BSG_POLYGON_UPDATE_PT_APPEND) {
	return bsg_append_polygon_pt(s, cp);
    }

    if (utype == BSG_POLYGON_UPDATE_PT_SELECT) {
	return bsg_select_polygon_pt(s, cp);
    }

    if (utype == BSG_POLYGON_UPDATE_PT_SELECT_CLEAR) {
	bsg_select_clear_polygon_pt(s);
	return 1;
    }

    if (utype == BSG_POLYGON_UPDATE_PT_MOVE) {
	return bsg_move_polygon_pt(s, cp);
    }

    /* Polygon updated, now update typed line geometry. */
    _bsg_polygon_realize_lines(s);

    /* Updated */
    bsg_node_revision_bump(s);

    return 0;
}

int
bsg_update_polygon(struct bsg_node *s, struct bsg_view *v, int utype)
{
    if (!s)
	return 0;

    struct bsg_polygon *p = bsg_node_polygon(s);

    // Regardless of type, sync fill color
    struct bsg_node *fobj = bsg_scene_child_find(s, "*fill*");
    if (fobj) {
	unsigned char color[3] = {0, 0, 0};
	bu_color_to_rgb_chars(&p->fill_color, color);
	bsg_node_set_color(fobj, color[0], color[1], color[2]);
    }

    if (utype == BSG_POLYGON_UPDATE_PROPS_ONLY) {

	for (size_t i = 0; i < BU_PTBL_LEN(&s->children); i++) {
	    struct bsg_node *s_c = (struct bsg_node *)BU_PTBL_GET(&s->children, i);
	    unsigned char color[3] = {0, 0, 0};
	    if (!s_c)
		continue;
	    bsg_node_get_color(s, &color[0], &color[1], &color[2]);
	    bsg_node_set_color(s_c, color[0], color[1], color[2]);
	}

	if (p->fill_flag) {
	    bsg_fill_polygon(s);
	} else {
	    if (fobj)
		bsg_scene_node_release(fobj);
	}

	/* Props-only changes still advance the revision so renderers downstream
	 * and any attached edit-preview source can detect color/fill updates
	 * without polling node revisions. */
	struct bsg_payload *payload = bsg_node_get_payload(s);
	if (payload)
	    bsg_payload_bump_revision(payload);

	return 0;
    }

    /* Need pixel dimension for calculating segment approximations on these
     * shapes - based on view info */
    int changed = 0;
    if (p->type == BSG_POLYGON_CIRCLE || p->type == BSG_POLYGON_ELLIPSE) {

	// Need the length of the diagonal of a pixel
	vect_t c1 = VINIT_ZERO;
	vect_t c2 = VINIT_ZERO;
	bsg_screen_to_view(v, &c1[0], &c1[1], 0, 0);
	bsg_screen_to_view(v, &c2[0], &c2[1], 1, 1);
	point_t p1, p2;
	MAT4X3PNT(p1, v->gv_view2model, c1);
	MAT4X3PNT(p2, v->gv_view2model, c2);
	fastf_t d = DIST_PNT_PNT(p1, p2);

	if (p->type == BSG_POLYGON_CIRCLE)
	    changed = bsg_update_polygon_circle(s, &v->gv_point, d);
	else
	    changed = bsg_update_polygon_ellipse(s, &v->gv_point, d);
    } else if (p->type == BSG_POLYGON_RECTANGLE) {
	changed = bsg_update_polygon_rectangle(s, &v->gv_point);
    } else if (p->type == BSG_POLYGON_SQUARE) {
	changed = bsg_update_polygon_square(s, &v->gv_point);
    } else if (p->type == BSG_POLYGON_GENERAL) {
	changed = bsg_update_general_polygon(s, utype, &v->gv_point);
    }

    /* Whenever geometry actually changed, advance the payload revision.  Any
     * edit-preview source attached to this node can compare
     * last_realized_revision to detect the change without the caller having to
     * check node revisions directly. */
    if (changed) {
	struct bsg_payload *payload = bsg_node_get_payload(s);
	if (payload)
	    bsg_payload_bump_revision(payload);
    }

    return changed;
}

int
bsg_polygon_update(bsg_polygon_ref ref, struct bsg_view *v, int utype)
{
    struct bsg_node *node = _bsg_polygon_node(ref);
    if (!node)
	return 0;
    if (!v)
	v = bsg_node_get_view(node);
    return bsg_update_polygon(node, v, utype);
}

int
bsg_polygon_update_screen_pt(bsg_polygon_ref ref, struct bsg_view *v, int x, int y, int utype)
{
    struct bsg_node *node = _bsg_polygon_node(ref);
    if (!node)
	return 0;
    if (!v)
	v = bsg_node_get_view(node);
    if (!v)
	return 0;
    v->gv_mouse_x = x;
    v->gv_mouse_y = y;
    if (bsg_screen_pt(&v->gv_point, (fastf_t)x, (fastf_t)y, v))
	return 0;
    (void)bsg_update_polygon(node, v, utype);
    return 1;
}

int
bsg_polygon_set_name(bsg_polygon_ref ref, const char *name)
{
    struct bsg_node *node = _bsg_polygon_node(ref);
    if (!node || !name)
	return 0;
    bsg_node_set_name(node, name);
    return 1;
}

int
bsg_polygon_set_view(bsg_polygon_ref ref, struct bsg_view *v)
{
    struct bsg_node *node = _bsg_polygon_node(ref);
    if (!node)
	return 0;
    bsg_node_set_view(node, v);
    return 1;
}

int
bsg_polygon_set_visual(bsg_polygon_ref ref,
		       const struct bu_color *edge_color,
		       const struct bu_color *fill_color,
		       fastf_t fill_slope_x,
		       fastf_t fill_slope_y,
		       fastf_t fill_density,
		       fastf_t vZ,
		       int fill_flag)
{
    struct bsg_node *node = _bsg_polygon_node(ref);
    struct bsg_polygon *p = bsg_node_polygon(node);
    if (!node || !p)
	return 0;

    if (edge_color) {
	unsigned char rgb[3] = {0, 0, 0};
	bu_color_to_rgb_chars(edge_color, rgb);
	bsg_node_set_color(node, rgb[0], rgb[1], rgb[2]);
    }
    if (fill_color)
	BU_COLOR_CPY(&p->fill_color, fill_color);
    p->fill_dir[0] = fill_slope_x;
    p->fill_dir[1] = fill_slope_y;
    p->fill_delta = fill_density;
    p->vZ = vZ;
    p->fill_flag = fill_flag ? 1 : 0;
    return bsg_update_polygon(node, bsg_node_get_view(node), BSG_POLYGON_UPDATE_PROPS_ONLY);
}

int
bsg_polygon_set_open(bsg_polygon_ref ref, int open)
{
    struct bsg_node *node = _bsg_polygon_node(ref);
    struct bsg_polygon *p = bsg_node_polygon(node);
    if (!node || !p || p->type != BSG_POLYGON_GENERAL || !p->polygon.contour)
	return 0;
    p->polygon.contour[0].open = open ? 1 : 0;
    return bsg_update_polygon(node, bsg_node_get_view(node), BSG_POLYGON_UPDATE_DEFAULT);
}

int
bsg_polygon_set_contour_open(bsg_polygon_ref ref, long contour_i, int open)
{
    struct bsg_node *node = _bsg_polygon_node(ref);
    struct bsg_polygon *p = bsg_node_polygon(node);
    if (!node || !p || p->type != BSG_POLYGON_GENERAL || !p->polygon.contour)
	return 0;
    if (contour_i < 0 || contour_i >= (long)p->polygon.num_contours)
	return 0;
    p->polygon.contour[contour_i].open = open ? 1 : 0;
    (void)bsg_update_polygon(node, bsg_node_get_view(node), BSG_POLYGON_UPDATE_DEFAULT);
    return 1;
}

int
bsg_polygon_set_all_contours_open(bsg_polygon_ref ref, int open)
{
    struct bsg_node *node = _bsg_polygon_node(ref);
    struct bsg_polygon *p = bsg_node_polygon(node);
    if (!node || !p || p->type != BSG_POLYGON_GENERAL || !p->polygon.contour)
	return 0;
    for (size_t i = 0; i < p->polygon.num_contours; i++)
	p->polygon.contour[i].open = open ? 1 : 0;
    (void)bsg_update_polygon(node, bsg_node_get_view(node), BSG_POLYGON_UPDATE_DEFAULT);
    return 1;
}

int
bsg_polygon_close(bsg_polygon_ref ref)
{
    struct bsg_node *node = _bsg_polygon_node(ref);
    struct bsg_polygon *p = bsg_node_polygon(node);
    if (!node || !p || !p->polygon.contour)
	return 0;
    if (!p->polygon.contour[0].open)
	return 1;
    if (p->polygon.contour[0].num_points < 3) {
	bsg_scene_node_release(node);
	return 0;
    }
    p->polygon.contour[0].open = 0;
    bsg_update_polygon(node, bsg_node_get_view(node), BSG_POLYGON_UPDATE_DEFAULT);
    return 1;
}

int
bsg_polygon_clear_selected_point(bsg_polygon_ref ref)
{
    struct bsg_node *node = _bsg_polygon_node(ref);
    struct bsg_polygon *p = bsg_node_polygon(node);
    if (!node || !p)
	return 0;
    p->curr_point_i = -1;
    p->curr_contour_i = 0;
    bsg_update_polygon(node, bsg_node_get_view(node), BSG_POLYGON_UPDATE_PROPS_ONLY);
    return 1;
}

int
bsg_polygon_set_fill(bsg_polygon_ref ref, int fill_flag, fastf_t fill_slope_x, fastf_t fill_slope_y, fastf_t fill_density)
{
    struct bsg_node *node = _bsg_polygon_node(ref);
    struct bsg_polygon *p = bsg_node_polygon(node);
    if (!node || !p)
	return 0;
    p->fill_flag = fill_flag ? 1 : 0;
    p->fill_dir[0] = fill_slope_x;
    p->fill_dir[1] = fill_slope_y;
    p->fill_delta = fill_density;
    (void)bsg_update_polygon(node, bsg_node_get_view(node), BSG_POLYGON_UPDATE_DEFAULT);
    return 1;
}

int
bsg_polygon_fill_color_get(bsg_polygon_ref ref, struct bu_color *fill_color)
{
    struct bsg_polygon *p = bsg_node_polygon(_bsg_polygon_node(ref));
    if (!p || !fill_color)
	return 0;
    BU_COLOR_CPY(fill_color, &p->fill_color);
    return 1;
}

int
bsg_polygon_fill_color_set(bsg_polygon_ref ref, const struct bu_color *fill_color)
{
    struct bsg_node *node = _bsg_polygon_node(ref);
    struct bsg_polygon *p = bsg_node_polygon(node);
    if (!node || !p || !fill_color)
	return 0;
    BU_COLOR_CPY(&p->fill_color, fill_color);
    (void)bsg_update_polygon(node, bsg_node_get_view(node), BSG_POLYGON_UPDATE_PROPS_ONLY);
    return 1;
}

int
bsg_polygon_remove(bsg_polygon_ref ref)
{
    struct bsg_node *node = _bsg_polygon_node(ref);
    if (!node)
	return 0;
    bsg_scene_node_release(node);
    return 1;
}

void *
bsg_polygon_user_data(bsg_polygon_ref ref)
{
    struct bsg_polygon *p = bsg_node_polygon(_bsg_polygon_node(ref));
    return p ? p->u_data : NULL;
}

int
bsg_polygon_user_data_set(bsg_polygon_ref ref, void *user_data)
{
    struct bsg_polygon *p = bsg_node_polygon(_bsg_polygon_node(ref));
    if (!p)
	return 0;
    p->u_data = user_data;
    return 1;
}

struct bsg_node *
bsg_dup_view_polygon(const char *nname, struct bsg_node *s)
{
    if (!nname || !s)
	return NULL;

    struct bsg_polygon *ip = bsg_node_polygon(s);

    struct bsg_polygon *p;
    BU_GET(p, struct bsg_polygon);
    bsg_polygon_cpy(p, ip);

    struct bsg_node *np = bsg_create_polygon_obj(bsg_node_get_view(s), (int)bsg_node_source_flags(s), p);

    // Have geometry, now copy visual settings
    {
	unsigned char color[3] = {0, 0, 0};
	bsg_node_get_color(s, &color[0], &color[1], &color[2]);
	bsg_node_set_color(np, color[0], color[1], color[2]);
    }

    // Update scene object line geometry.
    _bsg_polygon_realize_lines(np);

    bsg_node_set_name(np, nname);

    // Return new object
    return np;
}

int
bsg_polygon_csg_ref(bsg_polygon_ref target, bsg_polygon_ref stencil, bg_clip_t op)
{
    return bsg_polygon_csg(_bsg_polygon_node(target), _bsg_polygon_node(stencil), op);
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
