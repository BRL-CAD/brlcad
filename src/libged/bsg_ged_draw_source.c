/*              B S G _ G E D _ D R A W _ S O U R C E . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this file; see the file named COPYING for more information.
 */
/** @file bsg_ged_draw_source.c
 *
 * GED draw-source metadata helpers backed by public BSG database-source fields.
 */

#include "common.h"

#include <limits.h>
#include <math.h>
#include <string.h>

#include "bg/plane.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bsg/appearance.h"
#include "bsg/database_source.h"
#include "bsg/draw_source.h"
#include "bsg/draw_intent.h"
#include "bsg/field.h"
#include "bsg/geometry.h"
#include "bsg/material.h"
#include "bsg/scene_builder.h"
#include "ged/bsg_ged_draw.h"
#include "nmg.h"
#include "rt/db_instance.h"
#include "rt/db_io.h"
#include "rt/functab.h"
#include "rt/geom.h"
#include "rt/primitives/arb8.h"
#include "rt/primitives/datum.h"
#include "rt/primitives/dsp.h"
#include "rt/primitives/ebm.h"
#include "rt/primitives/ell.h"
#include "rt/primitives/ehy.h"
#include "rt/primitives/epa.h"
#include "rt/primitives/eto.h"
#include "rt/primitives/extrude.h"
#include "rt/primitives/hf.h"
#include "rt/primitives/hrt.h"
#include "rt/primitives/pipe.h"
#include "rt/primitives/rhc.h"
#include "rt/primitives/rpc.h"
#include "rt/primitives/revolve.h"
#include "rt/primitives/sketch.h"
#include "rt/primitives/superell.h"
#include "rt/primitives/tgc.h"
#include "rt/primitives/tor.h"
#include "rt/primitives/vol.h"
#include "rt/tree.h"
#include "./ged_private.h"
#include "./bsg_ged_draw_private.h"


static void _ged_draw_scene_ref_geometry_set_command_count(bsg_scene_ref ref, size_t vlen);
static void _ged_draw_scene_ref_copy_display_state(bsg_scene_ref dst, bsg_scene_ref src);
static bsg_scene_ref _ged_draw_scene_ref_create_child_shape(bsg_scene_ref primary_ref,
							    const char *source_name,
							    const char *shape_name,
							    bsg_scene_ref *child_source_out);
static void _ged_draw_scene_ref_clear_child_sources(bsg_scene_ref primary_ref);
static int ged_draw_scene_ref_publish_submodel_wireframe_children(bsg_scene_ref ref,
								  struct rt_db_internal *ip,
								  const struct bg_tess_tol *ttol,
								  const struct bn_tol *tol,
								  struct bsg_view *v);

enum {
    GED_DRAW_NMG_CNURB_INTERIOR_SAMPLES = 10,
    GED_DRAW_NMG_CNURB_SAMPLE_POINTS = GED_DRAW_NMG_CNURB_INTERIOR_SAMPLES + 2,
    GED_DRAW_NMG_SNURB_INTERIOR_SAMPLES = 10
};


const char *
ged_draw_scene_ref_name(bsg_scene_ref ref)
{
    return bsg_scene_name(ref);
}


const struct db_full_path *
ged_draw_scene_ref_fullpath(bsg_scene_ref ref)
{
    ged_draw_shape_state *bd = ged_draw_shape_state_get_scene_ref(ref);
    if (!bd || bd->s_fullpath.fp_len <= 0)
        return NULL;
    return &bd->s_fullpath;
}


struct directory *
ged_draw_scene_ref_leaf_dp(bsg_scene_ref ref)
{
    ged_draw_shape_state *bd = ged_draw_shape_state_get_scene_ref(ref);
    if (!bd)
	return RT_DIR_NULL;
    if (bd->leaf_dp)
	return bd->leaf_dp;
    if (bd->s_fullpath.fp_len > 0)
	return DB_FULL_PATH_CUR_DIR(&bd->s_fullpath);
    return RT_DIR_NULL;
}


ged_draw_shape_state *
ged_draw_scene_ref_shape_state(bsg_scene_ref ref)
{
    return ged_draw_shape_state_get_scene_ref(ref);
}


struct ged_draw_source_state *
ged_draw_scene_ref_source_data(bsg_scene_ref ref)
{
    ged_draw_shape_state *shape_data = ged_draw_shape_state_get_scene_ref(ref);
    return shape_data ? shape_data->source_data : NULL;
}


struct bu_list *
ged_draw_scene_ref_geometry_pool(bsg_scene_ref ref)
{
    return bsg_scene_ref_is_null(ref) ? NULL : &rt_vlfree;
}


int
ged_draw_scene_ref_geometry_clear(bsg_scene_ref ref)
{
    ged_draw_shape_state *shape_data = ged_draw_shape_state_get_scene_ref(ref);
    if (!shape_data)
	return 0;
    (void)bsg_geometry_ref_clear(bsg_scene_ref_as_geometry(ref));
    shape_data->geometry_command_count = 0;
    shape_data->geometry_revision++;
    bsg_scene_invalidate(ref);
    return 1;
}


bsg_geometry_ref
ged_draw_scene_ref_geometry_ref(bsg_scene_ref ref)
{
    return bsg_scene_ref_as_geometry(ref);
}

static void
_ged_draw_bounds_include_point(point_t bmin, point_t bmax, const point_t pt)
{
    VMINMAX(bmin, bmax, pt);
}


static void
_ged_draw_bounds_include_point_marker(point_t bmin, point_t bmax, const point_t pt)
{
    point_t marker_min;
    point_t marker_max;

    VSET(marker_min, pt[X] - 1.0, pt[Y] - 1.0, pt[Z] - 1.0);
    VSET(marker_max, pt[X] + 1.0, pt[Y] + 1.0, pt[Z] + 1.0);
    VMIN(bmin, marker_min);
    VMAX(bmax, marker_max);
}


static int
_ged_draw_line_geometry_bounds(bsg_geometry_ref geometry,
			       point_t bmin,
			       point_t bmax,
			       size_t *length,
			       int *bad_cmd)
{
    bsg_field_ref coords = bsg_geometry_ref_coordinates_field(geometry);
    bsg_field_ref commands = bsg_geometry_ref_primitive_sets_field(geometry);
    size_t count = bsg_field_multi_count(coords);

    for (size_t i = 0; i < count; i++) {
	point_t pt;
	int cmd = (i % 2) ? BSG_GEOMETRY_LINE_DRAW : BSG_GEOMETRY_LINE_MOVE;

	if (!bsg_field_multi_point_at(coords, i, pt))
	    return 0;
	(void)bsg_field_multi_int_at(commands, i, &cmd);

	switch (cmd) {
	    case BSG_GEOMETRY_LINE_MOVE:
	    case BSG_GEOMETRY_LINE_DRAW:
		_ged_draw_bounds_include_point(bmin, bmax, pt);
		break;
	    case BSG_GEOMETRY_POINT_DRAW:
		_ged_draw_bounds_include_point_marker(bmin, bmax, pt);
		break;
	    default:
		if (bad_cmd)
		    *bad_cmd = cmd;
		return 0;
	}
    }

    if (length)
	*length = count;
    return count ? 1 : 0;
}


static int
_ged_draw_point_geometry_bounds(bsg_geometry_ref geometry,
				point_t bmin,
				point_t bmax,
				size_t *length)
{
    bsg_field_ref coords = bsg_geometry_ref_coordinates_field(geometry);
    size_t count = bsg_field_multi_count(coords);

    for (size_t i = 0; i < count; i++) {
	point_t pt;
	if (!bsg_field_multi_point_at(coords, i, pt))
	    return 0;
	_ged_draw_bounds_include_point_marker(bmin, bmax, pt);
    }

    if (length)
	*length = count;
    return count ? 1 : 0;
}


static int
_ged_draw_indexed_geometry_bounds(bsg_geometry_ref geometry,
				  point_t bmin,
				  point_t bmax,
				  size_t *length)
{
    bsg_field_ref coords = bsg_geometry_ref_coordinates_field(geometry);
    bsg_field_ref indices = bsg_geometry_ref_indices_field(geometry);
    size_t coord_count = bsg_field_multi_count(coords);
    size_t index_count = bsg_field_multi_count(indices);
    size_t used = 0;

    for (size_t i = 0; i < index_count; i++) {
	int idx = -1;
	point_t pt;

	if (!bsg_field_multi_int_at(indices, i, &idx))
	    return 0;
	if (idx < 0)
	    continue;
	if ((size_t)idx >= coord_count)
	    return 0;
	if (!bsg_field_multi_point_at(coords, (size_t)idx, pt))
	    return 0;
	_ged_draw_bounds_include_point(bmin, bmax, pt);
	used++;
    }

    if (length)
	*length = index_count;
    return used ? 1 : 0;
}


static int
_ged_draw_scene_ref_geometry_bounds(bsg_scene_ref ref,
				    point_t bmin,
				    point_t bmax,
				    size_t *length,
				    int *bad_cmd)
{
    bsg_geometry_ref geometry = bsg_scene_ref_as_geometry(ref);

    if (length)
	*length = 0;
    if (bad_cmd)
	*bad_cmd = 0;

    switch (bsg_geometry_ref_kind(geometry)) {
	case BSG_GEOMETRY_NODE_LINE_SET:
	    return _ged_draw_line_geometry_bounds(geometry, bmin, bmax,
		    length, bad_cmd);
	case BSG_GEOMETRY_NODE_POINT_SET:
	    return _ged_draw_point_geometry_bounds(geometry, bmin, bmax, length);
	case BSG_GEOMETRY_NODE_INDEXED_LINE_SET:
	case BSG_GEOMETRY_NODE_INDEXED_FACE_SET:
	    return _ged_draw_indexed_geometry_bounds(geometry, bmin, bmax, length);
	default:
	    return 0;
    }
}


int
ged_draw_scene_ref_update_bounds_from_geometry(bsg_scene_ref ref, int *bad_cmd)
{
    if (bad_cmd)
	*bad_cmd = 0;
    if (bsg_scene_ref_is_null(ref))
	return 0;

    point_t bmin, bmax;
    size_t length = 0;
    VSET(bmin, INFINITY, INFINITY, INFINITY);
    VSET(bmax, -INFINITY, -INFINITY, -INFINITY);

    if (!_ged_draw_scene_ref_geometry_bounds(ref, bmin, bmax, &length, bad_cmd))
	return 0;

    _ged_draw_scene_ref_geometry_set_command_count(ref, length);

    vect_t center;
    center[X] = (bmin[X] + bmax[X]) * 0.5;
    center[Y] = (bmin[Y] + bmax[Y]) * 0.5;
    center[Z] = (bmin[Z] + bmax[Z]) * 0.5;
    bsg_scene_set_draw_center(ref, center);

    fastf_t size = bmax[X] - bmin[X];
    V_MAX(size, bmax[Y] - bmin[Y]);
    V_MAX(size, bmax[Z] - bmin[Z]);
    bsg_scene_set_draw_size(ref, size);
    bsg_scene_set_non_database_source(ref, 0);
    return 1;
}


void
ged_draw_scene_ref_set_draw_center(bsg_scene_ref ref, const point_t center)
{
    bsg_scene_set_draw_center(ref, center);
}


int
ged_draw_scene_ref_set_bounds_from_minmax(bsg_scene_ref ref,
					  const point_t min,
					  const point_t max,
					  int set_scene_bounds)
{
    if (bsg_scene_ref_is_null(ref) || !min || !max)
	return 0;

    vect_t center;
    center[X] = (min[X] + max[X]) * 0.5;
    center[Y] = (min[Y] + max[Y]) * 0.5;
    center[Z] = (min[Z] + max[Z]) * 0.5;
    bsg_scene_set_draw_center(ref, center);

    fastf_t size = max[X] - min[X];
    V_MAX(size, max[Y] - min[Y]);
    V_MAX(size, max[Z] - min[Z]);
    bsg_scene_set_draw_size(ref, size);

    if (set_scene_bounds)
	bsg_scene_set_bounds(ref, min, max, 1);
    return 1;
}


int
ged_draw_scene_ref_draw_mat(bsg_scene_ref ref, mat_t mat)
{
    if (bsg_scene_ref_is_null(ref) || !mat)
	return 0;
    bsg_scene_draw_mat(ref, mat);
    return 1;
}


int
ged_draw_scene_ref_draw_mode(bsg_scene_ref ref)
{
    return bsg_scene_dmode(ref);
}


void
ged_draw_scene_ref_set_draw_mode(bsg_scene_ref ref, int draw_mode)
{
    bsg_scene_set_dmode(ref, draw_mode);
    struct bsg_draw_intent *di = bsg_scene_draw_intent(ref);
    if (di)
	bsg_draw_intent_set_mode(di, (bsg_draw_mode)draw_mode);
}


int
ged_draw_scene_ref_line_style(bsg_scene_ref ref)
{
    return bsg_scene_line_style(ref);
}


void
ged_draw_scene_ref_set_work_flag(bsg_scene_ref ref, int wflag)
{
    bsg_scene_set_work_flag(ref, wflag);
}


int
ged_draw_scene_ref_apply_settings(bsg_scene_ref ref,
				  struct bsg_appearance_settings *settings)
{
    return bsg_scene_apply_appearance_settings(ref, settings);
}


void
ged_draw_scene_ref_set_material_rgb(bsg_scene_ref ref,
				    const unsigned char rgb[3])
{
    if (!rgb)
	return;
    bsg_scene_material_set_rgb(ref, rgb[0], rgb[1], rgb[2]);
}


int
ged_draw_scene_ref_realization_current(bsg_scene_ref ref)
{
    bsg_database_source_ref source =
	bsg_database_source_ref_from_scene(ged_draw_shape_source_ref(ref));
    return bsg_database_source_ref_realization_status(source) ==
	BSG_DATABASE_SOURCE_REALIZATION_CURRENT;
}


void
ged_draw_scene_ref_realization_set_current(bsg_scene_ref ref, int current)
{
    bsg_database_source_realization_status status =
	current ? BSG_DATABASE_SOURCE_REALIZATION_CURRENT :
	BSG_DATABASE_SOURCE_REALIZATION_STALE;
    bsg_scene_ref source_ref = ged_draw_shape_source_ref(ref);
    (void)bsg_database_source_ref_set_realization_status(
	    bsg_database_source_ref_from_scene(source_ref), status);
    if (!bsg_scene_ref_equal(source_ref, ref))
	(void)bsg_database_source_ref_set_realization_status(
		bsg_database_source_ref_from_scene(ref), status);
}


void
ged_draw_scene_ref_realization_set_roles(bsg_scene_ref ref, int csg_obj, int mesh_obj)
{
    int flags = BSG_DATABASE_SOURCE_REALIZATION_ROLE_NONE;
    if (csg_obj)
	flags |= BSG_DATABASE_SOURCE_REALIZATION_ROLE_CSG;
    if (mesh_obj)
	flags |= BSG_DATABASE_SOURCE_REALIZATION_ROLE_MESH;
    bsg_scene_ref source_ref = ged_draw_shape_source_ref(ref);
    (void)bsg_database_source_ref_set_realization_role_flags(
	    bsg_database_source_ref_from_scene(source_ref), flags);
    if (!bsg_scene_ref_equal(source_ref, ref))
	(void)bsg_database_source_ref_set_realization_role_flags(
		bsg_database_source_ref_from_scene(ref), flags);
}


fastf_t
ged_draw_scene_ref_realization_view_scale(bsg_scene_ref ref)
{
    return (fastf_t)bsg_database_source_ref_realization_view_scale(
	    bsg_database_source_ref_from_scene(ged_draw_shape_source_ref(ref)));
}


fastf_t
ged_draw_scene_ref_realization_curve_scale(bsg_scene_ref ref)
{
    return (fastf_t)bsg_database_source_ref_realization_curve_scale(
	    bsg_database_source_ref_from_scene(ged_draw_shape_source_ref(ref)));
}


fastf_t
ged_draw_scene_ref_realization_point_scale(bsg_scene_ref ref)
{
    return (fastf_t)bsg_database_source_ref_realization_point_scale(
	    bsg_database_source_ref_from_scene(ged_draw_shape_source_ref(ref)));
}


void
ged_draw_scene_ref_realization_set_view_policy(bsg_scene_ref ref,
					       int view_dependent,
					       fastf_t view_scale,
					       size_t bot_threshold,
					       fastf_t curve_scale,
					       fastf_t point_scale)
{
    bsg_scene_ref source_ref = ged_draw_shape_source_ref(ref);
    (void)bsg_database_source_ref_set_realization_view_policy(
	    bsg_database_source_ref_from_scene(source_ref),
	    view_dependent, (double)view_scale, (uint64_t)bot_threshold,
	    (double)curve_scale, (double)point_scale);
    if (!bsg_scene_ref_equal(source_ref, ref))
	(void)bsg_database_source_ref_set_realization_view_policy(
		bsg_database_source_ref_from_scene(ref),
		view_dependent, (double)view_scale, (uint64_t)bot_threshold,
		(double)curve_scale, (double)point_scale);
}


int
ged_draw_scene_ref_realization_pipeline_candidate(bsg_scene_ref ref)
{
    int flags = bsg_database_source_ref_realization_role_flags(
	    bsg_database_source_ref_from_scene(ged_draw_shape_source_ref(ref)));
    return (flags & (BSG_DATABASE_SOURCE_REALIZATION_ROLE_CSG |
		BSG_DATABASE_SOURCE_REALIZATION_ROLE_MESH)) ? 1 : 0;
}


void
ged_draw_scene_ref_realization_reset(bsg_scene_ref ref)
{
    ged_draw_scene_ref_realization_set_current(ref, 0);
    ged_draw_scene_ref_realization_set_roles(ref, 0, 0);
    ged_draw_scene_ref_realization_set_view_policy(ref, 0, 0.0, 0, 0.0, 0.0);
}


void
ged_draw_scene_ref_invalidate(bsg_scene_ref ref)
{
    bsg_scene_invalidate(ref);
}


static int
_ged_draw_scene_ref_publish_adaptive_wireframe(bsg_scene_ref ref,
					       struct rt_db_internal *ip,
					       const struct bn_tol *tol,
					       struct bsg_view *v)
{
    if (!ip || !ip->idb_meth)
	return -1;
    if (bsg_scene_ref_is_null(ref) || !ip->idb_meth->ft_lod_realize)
	return -1;

    struct rt_primitive_lod_realization realization;
    memset(&realization, 0, sizeof(realization));
    int ret = ip->idb_meth->ft_lod_realize(&realization, ip, tol, v,
	    bsg_scene_draw_size(ref));
    if (ret < 0) {
	if (realization.line_points)
	    bu_free(realization.line_points, "primitive LoD line-set points");
	if (realization.line_commands)
	    bu_free(realization.line_commands, "primitive LoD line-set commands");
	return ret;
    }
    if (!realization.has_line_set) {
	if (realization.line_points)
	    bu_free(realization.line_points, "primitive LoD line-set points");
	if (realization.line_commands)
	    bu_free(realization.line_commands, "primitive LoD line-set commands");
	return -1;
    }
    int ok = ged_draw_scene_ref_publish_line_set(ref,
	    (const point_t *)realization.line_points,
	    realization.line_commands, realization.line_count);
    if (realization.line_points)
	bu_free(realization.line_points, "primitive LoD line-set points");
    if (realization.line_commands)
	bu_free(realization.line_commands, "primitive LoD line-set commands");
    return ok ? ret : -1;
}


static void
ged_draw_primitive_realization_line_set_free(struct rt_primitive_lod_realization *realization)
{
    if (!realization)
	return;
    if (realization->line_points)
	bu_free(realization->line_points, "primitive realization line-set points");
    if (realization->line_commands)
	bu_free(realization->line_commands, "primitive realization line-set commands");
    realization->line_points = NULL;
    realization->line_commands = NULL;
    realization->line_count = 0;
    realization->line_capacity = 0;
    realization->has_line_set = 0;
}


struct ged_draw_line_set_buffer {
    point_t *points;
    int *commands;
    size_t count;
    size_t capacity;
};


static void
ged_draw_line_set_buffer_free(struct ged_draw_line_set_buffer *buffer)
{
    if (!buffer)
	return;
    if (buffer->points)
	bu_free(buffer->points, "GED draw dynamic line-set points");
    if (buffer->commands)
	bu_free(buffer->commands, "GED draw dynamic line-set commands");
    buffer->points = NULL;
    buffer->commands = NULL;
    buffer->count = 0;
    buffer->capacity = 0;
}


static void
ged_draw_line_set_buffer_reserve(struct ged_draw_line_set_buffer *buffer,
				 size_t count)
{
    size_t capacity;

    if (!buffer || count <= buffer->capacity)
	return;

    capacity = buffer->capacity ? buffer->capacity : 64;
    while (capacity < count)
	capacity *= 2;

    buffer->points = (point_t *)bu_realloc(buffer->points,
	    capacity * sizeof(point_t), "GED draw dynamic line-set points");
    buffer->commands = (int *)bu_realloc(buffer->commands,
	    capacity * sizeof(int), "GED draw dynamic line-set commands");
    buffer->capacity = capacity;
}


static void
ged_draw_line_set_buffer_append(struct ged_draw_line_set_buffer *buffer,
				const point_t pt,
				int command)
{
    if (!buffer)
	return;
    ged_draw_line_set_buffer_reserve(buffer, buffer->count + 1);
    VMOVE(buffer->points[buffer->count], pt);
    buffer->commands[buffer->count] = command;
    buffer->count++;
}


static int
ged_draw_scene_ref_publish_grip_wireframe_line_set(bsg_scene_ref ref,
						   const struct rt_grip_internal *grip)
{
    vect_t normal;
    point_t center;
    vect_t xbase;
    vect_t ybase;
    vect_t x_1;
    vect_t x_2;
    vect_t y_1;
    vect_t y_2;
    vect_t tip;
    point_t points[11];
    int commands[11];
    size_t idx = 0;

    if (!grip)
	return 0;
    RT_GRIP_CK_MAGIC(grip);

    VMOVE(center, grip->center);
    VMOVE(normal, grip->normal);

    bn_vec_perp(xbase, normal);
    VCROSS(ybase, xbase, normal);

    VUNITIZE(xbase);
    VUNITIZE(ybase);
    VSCALE(xbase, xbase, grip->mag / 4.0);
    VSCALE(ybase, ybase, grip->mag / 4.0);

    VADD2(x_1, center, xbase);
    VSUB2(x_2, center, xbase);
    VADD2(y_1, center, ybase);
    VSUB2(y_2, center, ybase);

    VMOVE(points[idx], x_1);
    commands[idx++] = BSG_GEOMETRY_LINE_MOVE;
    VMOVE(points[idx], y_1);
    commands[idx++] = BSG_GEOMETRY_LINE_DRAW;
    VMOVE(points[idx], x_2);
    commands[idx++] = BSG_GEOMETRY_LINE_DRAW;
    VMOVE(points[idx], y_2);
    commands[idx++] = BSG_GEOMETRY_LINE_DRAW;
    VMOVE(points[idx], x_1);
    commands[idx++] = BSG_GEOMETRY_LINE_DRAW;

    VSCALE(tip, normal, grip->mag);
    VADD2(tip, center, tip);

    VMOVE(points[idx], x_1);
    commands[idx++] = BSG_GEOMETRY_LINE_MOVE;
    VMOVE(points[idx], tip);
    commands[idx++] = BSG_GEOMETRY_LINE_DRAW;
    VMOVE(points[idx], x_2);
    commands[idx++] = BSG_GEOMETRY_LINE_DRAW;
    VMOVE(points[idx], y_1);
    commands[idx++] = BSG_GEOMETRY_LINE_MOVE;
    VMOVE(points[idx], tip);
    commands[idx++] = BSG_GEOMETRY_LINE_DRAW;
    VMOVE(points[idx], y_2);
    commands[idx++] = BSG_GEOMETRY_LINE_DRAW;

    return ged_draw_scene_ref_publish_line_set(ref, (const point_t *)points,
	    commands, idx);
}


static void
ged_draw_line_set_append_16_point_ring(point_t *points,
				       int *commands,
				       size_t *idx,
				       const fastf_t *ring)
{
    VMOVE(points[*idx], &ring[15 * ELEMENTS_PER_VECT]);
    commands[(*idx)++] = BSG_GEOMETRY_LINE_MOVE;
    for (int i = 0; i < 16; i++) {
	VMOVE(points[*idx], &ring[i * ELEMENTS_PER_VECT]);
	commands[(*idx)++] = BSG_GEOMETRY_LINE_DRAW;
    }
}


static void
ged_draw_line_set_append_16_point_connectors(point_t *points,
					     int *commands,
					     size_t *idx,
					     const fastf_t *top,
					     const fastf_t *bottom)
{
    for (int i = 0; i < 16; i += 4) {
	VMOVE(points[*idx], &top[i * ELEMENTS_PER_VECT]);
	commands[(*idx)++] = BSG_GEOMETRY_LINE_MOVE;
	VMOVE(points[*idx], &bottom[i * ELEMENTS_PER_VECT]);
	commands[(*idx)++] = BSG_GEOMETRY_LINE_DRAW;
    }
}


static int
ged_draw_scene_ref_publish_half_wireframe_line_set(bsg_scene_ref ref,
						   const struct rt_half_internal *half)
{
    plane_t eqn;
    vect_t cent;
    vect_t xbase;
    vect_t ybase;
    vect_t x_1;
    vect_t x_2;
    vect_t y_1;
    vect_t y_2;
    vect_t tip;
    point_t points[10];
    int commands[10];
    size_t idx = 0;

    if (!half)
	return 0;
    RT_HALF_CK_MAGIC(half);

    HMOVE(eqn, half->eqn);
    VSCALE(cent, eqn, eqn[W]);

    bn_vec_perp(xbase, &eqn[0]);
    VCROSS(ybase, xbase, eqn);

    VUNITIZE(xbase);
    VUNITIZE(ybase);
    VSCALE(xbase, xbase, 1000);
    VSCALE(ybase, ybase, 1000);

    VADD2(x_1, cent, xbase);
    VSUB2(x_2, cent, xbase);
    VADD2(y_1, cent, ybase);
    VSUB2(y_2, cent, ybase);

    VMOVE(points[idx], x_1);
    commands[idx++] = BSG_GEOMETRY_LINE_MOVE;
    VMOVE(points[idx], x_2);
    commands[idx++] = BSG_GEOMETRY_LINE_DRAW;
    VMOVE(points[idx], y_1);
    commands[idx++] = BSG_GEOMETRY_LINE_MOVE;
    VMOVE(points[idx], y_2);
    commands[idx++] = BSG_GEOMETRY_LINE_DRAW;
    VMOVE(points[idx], x_2);
    commands[idx++] = BSG_GEOMETRY_LINE_DRAW;
    VMOVE(points[idx], y_1);
    commands[idx++] = BSG_GEOMETRY_LINE_DRAW;
    VMOVE(points[idx], x_1);
    commands[idx++] = BSG_GEOMETRY_LINE_DRAW;
    VMOVE(points[idx], y_2);
    commands[idx++] = BSG_GEOMETRY_LINE_DRAW;

    VSCALE(tip, eqn, 500);
    VADD2(tip, cent, tip);
    VMOVE(points[idx], cent);
    commands[idx++] = BSG_GEOMETRY_LINE_MOVE;
    VMOVE(points[idx], tip);
    commands[idx++] = BSG_GEOMETRY_LINE_DRAW;

    return ged_draw_scene_ref_publish_line_set(ref, (const point_t *)points,
	    commands, idx);
}


static int
ged_draw_scene_ref_publish_arb_wireframe_line_set(bsg_scene_ref ref,
						  const struct rt_arb_internal *arb)
{
    static const int contours[4][4] = {
	{0, 1, 2, 3},
	{4, 0, 3, 7},
	{5, 4, 7, 6},
	{1, 5, 6, 2}
    };
    point_t points[16];
    int commands[16];
    size_t idx = 0;

    if (!arb)
	return 0;
    RT_ARB_CK_MAGIC(arb);

    for (int i = 0; i < 4; i++) {
	for (int j = 0; j < 4; j++) {
	    VMOVE(points[idx], arb->pt[contours[i][j]]);
	    commands[idx++] = (j == 0) ? BSG_GEOMETRY_LINE_MOVE :
		BSG_GEOMETRY_LINE_DRAW;
	}
    }

    return ged_draw_scene_ref_publish_line_set(ref, (const point_t *)points,
	    commands, idx);
}


static int
ged_draw_scene_ref_publish_arbn_wireframe_line_set(bsg_scene_ref ref,
						   const struct rt_arbn_internal *aip,
						   const struct bn_tol *tol)
{
    struct ged_draw_line_set_buffer buffer = { NULL, NULL, 0, 0 };
    int ok;

    if (!aip || !tol)
	return 0;
    RT_ARBN_CK_MAGIC(aip);

    for (size_t i = 0; i < aip->neqn - 1; i++) {
	for (size_t j = i + 1; j < aip->neqn; j++) {
	    double dot;
	    int point_count;
	    point_t a;
	    point_t b;
	    vect_t dist;

	    VSETALL(a, 0);
	    VSETALL(b, 0);

	    dot = VDOT(aip->eqn[i], aip->eqn[j]);
	    if (BN_VECT_ARE_PARALLEL(dot, tol))
		continue;

	    point_count = 0;
	    for (size_t k = 0; k < aip->neqn; k++) {
		size_t m;
		point_t pt;
		size_t next_k = 0;

		if (k == i || k == j)
		    continue;
		if (bg_make_pnt_3planes(pt, aip->eqn[i], aip->eqn[j], aip->eqn[k]) < 0)
		    continue;

		for (m = 0; m < aip->neqn; m++) {
		    if (i == m || j == m || k == m)
			continue;
		    if (VDOT(pt, aip->eqn[m])-aip->eqn[m][3] > tol->dist) {
			next_k = 1;
			break;
		    }
		}

		if (next_k != 0)
		    continue;

		if (point_count <= 0) {
		    ged_draw_line_set_buffer_append(&buffer, pt,
			    BSG_GEOMETRY_LINE_MOVE);
		    VMOVE(a, pt);
		} else if (point_count == 1) {
		    VSUB2(dist, pt, a);
		    if (MAGSQ(dist) < tol->dist_sq)
			continue;
		    ged_draw_line_set_buffer_append(&buffer, pt,
			    BSG_GEOMETRY_LINE_DRAW);
		    VMOVE(b, pt);
		} else {
		    VSUB2(dist, pt, a);
		    if (MAGSQ(dist) < tol->dist_sq)
			continue;
		    VSUB2(dist, pt, b);
		    if (MAGSQ(dist) < tol->dist_sq)
			continue;
		    bu_log("ARBN typed wireframe error, point_count=%d (>2) on edge %zu/%zu, non-convex\n",
			    point_count + 1, i, j);
		    VPRINT(" a", a);
		    VPRINT(" b", b);
		    VPRINT("pt", pt);
		    ged_draw_line_set_buffer_append(&buffer, pt,
			    BSG_GEOMETRY_LINE_DRAW);
		}
		point_count++;
	    }
	}
    }

    ok = ged_draw_scene_ref_publish_line_set(ref, (const point_t *)buffer.points,
	    buffer.commands, buffer.count);
    ged_draw_line_set_buffer_free(&buffer);
    return ok;
}


static int
ged_draw_scene_ref_publish_ars_wireframe_line_set(bsg_scene_ref ref,
						  const struct rt_ars_internal *ars)
{
    struct ged_draw_line_set_buffer buffer = { NULL, NULL, 0, 0 };
    int ok;

    if (!ars)
	return 0;
    RT_ARS_CK_MAGIC(ars);

    for (size_t i = 0; i < ars->ncurves; i++) {
	fastf_t *v1 = ars->curves[i];

	ged_draw_line_set_buffer_append(&buffer, v1,
		BSG_GEOMETRY_LINE_MOVE);
	v1 += ELEMENTS_PER_VECT;
	for (size_t j = 1; j <= ars->pts_per_curve; j++, v1 += ELEMENTS_PER_VECT)
	    ged_draw_line_set_buffer_append(&buffer, v1,
		    BSG_GEOMETRY_LINE_DRAW);
    }

    for (size_t i = 0; i < ars->pts_per_curve; i++) {
	ged_draw_line_set_buffer_append(&buffer,
		&ars->curves[0][i * ELEMENTS_PER_VECT],
		BSG_GEOMETRY_LINE_MOVE);
	for (size_t j = 1; j < ars->ncurves; j++)
	    ged_draw_line_set_buffer_append(&buffer,
		    &ars->curves[j][i * ELEMENTS_PER_VECT],
		    BSG_GEOMETRY_LINE_DRAW);
    }

    ok = ged_draw_scene_ref_publish_line_set(ref, (const point_t *)buffer.points,
	    buffer.commands, buffer.count);
    ged_draw_line_set_buffer_free(&buffer);
    return ok;
}


static int
ged_draw_scene_ref_publish_tgc_wireframe_line_set(bsg_scene_ref ref,
						  const struct rt_tgc_internal *tgc)
{
    fastf_t top[16 * ELEMENTS_PER_VECT];
    fastf_t bottom[16 * ELEMENTS_PER_VECT];
    point_t v;
    point_t top_center;
    vect_t a;
    vect_t b;
    vect_t c;
    vect_t d;
    vect_t h;
    point_t points[42];
    int commands[42];
    size_t idx = 0;

    if (!tgc)
	return 0;
    RT_TGC_CK_MAGIC(tgc);

    VMOVE(v, tgc->v);
    VMOVE(a, tgc->a);
    VMOVE(b, tgc->b);
    VMOVE(c, tgc->c);
    VMOVE(d, tgc->d);
    VMOVE(h, tgc->h);

    rt_ell_16pnts(bottom, v, a, b);
    VADD2(top_center, v, h);
    rt_ell_16pnts(top, top_center, c, d);

    ged_draw_line_set_append_16_point_ring(points, commands, &idx, top);
    ged_draw_line_set_append_16_point_ring(points, commands, &idx, bottom);
    ged_draw_line_set_append_16_point_connectors(points, commands, &idx,
	    top, bottom);

    return ged_draw_scene_ref_publish_line_set(ref, (const point_t *)points,
	    commands, idx);
}


static int
ged_draw_scene_ref_publish_cline_wireframe_line_set(bsg_scene_ref ref,
						    const struct rt_cline_internal *cline)
{
    fastf_t top[16 * ELEMENTS_PER_VECT];
    fastf_t bottom[16 * ELEMENTS_PER_VECT];
    point_t center;
    vect_t h;
    point_t top_center;
    vect_t unit_a;
    vect_t unit_b;
    vect_t a;
    vect_t b;
    fastf_t inner_radius;
    point_t points[84];
    int commands[84];
    size_t idx = 0;

    if (!cline)
	return 0;
    RT_CLINE_CK_MAGIC(cline);

    VMOVE(center, cline->v);
    VMOVE(h, cline->h);
    VADD2(top_center, center, h);
    bn_vec_ortho(unit_a, h);
    VCROSS(unit_b, unit_a, h);
    VUNITIZE(unit_b);
    VSCALE(a, unit_a, cline->radius);
    VSCALE(b, unit_b, cline->radius);

    rt_ell_16pnts(bottom, center, a, b);
    rt_ell_16pnts(top, top_center, a, b);
    ged_draw_line_set_append_16_point_ring(points, commands, &idx, top);
    ged_draw_line_set_append_16_point_ring(points, commands, &idx, bottom);
    ged_draw_line_set_append_16_point_connectors(points, commands, &idx,
	    top, bottom);

    if (cline->thickness > 0.0 && cline->thickness < cline->radius) {
	inner_radius = cline->radius - cline->thickness;
	VSCALE(a, unit_a, inner_radius);
	VSCALE(b, unit_b, inner_radius);

	rt_ell_16pnts(bottom, center, a, b);
	rt_ell_16pnts(top, top_center, a, b);
	ged_draw_line_set_append_16_point_ring(points, commands, &idx, top);
	ged_draw_line_set_append_16_point_ring(points, commands, &idx, bottom);
	ged_draw_line_set_append_16_point_connectors(points, commands, &idx,
		top, bottom);
    }

    return ged_draw_scene_ref_publish_line_set(ref, (const point_t *)points,
	    commands, idx);
}


static void
ged_draw_part_hemisphere(point_t ov[13],
			 const fastf_t *v,
			 const fastf_t *a,
			 const fastf_t *b,
			 const fastf_t *h)
{
    VADD2(ov[12], v, h);

    VADD2(ov[0], v, a);
    VJOIN2(ov[1], v, M_SQRT1_2, a, M_SQRT1_2, b);
    VADD2(ov[2], v, b);
    VJOIN2(ov[3], v, -M_SQRT1_2, a, M_SQRT1_2, b);
    VSUB2(ov[4], v, a);
    VJOIN2(ov[5], v, -M_SQRT1_2, a, -M_SQRT1_2, b);
    VSUB2(ov[6], v, b);
    VJOIN2(ov[7], v, M_SQRT1_2, a, -M_SQRT1_2, b);

    VJOIN2(ov[8], v, M_SQRT1_2, a, M_SQRT1_2, h);
    VJOIN2(ov[10], v, -M_SQRT1_2, a, M_SQRT1_2, h);

    VJOIN2(ov[9], v, M_SQRT1_2, b, M_SQRT1_2, h);
    VJOIN2(ov[11], v, -M_SQRT1_2, b, M_SQRT1_2, h);
}


static void
ged_draw_line_set_append_point(point_t *points,
			       int *commands,
			       size_t *idx,
			       const point_t pt,
			       int command)
{
    VMOVE(points[*idx], pt);
    commands[(*idx)++] = command;
}


static void
ged_draw_line_set_append_part_hemisphere_outline(point_t *points,
						 int *commands,
						 size_t *idx,
						 const point_t hemi[13])
{
    ged_draw_line_set_append_point(points, commands, idx, hemi[0],
	    BSG_GEOMETRY_LINE_MOVE);
    for (int i = 7; i >= 0; i--)
	ged_draw_line_set_append_point(points, commands, idx, hemi[i],
		BSG_GEOMETRY_LINE_DRAW);
    ged_draw_line_set_append_point(points, commands, idx, hemi[8],
	    BSG_GEOMETRY_LINE_DRAW);
    ged_draw_line_set_append_point(points, commands, idx, hemi[12],
	    BSG_GEOMETRY_LINE_DRAW);
    ged_draw_line_set_append_point(points, commands, idx, hemi[10],
	    BSG_GEOMETRY_LINE_DRAW);
    ged_draw_line_set_append_point(points, commands, idx, hemi[4],
	    BSG_GEOMETRY_LINE_DRAW);
    ged_draw_line_set_append_point(points, commands, idx, hemi[2],
	    BSG_GEOMETRY_LINE_MOVE);
    ged_draw_line_set_append_point(points, commands, idx, hemi[9],
	    BSG_GEOMETRY_LINE_DRAW);
    ged_draw_line_set_append_point(points, commands, idx, hemi[12],
	    BSG_GEOMETRY_LINE_DRAW);
    ged_draw_line_set_append_point(points, commands, idx, hemi[11],
	    BSG_GEOMETRY_LINE_DRAW);
    ged_draw_line_set_append_point(points, commands, idx, hemi[6],
	    BSG_GEOMETRY_LINE_DRAW);
}


static int
ged_draw_scene_ref_publish_part_wireframe_line_set(bsg_scene_ref ref,
						   const struct rt_part_internal *part)
{
    point_t sphere_rim[16];
    point_t vhemi[13];
    point_t hhemi[13];
    point_t v;
    point_t tail;
    vect_t a;
    vect_t b;
    vect_t c;
    vect_t as;
    vect_t bs;
    vect_t hs;
    vect_t hunit;
    point_t points[51];
    int commands[51];
    size_t idx = 0;

    if (!part)
	return 0;
    RT_PART_CK_MAGIC(part);
    VMOVE(v, part->part_V);

    if (part->part_type == RT_PARTICLE_TYPE_SPHERE) {
	VSET(a, part->part_vrad, 0, 0);
	VSET(b, 0, part->part_vrad, 0);
	VSET(c, 0, 0, part->part_vrad);

	rt_ell_16pnts(&sphere_rim[0][X], v, a, b);
	ged_draw_line_set_append_16_point_ring(points, commands, &idx,
		&sphere_rim[0][X]);

	rt_ell_16pnts(&sphere_rim[0][X], v, b, c);
	ged_draw_line_set_append_16_point_ring(points, commands, &idx,
		&sphere_rim[0][X]);

	rt_ell_16pnts(&sphere_rim[0][X], v, a, c);
	ged_draw_line_set_append_16_point_ring(points, commands, &idx,
		&sphere_rim[0][X]);

	return ged_draw_scene_ref_publish_line_set(ref, (const point_t *)points,
		commands, idx);
    }

    VMOVE(hunit, part->part_H);
    VUNITIZE(hunit);
    bn_vec_perp(a, hunit);
    VUNITIZE(a);
    VCROSS(b, hunit, a);
    VUNITIZE(b);

    VSCALE(as, a, part->part_vrad);
    VSCALE(bs, b, part->part_vrad);
    VSCALE(hs, hunit, -part->part_vrad);
    ged_draw_part_hemisphere(vhemi, v, as, bs, hs);

    VADD2(tail, v, part->part_H);
    VSCALE(as, a, part->part_hrad);
    VSCALE(bs, b, part->part_hrad);
    VSCALE(hs, hunit, part->part_hrad);
    ged_draw_part_hemisphere(hhemi, tail, as, bs, hs);

    ged_draw_line_set_append_part_hemisphere_outline(points, commands, &idx,
	    (const point_t *)vhemi);
    ged_draw_line_set_append_part_hemisphere_outline(points, commands, &idx,
	    (const point_t *)hhemi);

    for (int i = 0; i <= 6; i += 2) {
	ged_draw_line_set_append_point(points, commands, &idx, vhemi[i],
		BSG_GEOMETRY_LINE_MOVE);
	ged_draw_line_set_append_point(points, commands, &idx, hhemi[i],
		BSG_GEOMETRY_LINE_DRAW);
    }

    return ged_draw_scene_ref_publish_line_set(ref, (const point_t *)points,
	    commands, idx);
}


static int
ged_draw_scene_ref_publish_joint_wireframe_line_set(bsg_scene_ref ref,
						    const struct rt_joint_internal *joint)
{
    fastf_t top[16 * ELEMENTS_PER_VECT];
    fastf_t middle[16 * ELEMENTS_PER_VECT];
    fastf_t bottom[16 * ELEMENTS_PER_VECT];
    point_t location;
    point_t a = {5, 0, 0};
    point_t b = {0, 5, 0};
    point_t c = {0, 0, 5};
    point_t points[51];
    int commands[51];
    size_t idx = 0;

    if (!joint)
	return 0;
    RT_JOINT_CK_MAGIC(joint);

    VMOVE(location, joint->location);
    rt_ell_16pnts(top, location, a, b);
    rt_ell_16pnts(bottom, location, b, c);
    rt_ell_16pnts(middle, location, a, c);

    ged_draw_line_set_append_16_point_ring(points, commands, &idx, top);
    ged_draw_line_set_append_16_point_ring(points, commands, &idx, bottom);
    ged_draw_line_set_append_16_point_ring(points, commands, &idx, middle);

    return ged_draw_scene_ref_publish_line_set(ref, (const point_t *)points,
	    commands, idx);
}


static void
ged_draw_line_set_append_sphere_rings(point_t *points,
				      int *commands,
				      size_t *idx,
				      const point_t center,
				      fastf_t radius)
{
    fastf_t top[16 * ELEMENTS_PER_VECT];
    fastf_t middle[16 * ELEMENTS_PER_VECT];
    fastf_t bottom[16 * ELEMENTS_PER_VECT];
    point_t v;
    vect_t a;
    vect_t b;
    vect_t c;

    VMOVE(v, center);
    VSET(a, radius, 0, 0);
    VSET(b, 0, radius, 0);
    VSET(c, 0, 0, radius);

    rt_ell_16pnts(top, v, a, b);
    rt_ell_16pnts(bottom, v, b, c);
    rt_ell_16pnts(middle, v, a, c);

    ged_draw_line_set_append_16_point_ring(points, commands, idx, top);
    ged_draw_line_set_append_16_point_ring(points, commands, idx, bottom);
    ged_draw_line_set_append_16_point_ring(points, commands, idx, middle);
}


static int
ged_draw_scene_ref_publish_metaball_wireframe_line_set(bsg_scene_ref ref,
						       const struct rt_metaball_internal *metaball)
{
    const struct wdb_metaball_pnt *mbpt;
    point_t *points = NULL;
    int *commands = NULL;
    size_t point_count = 0;
    size_t idx = 0;
    int ok;

    if (!metaball)
	return 0;
    RT_METABALL_CK_MAGIC(metaball);

    for (BU_LIST_FOR(mbpt, wdb_metaball_pnt, &metaball->metaball_ctrl_head))
	point_count++;

    if (point_count) {
	points = (point_t *)bu_calloc(point_count * 51, sizeof(point_t),
		"GED draw metaball wireframe points");
	commands = (int *)bu_calloc(point_count * 51, sizeof(int),
		"GED draw metaball wireframe commands");

	for (BU_LIST_FOR(mbpt, wdb_metaball_pnt, &metaball->metaball_ctrl_head)) {
	    point_t center;
	    VMOVE(center, mbpt->coord);
	    ged_draw_line_set_append_sphere_rings(points, commands, &idx,
		    center, mbpt->field_strength / metaball->threshold);
	}
    }

    ok = ged_draw_scene_ref_publish_line_set(ref, (const point_t *)points,
	    commands, idx);

    if (points)
	bu_free(points, "GED draw metaball wireframe points");
    if (commands)
	bu_free(commands, "GED draw metaball wireframe commands");

    return ok;
}


static int
ged_draw_scene_ref_publish_hyp_wireframe_line_set(bsg_scene_ref ref,
						  const struct rt_hyp_internal *hyp_in)
{
    vect_t major_axis[8];
    vect_t minor_axis[8];
    vect_t height_axis[7];
    vect_t bunit;
    vect_t ell[16];
    vect_t ribs[16][7];
    point_t hyp_v;
    vect_t hyp_h;
    vect_t hyp_au;
    fastf_t hyp_r1;
    fastf_t hyp_r2;
    fastf_t hyp_c;
    fastf_t scale;
    fastf_t cos22_5 = 0.9238795325112867385;
    fastf_t cos67_5 = 0.3826834323650898373;
    point_t points[231];
    int commands[231];
    size_t idx = 0;

    if (!hyp_in)
	return 0;
    RT_HYP_CK_MAGIC(hyp_in);

    hyp_r1 = hyp_in->hyp_bnr * MAGNITUDE(hyp_in->hyp_A);
    hyp_r2 = hyp_in->hyp_bnr * hyp_in->hyp_b;
    hyp_c = sqrt(4 * MAGSQ(hyp_in->hyp_A) / MAGSQ(hyp_in->hyp_Hi) *
	    (1 - hyp_in->hyp_bnr * hyp_in->hyp_bnr));
    VSCALE(hyp_h, hyp_in->hyp_Hi, 0.5);
    VADD2(hyp_v, hyp_in->hyp_Vi, hyp_h);
    VMOVE(hyp_au, hyp_in->hyp_A);
    VUNITIZE(hyp_au);

    VCROSS(bunit, hyp_h, hyp_au);
    VUNITIZE(bunit);

    VMOVE(height_axis[0], hyp_h);
    VSCALE(height_axis[1], height_axis[0], 0.5);
    VSCALE(height_axis[2], height_axis[0], 0.25);
    VSETALL(height_axis[3], 0);
    VREVERSE(height_axis[4], height_axis[2]);
    VREVERSE(height_axis[5], height_axis[1]);
    VREVERSE(height_axis[6], height_axis[0]);

    for (int i = 0; i < 7; i++) {
	scale = sqrt(MAGSQ(height_axis[i]) * (hyp_c * hyp_c) /
		(hyp_r1 * hyp_r1) + 1);

	VSCALE(major_axis[0], hyp_au, hyp_r1 * scale);
	VSCALE(major_axis[1], major_axis[0], cos22_5);
	VSCALE(major_axis[2], major_axis[0], M_SQRT1_2);
	VSCALE(major_axis[3], major_axis[0], cos67_5);
	VREVERSE(major_axis[4], major_axis[3]);
	VREVERSE(major_axis[5], major_axis[2]);
	VREVERSE(major_axis[6], major_axis[1]);
	VREVERSE(major_axis[7], major_axis[0]);

	VSCALE(minor_axis[0], bunit, hyp_r2 * scale);
	VSCALE(minor_axis[1], minor_axis[0], cos22_5);
	VSCALE(minor_axis[2], minor_axis[0], M_SQRT1_2);
	VSCALE(minor_axis[3], minor_axis[0], cos67_5);
	VREVERSE(minor_axis[4], minor_axis[3]);
	VREVERSE(minor_axis[5], minor_axis[2]);
	VREVERSE(minor_axis[6], minor_axis[1]);
	VREVERSE(minor_axis[7], minor_axis[0]);

	VADD3(ell[ 0], hyp_v, height_axis[i], major_axis[0]);
	VADD4(ell[ 1], hyp_v, height_axis[i], major_axis[1], minor_axis[3]);
	VADD4(ell[ 2], hyp_v, height_axis[i], major_axis[2], minor_axis[2]);
	VADD4(ell[ 3], hyp_v, height_axis[i], major_axis[3], minor_axis[1]);
	VADD3(ell[ 4], hyp_v, height_axis[i], minor_axis[0]);
	VADD4(ell[ 5], hyp_v, height_axis[i], major_axis[4], minor_axis[1]);
	VADD4(ell[ 6], hyp_v, height_axis[i], major_axis[5], minor_axis[2]);
	VADD4(ell[ 7], hyp_v, height_axis[i], major_axis[6], minor_axis[3]);
	VADD3(ell[ 8], hyp_v, height_axis[i], major_axis[7]);
	VADD4(ell[ 9], hyp_v, height_axis[i], major_axis[6], minor_axis[4]);
	VADD4(ell[10], hyp_v, height_axis[i], major_axis[5], minor_axis[5]);
	VADD4(ell[11], hyp_v, height_axis[i], major_axis[4], minor_axis[6]);
	VADD3(ell[12], hyp_v, height_axis[i], minor_axis[7]);
	VADD4(ell[13], hyp_v, height_axis[i], major_axis[3], minor_axis[6]);
	VADD4(ell[14], hyp_v, height_axis[i], major_axis[2], minor_axis[5]);
	VADD4(ell[15], hyp_v, height_axis[i], major_axis[1], minor_axis[4]);

	ged_draw_line_set_append_point(points, commands, &idx, ell[15],
		BSG_GEOMETRY_LINE_MOVE);
	for (int j = 0; j < 16; j++) {
	    ged_draw_line_set_append_point(points, commands, &idx, ell[j],
		    BSG_GEOMETRY_LINE_DRAW);
	    VMOVE(ribs[j][i], ell[j]);
	}
    }

    for (int i = 0; i < 16; i++) {
	ged_draw_line_set_append_point(points, commands, &idx, ribs[i][0],
		BSG_GEOMETRY_LINE_MOVE);
	for (int j = 1; j < 7; j++)
	    ged_draw_line_set_append_point(points, commands, &idx, ribs[i][j],
		    BSG_GEOMETRY_LINE_DRAW);
    }

    return ged_draw_scene_ref_publish_line_set(ref, (const point_t *)points,
	    commands, idx);
}


static int
ged_draw_scene_ref_publish_pnts_wireframe_line_set(bsg_scene_ref ref,
						   const struct rt_pnts_internal *pnts)
{
    point_t *points = NULL;
    int *commands = NULL;
    size_t point_count;
    size_t command_count;
    size_t idx = 0;
    int ok;

    if (!pnts)
	return 0;
    RT_PNTS_CK_MAGIC(pnts);

    if (pnts->count == 0)
	return ged_draw_scene_ref_publish_line_set(ref, NULL, NULL, 0);

    point_count = (size_t)pnts->count;
    command_count = (pnts->scale > 0) ? point_count * 51 : point_count * 6;
    points = (point_t *)bu_calloc(command_count, sizeof(point_t),
	    "GED draw PNTS wireframe points");
    commands = (int *)bu_calloc(command_count, sizeof(int),
	    "GED draw PNTS wireframe commands");

    struct pnt *point = (struct pnt *)pnts->point;
    struct bu_list *head = &point->l;
    if (pnts->scale > 0) {
	for (BU_LIST_FOR(point, pnt, head)) {
	    point_t center;
	    VMOVE(center, point->v);
	    ged_draw_line_set_append_sphere_rings(points, commands, &idx,
		    center, pnts->scale);
	}
    } else {
	for (BU_LIST_FOR(point, pnt, head)) {
	    point_t a;
	    point_t b;
	    double vcoord = 1;
	    double hcoord = 1;

	    VSET(a, point->v[X] - hcoord, point->v[Y], point->v[Z]);
	    VSET(b, point->v[X] + hcoord, point->v[Y], point->v[Z]);
	    ged_draw_line_set_append_point(points, commands, &idx, a,
		    BSG_GEOMETRY_LINE_MOVE);
	    ged_draw_line_set_append_point(points, commands, &idx, b,
		    BSG_GEOMETRY_LINE_DRAW);

	    VSET(a, point->v[X], point->v[Y] - hcoord, point->v[Z]);
	    VSET(b, point->v[X], point->v[Y] + hcoord, point->v[Z]);
	    ged_draw_line_set_append_point(points, commands, &idx, a,
		    BSG_GEOMETRY_LINE_MOVE);
	    ged_draw_line_set_append_point(points, commands, &idx, b,
		    BSG_GEOMETRY_LINE_DRAW);

	    VSET(a, point->v[X], point->v[Y], point->v[Z] - vcoord);
	    VSET(b, point->v[X], point->v[Y], point->v[Z] + vcoord);
	    ged_draw_line_set_append_point(points, commands, &idx, a,
		    BSG_GEOMETRY_LINE_MOVE);
	    ged_draw_line_set_append_point(points, commands, &idx, b,
		    BSG_GEOMETRY_LINE_DRAW);
	}
    }

    ok = ged_draw_scene_ref_publish_line_set(ref, (const point_t *)points,
	    commands, idx);

    bu_free(points, "GED draw PNTS wireframe points");
    bu_free(commands, "GED draw PNTS wireframe commands");

    return ok;
}


static int
ged_draw_scene_ref_publish_script_wireframe_line_set(bsg_scene_ref ref,
						     const struct rt_script_internal *script)
{
    if (!script)
	return 0;
    RT_SCRIPT_CK_MAGIC(script);

    if (bu_vls_addr(&script->s_type))
	bu_log("Script data not found or not specified\n");

    return ged_draw_scene_ref_publish_line_set(ref, NULL, NULL, 0);
}


static int
ged_draw_scene_ref_publish_pipe_wireframe_line_set(bsg_scene_ref ref,
						   struct rt_db_internal *ip)
{
    struct rt_primitive_lod_realization realization = { 0 };
    int ret = rt_pipe_wireframe_line_set(&realization, ip);

    if (ret < 0 || !realization.has_line_set) {
	ged_draw_primitive_realization_line_set_free(&realization);
	return 0;
    }

    int ok = ged_draw_scene_ref_publish_line_set(ref,
	    (const point_t *)realization.line_points,
	    realization.line_commands, realization.line_count);
    ged_draw_primitive_realization_line_set_free(&realization);
    return ok;
}


static int
ged_draw_scene_ref_publish_tor_wireframe_line_set(bsg_scene_ref ref,
						  struct rt_db_internal *ip,
						  const struct bg_tess_tol *ttol)
{
    struct rt_primitive_lod_realization realization = { 0 };
    int ret = rt_tor_wireframe_line_set(&realization, ip, ttol);

    if (ret < 0 || !realization.has_line_set) {
	ged_draw_primitive_realization_line_set_free(&realization);
	return 0;
    }

    int ok = ged_draw_scene_ref_publish_line_set(ref,
	    (const point_t *)realization.line_points,
	    realization.line_commands, realization.line_count);
    ged_draw_primitive_realization_line_set_free(&realization);
    return ok;
}


static int
ged_draw_scene_ref_publish_rpc_wireframe_line_set(bsg_scene_ref ref,
						  struct rt_db_internal *ip,
						  const struct bg_tess_tol *ttol)
{
    struct rt_primitive_lod_realization realization = { 0 };
    int ret = rt_rpc_wireframe_line_set(&realization, ip, ttol);

    if (ret < 0 || !realization.has_line_set) {
	ged_draw_primitive_realization_line_set_free(&realization);
	return 0;
    }

    int ok = ged_draw_scene_ref_publish_line_set(ref,
	    (const point_t *)realization.line_points,
	    realization.line_commands, realization.line_count);
    ged_draw_primitive_realization_line_set_free(&realization);
    return ok;
}


static int
ged_draw_scene_ref_publish_rhc_wireframe_line_set(bsg_scene_ref ref,
						  struct rt_db_internal *ip,
						  const struct bg_tess_tol *ttol)
{
    struct rt_primitive_lod_realization realization = { 0 };
    int ret = rt_rhc_wireframe_line_set(&realization, ip, ttol);

    if (ret < 0 || !realization.has_line_set) {
	ged_draw_primitive_realization_line_set_free(&realization);
	return 0;
    }

    int ok = ged_draw_scene_ref_publish_line_set(ref,
	    (const point_t *)realization.line_points,
	    realization.line_commands, realization.line_count);
    ged_draw_primitive_realization_line_set_free(&realization);
    return ok;
}


static int
ged_draw_scene_ref_publish_epa_wireframe_line_set(bsg_scene_ref ref,
						  struct rt_db_internal *ip,
						  const struct bg_tess_tol *ttol)
{
    struct rt_primitive_lod_realization realization = { 0 };
    int ret = rt_epa_wireframe_line_set(&realization, ip, ttol);

    if (ret < 0 || !realization.has_line_set) {
	ged_draw_primitive_realization_line_set_free(&realization);
	return 0;
    }

    int ok = ged_draw_scene_ref_publish_line_set(ref,
	    (const point_t *)realization.line_points,
	    realization.line_commands, realization.line_count);
    ged_draw_primitive_realization_line_set_free(&realization);
    return ok;
}


static int
ged_draw_scene_ref_publish_ehy_wireframe_line_set(bsg_scene_ref ref,
						  struct rt_db_internal *ip,
						  const struct bg_tess_tol *ttol)
{
    struct rt_primitive_lod_realization realization = { 0 };
    int ret = rt_ehy_wireframe_line_set(&realization, ip, ttol);

    if (ret < 0 || !realization.has_line_set) {
	ged_draw_primitive_realization_line_set_free(&realization);
	return 0;
    }

    int ok = ged_draw_scene_ref_publish_line_set(ref,
	    (const point_t *)realization.line_points,
	    realization.line_commands, realization.line_count);
    ged_draw_primitive_realization_line_set_free(&realization);
    return ok;
}


static int
ged_draw_scene_ref_publish_eto_wireframe_line_set(bsg_scene_ref ref,
						  struct rt_db_internal *ip,
						  const struct bg_tess_tol *ttol)
{
    struct rt_primitive_lod_realization realization = { 0 };
    int ret = rt_eto_wireframe_line_set(&realization, ip, ttol);

    if (ret < 0 || !realization.has_line_set) {
	ged_draw_primitive_realization_line_set_free(&realization);
	return 0;
    }

    int ok = ged_draw_scene_ref_publish_line_set(ref,
	    (const point_t *)realization.line_points,
	    realization.line_commands, realization.line_count);
    ged_draw_primitive_realization_line_set_free(&realization);
    return ok;
}


static int
ged_draw_scene_ref_publish_superell_wireframe_line_set(bsg_scene_ref ref,
						       struct rt_db_internal *ip)
{
    struct rt_primitive_lod_realization realization = { 0 };
    int ret = rt_superell_wireframe_line_set(&realization, ip);

    if (ret < 0 || !realization.has_line_set) {
	ged_draw_primitive_realization_line_set_free(&realization);
	return 0;
    }

    int ok = ged_draw_scene_ref_publish_line_set(ref,
	    (const point_t *)realization.line_points,
	    realization.line_commands, realization.line_count);
    ged_draw_primitive_realization_line_set_free(&realization);
    return ok;
}


static int
ged_draw_scene_ref_publish_hrt_wireframe_line_set(bsg_scene_ref ref,
						  struct rt_db_internal *ip,
						  const struct bg_tess_tol *ttol)
{
    struct rt_primitive_lod_realization realization = { 0 };
    int ret = rt_hrt_wireframe_line_set(&realization, ip, ttol);

    if (ret < 0 || !realization.has_line_set) {
	ged_draw_primitive_realization_line_set_free(&realization);
	return 0;
    }

    int ok = ged_draw_scene_ref_publish_line_set(ref,
	    (const point_t *)realization.line_points,
	    realization.line_commands, realization.line_count);
    ged_draw_primitive_realization_line_set_free(&realization);
    return ok;
}


static int
ged_draw_scene_ref_publish_datum_wireframe_line_set(bsg_scene_ref ref,
						    struct rt_db_internal *ip)
{
    struct rt_primitive_lod_realization realization = { 0 };
    int ret = rt_datum_wireframe_line_set(&realization, ip);

    if (ret < 0 || !realization.has_line_set) {
	ged_draw_primitive_realization_line_set_free(&realization);
	return 0;
    }

    int ok = ged_draw_scene_ref_publish_line_set(ref,
	    (const point_t *)realization.line_points,
	    realization.line_commands, realization.line_count);
    ged_draw_primitive_realization_line_set_free(&realization);
    return ok;
}


static int
ged_draw_scene_ref_publish_sketch_wireframe_line_set(bsg_scene_ref ref,
						     struct rt_db_internal *ip,
						     const struct bg_tess_tol *ttol)
{
    struct rt_primitive_lod_realization realization = { 0 };
    int ret = rt_sketch_wireframe_line_set(&realization, ip, ttol);

    if (ret < 0 || !realization.has_line_set) {
	ged_draw_primitive_realization_line_set_free(&realization);
	return 0;
    }

    int ok = ged_draw_scene_ref_publish_line_set(ref,
	    (const point_t *)realization.line_points,
	    realization.line_commands, realization.line_count);
    ged_draw_primitive_realization_line_set_free(&realization);
    return ok;
}


static int
ged_draw_scene_ref_publish_extrude_wireframe_line_set(bsg_scene_ref ref,
						      struct rt_db_internal *ip,
						      const struct bg_tess_tol *ttol)
{
    struct rt_primitive_lod_realization realization = { 0 };
    int ret = rt_extrude_wireframe_line_set(&realization, ip, ttol);

    if (ret < 0 || !realization.has_line_set) {
	ged_draw_primitive_realization_line_set_free(&realization);
	return 0;
    }

    int ok = ged_draw_scene_ref_publish_line_set(ref,
	    (const point_t *)realization.line_points,
	    realization.line_commands, realization.line_count);
    ged_draw_primitive_realization_line_set_free(&realization);
    return ok;
}


static int
ged_draw_scene_ref_publish_revolve_wireframe_line_set(bsg_scene_ref ref,
						      struct rt_db_internal *ip,
						      const struct bg_tess_tol *ttol)
{
    struct rt_primitive_lod_realization realization = { 0 };
    int ret = rt_revolve_wireframe_line_set(&realization, ip, ttol);

    if (ret < 0 || !realization.has_line_set) {
	ged_draw_primitive_realization_line_set_free(&realization);
	return 0;
    }

    int ok = ged_draw_scene_ref_publish_line_set(ref,
	    (const point_t *)realization.line_points,
	    realization.line_commands, realization.line_count);
    ged_draw_primitive_realization_line_set_free(&realization);
    return ok;
}


static int
ged_draw_scene_ref_publish_ebm_wireframe_line_set(bsg_scene_ref ref,
						  struct rt_db_internal *ip)
{
    struct rt_primitive_lod_realization realization = { 0 };
    int ret = rt_ebm_wireframe_line_set(&realization, ip);

    if (ret < 0 || !realization.has_line_set) {
	ged_draw_primitive_realization_line_set_free(&realization);
	return 0;
    }

    int ok = ged_draw_scene_ref_publish_line_set(ref,
	    (const point_t *)realization.line_points,
	    realization.line_commands, realization.line_count);
    ged_draw_primitive_realization_line_set_free(&realization);
    return ok;
}


static int
ged_draw_scene_ref_publish_vol_wireframe_line_set(bsg_scene_ref ref,
						  struct rt_db_internal *ip)
{
    struct rt_primitive_lod_realization realization = { 0 };
    int ret = rt_vol_wireframe_line_set(&realization, ip);

    if (ret < 0 || !realization.has_line_set) {
	ged_draw_primitive_realization_line_set_free(&realization);
	return 0;
    }

    int ok = ged_draw_scene_ref_publish_line_set(ref,
	    (const point_t *)realization.line_points,
	    realization.line_commands, realization.line_count);
    ged_draw_primitive_realization_line_set_free(&realization);
    return ok;
}


static int
ged_draw_scene_ref_publish_dsp_wireframe_line_set(bsg_scene_ref ref,
						  struct rt_db_internal *ip,
						  const struct bg_tess_tol *ttol)
{
    struct rt_primitive_lod_realization realization = { 0 };
    int ret = rt_dsp_wireframe_line_set(&realization, ip, ttol);

    if (ret < 0 || !realization.has_line_set) {
	ged_draw_primitive_realization_line_set_free(&realization);
	return 0;
    }

    int ok = ged_draw_scene_ref_publish_line_set(ref,
	    (const point_t *)realization.line_points,
	    realization.line_commands, realization.line_count);
    ged_draw_primitive_realization_line_set_free(&realization);
    return ok;
}


static int
ged_draw_scene_ref_publish_hf_wireframe_line_set(bsg_scene_ref ref,
						 struct rt_db_internal *ip,
						 const struct bg_tess_tol *ttol)
{
    struct rt_primitive_lod_realization realization = { 0 };
    int ret = rt_hf_wireframe_line_set(&realization, ip, ttol);

    if (ret < 0 || !realization.has_line_set) {
	ged_draw_primitive_realization_line_set_free(&realization);
	return 0;
    }

    int ok = ged_draw_scene_ref_publish_line_set(ref,
	    (const point_t *)realization.line_points,
	    realization.line_commands, realization.line_count);
    ged_draw_primitive_realization_line_set_free(&realization);
    return ok;
}


static int
ged_draw_scene_ref_publish_nmg_wireframe_line_set(bsg_scene_ref ref,
						  struct rt_db_internal *ip)
{
    if (!ip || ip->idb_type != ID_NMG || !ip->idb_ptr)
	return 0;

    return ged_draw_scene_ref_geometry_publish_nmg_model(ref,
	    (const struct model *)ip->idb_ptr, GED_DRAW_NMG_STYLE_VECTOR);
}


static int
ged_draw_scene_ref_publish_ell_wireframe_line_set(bsg_scene_ref ref,
						  const struct rt_ell_internal *ell)
{
    fastf_t top[16 * ELEMENTS_PER_VECT];
    fastf_t middle[16 * ELEMENTS_PER_VECT];
    fastf_t bottom[16 * ELEMENTS_PER_VECT];
    point_t v;
    vect_t a;
    vect_t b;
    vect_t c;
    point_t points[51];
    int commands[51];
    size_t idx = 0;

    if (!ell)
	return 0;
    RT_ELL_CK_MAGIC(ell);

    VMOVE(v, ell->v);
    VMOVE(a, ell->a);
    VMOVE(b, ell->b);
    VMOVE(c, ell->c);

    rt_ell_16pnts(top, v, a, b);
    rt_ell_16pnts(bottom, v, b, c);
    rt_ell_16pnts(middle, v, a, c);

    ged_draw_line_set_append_16_point_ring(points, commands, &idx, top);
    ged_draw_line_set_append_16_point_ring(points, commands, &idx, bottom);
    ged_draw_line_set_append_16_point_ring(points, commands, &idx, middle);

    return ged_draw_scene_ref_publish_line_set(ref, (const point_t *)points,
	    commands, idx);
}

static int
ged_draw_scene_ref_publish_annot_record(bsg_scene_ref ref,
					struct rt_db_internal *ip)
{
    if (!ip || ip->idb_type != ID_ANNOT || !ip->idb_ptr)
	return 0;

    ged_draw_shape_state *shape_data = ged_draw_shape_state_get_scene_ref(ref);
    if (!shape_data)
	return 0;

    struct rt_annot_internal *ann = (struct rt_annot_internal *)ip->idb_ptr;
    RT_ANNOT_CK_MAGIC(ann);

    point_t *points = NULL;
    struct bsg_annotation_segment *segments = NULL;
    struct bu_vls summary = BU_VLS_INIT_ZERO;
    mat_t model_mat, display_mat;
    int ok = 0;

    MAT_IDN(model_mat);
    MAT_IDN(display_mat);

    if (ann->vert_count) {
	points = (point_t *)bu_calloc(ann->vert_count, sizeof(point_t),
		"GED ANNOT points");
	for (size_t i = 0; i < ann->vert_count; i++)
	    VSET(points[i], ann->verts[i][X], ann->verts[i][Y], 0.0);
    }

    if (ann->ant.count) {
	segments = (struct bsg_annotation_segment *)bu_calloc(ann->ant.count,
		sizeof(struct bsg_annotation_segment), "GED ANNOT segments");
	for (size_t i = 0; i < ann->ant.count; i++) {
	    uint32_t *lng = ann->ant.segments ? (uint32_t *)ann->ant.segments[i] : NULL;
	    if (!lng)
		continue;
	    segments[i].reverse = ann->ant.reverse ? ann->ant.reverse[i] : 0;
	    switch (*lng) {
		case CURVE_LSEG_MAGIC: {
		    const struct line_seg *lsg = (const struct line_seg *)lng;
		    segments[i].kind = BSG_ANNOTATION_SEGMENT_LINE;
		    segments[i].data.line.start = lsg->start;
		    segments[i].data.line.end = lsg->end;
		    break;
		}
		case CURVE_CARC_MAGIC: {
		    const struct carc_seg *csg = (const struct carc_seg *)lng;
		    segments[i].kind = BSG_ANNOTATION_SEGMENT_CARC;
		    segments[i].data.carc.start = csg->start;
		    segments[i].data.carc.end = csg->end;
		    segments[i].data.carc.radius = csg->radius;
		    segments[i].data.carc.center_is_left = csg->center_is_left;
		    segments[i].data.carc.orientation = csg->orientation;
		    segments[i].data.carc.center = csg->center;
		    break;
		}
		case CURVE_NURB_MAGIC: {
		    const struct nurb_seg *nsg = (const struct nurb_seg *)lng;
		    segments[i].kind = BSG_ANNOTATION_SEGMENT_NURB;
		    segments[i].data.nurb.order = nsg->order;
		    segments[i].data.nurb.point_type = nsg->pt_type;
		    segments[i].data.nurb.knot_count = (nsg->k.k_size > 0) ?
			(size_t)nsg->k.k_size : 0;
		    segments[i].data.nurb.knots = nsg->k.knots;
		    segments[i].data.nurb.control_point_count = (nsg->c_size > 0) ?
			(size_t)nsg->c_size : 0;
		    segments[i].data.nurb.control_points = nsg->ctl_points;
		    segments[i].data.nurb.weights = nsg->weights;
		    break;
		}
		case CURVE_BEZIER_MAGIC: {
		    const struct bezier_seg *bsg = (const struct bezier_seg *)lng;
		    segments[i].kind = BSG_ANNOTATION_SEGMENT_BEZIER;
		    segments[i].data.bezier.degree = bsg->degree;
		    segments[i].data.bezier.control_point_count = (bsg->degree >= 0) ?
			(size_t)bsg->degree + 1 : 0;
		    segments[i].data.bezier.control_points = bsg->ctl_points;
		    break;
		}
		case ANN_TSEG_MAGIC: {
		    const struct txt_seg *tsg = (const struct txt_seg *)lng;
		    const char *label = BU_VLS_IS_INITIALIZED(&tsg->label) ?
			bu_vls_cstr(&tsg->label) : "";
		    segments[i].kind = BSG_ANNOTATION_SEGMENT_TEXT;
		    segments[i].data.text.ref_pt = tsg->ref_pt;
		    segments[i].data.text.relative_position = tsg->rel_pos;
		    segments[i].data.text.text = (char *)label;
		    segments[i].data.text.size = tsg->txt_size;
		    segments[i].data.text.rotation = tsg->txt_rot_angle;
		    if (label[0]) {
			if (bu_vls_strlen(&summary))
			    bu_vls_strcat(&summary, " ");
			bu_vls_strcat(&summary, label);
		    }
		    break;
		}
		default:
		    break;
	    }
	}
    }

    if (!bu_vls_strlen(&summary))
	bu_vls_strcpy(&summary, "annotation");

    ok = bsg_geometry_ref_set_annotation(bsg_scene_ref_as_geometry(ref),
	    bu_vls_cstr(&summary), BSG_ANNOTATION_SPACE_DISPLAY, ann->V,
	    model_mat, display_mat, (const point_t *)points, ann->vert_count,
	    segments, ann->ant.count);
    if (ok) {
	shape_data->geometry_command_count = ann->ant.count;
	shape_data->geometry_revision++;
	bsg_scene_invalidate(ref);
    }

    if (points)
	bu_free(points, "GED ANNOT points");
    if (segments)
	bu_free(segments, "GED ANNOT segments");
    bu_vls_free(&summary);
    return ok;
}


static int
_ged_draw_scene_ref_publish_direct_primitive_wireframe(bsg_scene_ref ref,
						       struct rt_db_internal *ip,
						       const struct bg_tess_tol *ttol,
						       const struct bn_tol *tol,
						       struct bsg_view *v)
{
    int ok = 0;

    if (!ip)
	return 0;

    switch (ip->idb_type) {
	case ID_BOT:
	    ok = ged_draw_scene_ref_publish_bot_wireframe_line_set(ref,
		    (const struct rt_bot_internal *)ip->idb_ptr);
	    break;
	case ID_POLY:
	    ok = ged_draw_scene_ref_publish_poly_wireframe_line_set(ref,
		    (const struct rt_pg_internal *)ip->idb_ptr);
	    break;
	case ID_BREP:
	    ok = ged_draw_scene_ref_publish_brep_wireframe_line_set(ref,
		    (const struct rt_brep_internal *)ip->idb_ptr, tol);
	    break;
	case ID_BSPLINE:
	    ok = ged_draw_scene_ref_publish_bspline_wireframe_line_set(ref, ip,
		    tol);
	    break;
	case ID_TOR:
	    ok = ged_draw_scene_ref_publish_tor_wireframe_line_set(ref, ip, ttol);
	    break;
	case ID_RPC:
	    ok = ged_draw_scene_ref_publish_rpc_wireframe_line_set(ref, ip, ttol);
	    break;
	case ID_RHC:
	    ok = ged_draw_scene_ref_publish_rhc_wireframe_line_set(ref, ip, ttol);
	    break;
	case ID_EPA:
	    ok = ged_draw_scene_ref_publish_epa_wireframe_line_set(ref, ip, ttol);
	    break;
	case ID_EHY:
	    ok = ged_draw_scene_ref_publish_ehy_wireframe_line_set(ref, ip, ttol);
	    break;
	case ID_ETO:
	    ok = ged_draw_scene_ref_publish_eto_wireframe_line_set(ref, ip, ttol);
	    break;
	case ID_SUPERELL:
	    ok = ged_draw_scene_ref_publish_superell_wireframe_line_set(ref, ip);
	    break;
	case ID_HRT:
	    ok = ged_draw_scene_ref_publish_hrt_wireframe_line_set(ref, ip, ttol);
	    break;
	case ID_DATUM:
	    ok = ged_draw_scene_ref_publish_datum_wireframe_line_set(ref, ip);
	    break;
	case ID_SKETCH:
	    ok = ged_draw_scene_ref_publish_sketch_wireframe_line_set(ref, ip,
		    ttol);
	    break;
	case ID_EXTRUDE:
	    ok = ged_draw_scene_ref_publish_extrude_wireframe_line_set(ref, ip,
		    ttol);
	    break;
	case ID_REVOLVE:
	    ok = ged_draw_scene_ref_publish_revolve_wireframe_line_set(ref, ip,
		    ttol);
	    break;
	case ID_EBM:
	    ok = ged_draw_scene_ref_publish_ebm_wireframe_line_set(ref, ip);
	    break;
	case ID_VOL:
	    ok = ged_draw_scene_ref_publish_vol_wireframe_line_set(ref, ip);
	    break;
	case ID_DSP:
	    ok = ged_draw_scene_ref_publish_dsp_wireframe_line_set(ref, ip,
		    ttol);
	    break;
	case ID_HF:
	    ok = ged_draw_scene_ref_publish_hf_wireframe_line_set(ref, ip,
		    ttol);
	    break;
	case ID_NMG:
	    ok = ged_draw_scene_ref_publish_nmg_wireframe_line_set(ref, ip);
	    break;
	case ID_SUBMODEL:
	    ok = ged_draw_scene_ref_publish_submodel_wireframe_children(ref, ip,
		    ttol, tol, v);
	    break;
	case ID_ANNOT:
	    ok = ged_draw_scene_ref_publish_annot_record(ref, ip);
	    break;
	case ID_ELL:
	case ID_SPH:
	    ok = ged_draw_scene_ref_publish_ell_wireframe_line_set(ref,
		    (const struct rt_ell_internal *)ip->idb_ptr);
	    break;
	case ID_ARB8:
	    ok = ged_draw_scene_ref_publish_arb_wireframe_line_set(ref,
		    (const struct rt_arb_internal *)ip->idb_ptr);
	    break;
	case ID_ARBN:
	    ok = ged_draw_scene_ref_publish_arbn_wireframe_line_set(ref,
		    (const struct rt_arbn_internal *)ip->idb_ptr, tol);
	    break;
	case ID_ARS:
	    ok = ged_draw_scene_ref_publish_ars_wireframe_line_set(ref,
		    (const struct rt_ars_internal *)ip->idb_ptr);
	    break;
	case ID_GRIP:
	    ok = ged_draw_scene_ref_publish_grip_wireframe_line_set(ref,
		    (const struct rt_grip_internal *)ip->idb_ptr);
	    break;
	case ID_HALF:
	    ok = ged_draw_scene_ref_publish_half_wireframe_line_set(ref,
		    (const struct rt_half_internal *)ip->idb_ptr);
	    break;
	case ID_CLINE:
	    ok = ged_draw_scene_ref_publish_cline_wireframe_line_set(ref,
		    (const struct rt_cline_internal *)ip->idb_ptr);
	    break;
	case ID_PARTICLE:
	    ok = ged_draw_scene_ref_publish_part_wireframe_line_set(ref,
		    (const struct rt_part_internal *)ip->idb_ptr);
	    break;
	case ID_JOINT:
	    ok = ged_draw_scene_ref_publish_joint_wireframe_line_set(ref,
		    (const struct rt_joint_internal *)ip->idb_ptr);
	    break;
	case ID_METABALL:
	    ok = ged_draw_scene_ref_publish_metaball_wireframe_line_set(ref,
		    (const struct rt_metaball_internal *)ip->idb_ptr);
	    break;
	case ID_HYP:
	    ok = ged_draw_scene_ref_publish_hyp_wireframe_line_set(ref,
		    (const struct rt_hyp_internal *)ip->idb_ptr);
	    break;
	case ID_PNTS:
	    ok = ged_draw_scene_ref_publish_pnts_wireframe_line_set(ref,
		    (const struct rt_pnts_internal *)ip->idb_ptr);
	    break;
	case ID_PIPE:
	    ok = ged_draw_scene_ref_publish_pipe_wireframe_line_set(ref, ip);
	    break;
	case ID_SCRIPT:
	    ok = ged_draw_scene_ref_publish_script_wireframe_line_set(ref,
		    (const struct rt_script_internal *)ip->idb_ptr);
	    break;
	case ID_TGC:
	case ID_REC:
	    ok = ged_draw_scene_ref_publish_tgc_wireframe_line_set(ref,
		    (const struct rt_tgc_internal *)ip->idb_ptr);
	    break;
	default:
	    return 0;
    }

    return ok ? 1 : -1;
}


int
ged_draw_scene_ref_publish_primitive_wireframe(bsg_scene_ref ref,
					       struct rt_db_internal *ip,
					       const struct bg_tess_tol *ttol,
					       const struct bn_tol *tol,
					       struct bsg_view *v,
					       int adaptive)
{
    if (!adaptive || (ip && (ip->idb_type == ID_SUBMODEL ||
		    ip->idb_type == ID_ANNOT))) {
	int direct = _ged_draw_scene_ref_publish_direct_primitive_wireframe(
		ref, ip, ttol, tol, v);
	if (direct > 0)
	    return 0;
	if (direct < 0)
	    return -1;
    }

    if (adaptive)
	return _ged_draw_scene_ref_publish_adaptive_wireframe(ref, ip, tol, v);

    return -1;
}


int
ged_draw_scene_ref_publish_line_set(bsg_scene_ref ref,
				    const point_t *points,
				    const int *commands,
				    size_t point_count)
{
    ged_draw_shape_state *shape_data = ged_draw_shape_state_get_scene_ref(ref);

    if (!shape_data)
	return 0;
    if (point_count && !points)
	return 0;
    if (!bsg_geometry_ref_set_line_set(bsg_scene_ref_as_geometry(ref),
	    points, commands, point_count))
	return 0;

    shape_data->geometry_command_count = point_count;
    shape_data->geometry_revision++;
    bsg_scene_invalidate(ref);
    return 1;
}


int
ged_draw_scene_ref_publish_indexed_face_set(bsg_scene_ref ref,
					    const point_t *points,
					    size_t point_count,
					    const vect_t *normals,
					    size_t normal_count,
					    const int *indices,
					    size_t index_count)
{
    ged_draw_shape_state *shape_data = ged_draw_shape_state_get_scene_ref(ref);

    if (!shape_data)
	return 0;
    if (!point_count || !index_count)
	return ged_draw_scene_ref_geometry_clear(ref);
    if (!points || !indices)
	return 0;

    int ok = bsg_geometry_ref_set_indexed_face_set(bsg_scene_ref_as_geometry(ref),
	    points, point_count, normals, normal_count, indices, index_count);
    if (!ok)
	return 0;

    shape_data->geometry_command_count = point_count;
    shape_data->geometry_revision++;
    bsg_scene_invalidate(ref);
    return 1;
}


static void
_ged_draw_primitive_indexed_face_set_free(struct rt_primitive_indexed_face_set *face_set)
{
    if (!face_set)
	return;
    if (face_set->points)
	bu_free(face_set->points, "primitive indexed-face points");
    if (face_set->normals)
	bu_free(face_set->normals, "primitive indexed-face normals");
    if (face_set->indices)
	bu_free(face_set->indices, "primitive indexed-face indices");
    memset(face_set, 0, sizeof(*face_set));
}


int
ged_draw_scene_ref_publish_primitive_face_set(bsg_scene_ref ref,
					     struct rt_db_internal *ip,
					     const struct bg_tess_tol *ttol,
					     const struct bn_tol *tol,
					     const struct bsg_view *v)
{
    struct rt_primitive_indexed_face_set face_set;
    int ret;
    int ok;

    memset(&face_set, 0, sizeof(face_set));

    if (bsg_scene_ref_is_null(ref) || !ip || !ip->idb_meth ||
	    !ip->idb_meth->ft_indexed_face_set)
	return 0;

    ret = ip->idb_meth->ft_indexed_face_set(&face_set, ip, ttol, tol, v);
    if (ret != BRLCAD_OK) {
	_ged_draw_primitive_indexed_face_set_free(&face_set);
	return 0;
    }

    ok = face_set.point_count || face_set.index_count ?
	ged_draw_scene_ref_publish_indexed_face_set(ref,
		(const point_t *)face_set.points, face_set.point_count,
		(const vect_t *)face_set.normals, face_set.normal_count,
		face_set.indices, face_set.index_count) :
	ged_draw_scene_ref_geometry_clear(ref);

    _ged_draw_primitive_indexed_face_set_free(&face_set);
    return ok;
}


static int
_ged_draw_bot_face_order(const struct rt_bot_internal *bot,
			 size_t face_idx,
			 int vertex_order[3],
			 int normal_order[3])
{
    const int *face;

    if (!bot || !bot->faces || !vertex_order || !normal_order)
	return 0;

    face = &bot->faces[face_idx * 3];
    for (int i = 0; i < 3; i++) {
	if (face[i] < 0 || (size_t)face[i] >= bot->num_vertices)
	    return 0;
    }

    vertex_order[0] = face[0];
    normal_order[0] = 0;
    if (bot->orientation == RT_BOT_CW) {
	vertex_order[1] = face[2];
	vertex_order[2] = face[1];
	normal_order[1] = 2;
	normal_order[2] = 1;
    } else {
	vertex_order[1] = face[1];
	vertex_order[2] = face[2];
	normal_order[1] = 1;
	normal_order[2] = 2;
    }

    return 1;
}


int
ged_draw_scene_ref_publish_bot_wireframe_line_set(bsg_scene_ref ref,
						  const struct rt_bot_internal *bot)
{
    point_t *points = NULL;
    int *commands = NULL;
    size_t valid_faces = 0;
    size_t idx = 0;
    int ok;

    if (!bot)
	return 0;

    RT_BOT_CK_MAGIC(bot);
    if (!bot->vertices || !bot->faces || !bot->num_vertices || !bot->num_faces)
	return ged_draw_scene_ref_geometry_clear(ref);

    for (size_t i = 0; i < bot->num_faces; i++) {
	int vertex_order[3];
	int normal_order[3];
	if (_ged_draw_bot_face_order(bot, i, vertex_order, normal_order))
	    valid_faces++;
    }

    if (!valid_faces)
	return ged_draw_scene_ref_geometry_clear(ref);
    if (valid_faces > ((size_t)-1) / 4)
	return 0;

    size_t point_count = valid_faces * 4;
    points = (point_t *)bu_calloc(point_count, sizeof(point_t),
	    "GED draw BoT wireframe line-set points");
    commands = (int *)bu_calloc(point_count, sizeof(int),
	    "GED draw BoT wireframe line-set commands");

    for (size_t i = 0; i < bot->num_faces; i++) {
	int vertex_order[3];
	int normal_order[3];

	if (!_ged_draw_bot_face_order(bot, i, vertex_order, normal_order))
	    continue;

	VMOVE(points[idx], &bot->vertices[(size_t)vertex_order[0] * 3]);
	commands[idx++] = BSG_GEOMETRY_LINE_MOVE;
	VMOVE(points[idx], &bot->vertices[(size_t)vertex_order[1] * 3]);
	commands[idx++] = BSG_GEOMETRY_LINE_DRAW;
	VMOVE(points[idx], &bot->vertices[(size_t)vertex_order[2] * 3]);
	commands[idx++] = BSG_GEOMETRY_LINE_DRAW;
	VMOVE(points[idx], &bot->vertices[(size_t)vertex_order[0] * 3]);
	commands[idx++] = BSG_GEOMETRY_LINE_DRAW;
    }

    ok = ged_draw_scene_ref_publish_line_set(ref, (const point_t *)points,
	    commands, idx);

    bu_free(points, "GED draw BoT wireframe line-set points");
    bu_free(commands, "GED draw BoT wireframe line-set commands");
    return ok;
}


static int
_ged_draw_pg_face_valid(const struct rt_pg_face_internal *face)
{
    return face && face->verts && face->npts >= 3;
}


int
ged_draw_scene_ref_publish_poly_wireframe_line_set(bsg_scene_ref ref,
						   const struct rt_pg_internal *pg)
{
    point_t *points = NULL;
    int *commands = NULL;
    size_t point_count = 0;
    size_t idx = 0;
    int ok;

    if (!pg)
	return 0;

    RT_PG_CK_MAGIC(pg);
    if (!pg->poly || !pg->npoly)
	return ged_draw_scene_ref_geometry_clear(ref);

    for (size_t i = 0; i < pg->npoly; i++) {
	const struct rt_pg_face_internal *face = &pg->poly[i];
	if (!_ged_draw_pg_face_valid(face))
	    continue;
	if (point_count > ((size_t)-1) - (face->npts + 1))
	    return 0;
	point_count += face->npts + 1;
    }

    if (!point_count)
	return ged_draw_scene_ref_geometry_clear(ref);

    points = (point_t *)bu_calloc(point_count, sizeof(point_t),
	    "GED draw POLY wireframe line-set points");
    commands = (int *)bu_calloc(point_count, sizeof(int),
	    "GED draw POLY wireframe line-set commands");

    for (size_t i = 0; i < pg->npoly; i++) {
	const struct rt_pg_face_internal *face = &pg->poly[i];

	if (!_ged_draw_pg_face_valid(face))
	    continue;

	VMOVE(points[idx], &face->verts[0]);
	commands[idx++] = BSG_GEOMETRY_LINE_MOVE;
	for (size_t j = 1; j < face->npts; j++) {
	    VMOVE(points[idx], &face->verts[3 * j]);
	    commands[idx++] = BSG_GEOMETRY_LINE_DRAW;
	}
	VMOVE(points[idx], &face->verts[0]);
	commands[idx++] = BSG_GEOMETRY_LINE_DRAW;
    }

    ok = ged_draw_scene_ref_publish_line_set(ref, (const point_t *)points,
	    commands, idx);

    bu_free(points, "GED draw POLY wireframe line-set points");
    bu_free(commands, "GED draw POLY wireframe line-set commands");
    return ok;
}


static int
_ged_draw_nmg_vertexuse_coord(const struct vertexuse *vu, point_t point)
{
    if (!vu || !point)
	return 0;

    NMG_CK_VERTEXUSE(vu);
    NMG_CK_VERTEX(vu->v_p);
    if (!vu->v_p->vg_p)
	return 0;
    NMG_CK_VERTEX_G(vu->v_p->vg_p);
    VMOVE(point, vu->v_p->vg_p->coord);
    return 1;
}


static int
_ged_draw_nmg_edgeuse_has_cnurb(const struct edgeuse *eu)
{
    if (!eu)
	return 0;
    NMG_CK_EDGEUSE(eu);
    return (eu->g.magic_p && *eu->g.magic_p == NMG_EDGE_G_CNURB_MAGIC) ? 1 : 0;
}


static const struct face_g_snurb *
_ged_draw_nmg_faceuse_snurb(const struct faceuse *fu)
{
    if (!fu || !fu->f_p || !fu->f_p->g.magic_p)
	return NULL;
    if (*fu->f_p->g.magic_p != NMG_FACE_G_SNURB_MAGIC)
	return NULL;
    NMG_CK_FACE_G_SNURB(fu->f_p->g.snurb_p);
    return fu->f_p->g.snurb_p;
}


static fastf_t
_ged_draw_nmg_point_dist_sq(const point_t a, const point_t b)
{
    vect_t diff;

    VSUB2(diff, a, b);
    return MAGSQ(diff);
}


static int
_ged_draw_nmg_cnurb_linear_uv(const struct edgeuse *eu,
			      point_t start_uv,
			      point_t end_uv)
{
    const struct vertexuse *start_vu;
    const struct vertexuse *end_vu;

    if (!eu || !start_uv || !end_uv || !eu->eumate_p)
	return 0;

    start_vu = eu->vu_p;
    end_vu = eu->eumate_p->vu_p;
    if (!start_vu || !end_vu ||
	    !start_vu->a.magic_p || !end_vu->a.magic_p ||
	    *start_vu->a.magic_p != NMG_VERTEXUSE_A_CNURB_MAGIC ||
	    *end_vu->a.magic_p != NMG_VERTEXUSE_A_CNURB_MAGIC)
	return 0;

    VMOVE(start_uv, start_vu->a.cnurb_p->param);
    VMOVE(end_uv, end_vu->a.cnurb_p->param);
    return 1;
}


static int
_ged_draw_nmg_cnurb_eval_point(const struct edge_g_cnurb *cnurb,
			       const struct face_g_snurb *snurb,
			       fastf_t t,
			       point_t point)
{
    hpoint_t uvw = HINIT_ZERO;
    hpoint_t xyz = HINIT_ZERO;
    int coords;

    if (!cnurb || !point || cnurb->order <= 0)
	return 0;

    NMG_CK_EDGE_G_CNURB(cnurb);
    coords = RT_NURB_EXTRACT_COORDS(cnurb->pt_type);
    if (coords < 2 || coords > 4)
	return 0;

    nmg_nurb_c_eval(cnurb, t, uvw);
    if (RT_NURB_IS_PT_RATIONAL(cnurb->pt_type)) {
	int widx = coords - 1;
	fastf_t weight = uvw[widx];

	if (ZERO(weight))
	    return 0;
	for (int i = 0; i < widx; i++)
	    uvw[i] /= weight;
    }

    if (snurb) {
	int scoords;

	NMG_CK_FACE_G_SNURB(snurb);
	nmg_nurb_s_eval(snurb, uvw[0], uvw[1], xyz);
	scoords = RT_NURB_EXTRACT_COORDS(snurb->pt_type);
	if (RT_NURB_IS_PT_RATIONAL(snurb->pt_type)) {
	    int widx = scoords - 1;
	    fastf_t weight = xyz[widx];

	    if (widx < 1 || ZERO(weight))
		return 0;
	    for (int i = 0; i < widx; i++)
		xyz[i] /= weight;
	}
	VSET(point, xyz[X], xyz[Y], xyz[Z]);
    } else {
	VSET(point, uvw[X], uvw[Y], (coords > 2) ? uvw[Z] : 0.0);
    }

    return 1;
}


static int
_ged_draw_nmg_cnurb_sample_count(const struct edgeuse *eu,
				 const struct face_g_snurb *snurb,
				 size_t *point_count)
{
    const struct edge_g_cnurb *cnurb;

    if (point_count)
	*point_count = 0;
    if (!_ged_draw_nmg_edgeuse_has_cnurb(eu))
	return 0;

    cnurb = eu->g.cnurb_p;
    NMG_CK_EDGE_G_CNURB(cnurb);
    if (cnurb->order <= 0 && !snurb) {
	if (point_count)
	    *point_count = 2;
	return 1;
    }
    if (cnurb->order > 0 &&
	    (cnurb->order < 2 || !cnurb->k.knots ||
	     cnurb->k.k_size <= cnurb->order ||
	     cnurb->c_size < 2))
	return 0;

    if (point_count)
	*point_count = GED_DRAW_NMG_CNURB_SAMPLE_POINTS;
    return 1;
}


static int
_ged_draw_nmg_sample_cnurb_edge(const struct edgeuse *eu,
				const struct face_g_snurb *snurb,
				point_t *samples,
				size_t sample_count)
{
    const struct edge_g_cnurb *cnurb;
    point_t start = VINIT_ZERO;
    point_t end = VINIT_ZERO;

    if (!_ged_draw_nmg_edgeuse_has_cnurb(eu) || !samples || sample_count < 2)
	return 0;
    if (!_ged_draw_nmg_vertexuse_coord(eu->vu_p, start) ||
	    !_ged_draw_nmg_vertexuse_coord(eu->eumate_p->vu_p, end))
	return 0;

    cnurb = eu->g.cnurb_p;
    NMG_CK_EDGE_G_CNURB(cnurb);

    VMOVE(samples[0], start);
    VMOVE(samples[sample_count - 1], end);

    if (cnurb->order <= 0) {
	if (snurb) {
	    point_t start_uv = VINIT_ZERO;
	    point_t end_uv = VINIT_ZERO;

	    if (!_ged_draw_nmg_cnurb_linear_uv(eu, start_uv, end_uv))
		return 0;
	    for (size_t i = 1; i + 1 < sample_count; i++) {
		fastf_t t = (fastf_t)i / (fastf_t)(sample_count - 1);
		point_t uvw;

		VBLEND2(uvw, 1.0 - t, start_uv, t, end_uv);
		nmg_eval_linear_trim_curve(snurb, uvw, samples[i]);
	    }
	} else {
	    for (size_t i = 1; i + 1 < sample_count; i++) {
		fastf_t t = (fastf_t)i / (fastf_t)(sample_count - 1);

		VBLEND2(samples[i], 1.0 - t, start, t, end);
	    }
	}
	return 1;
    }

    {
	fastf_t t0 = cnurb->k.knots[cnurb->order - 1];
	fastf_t t1 = cnurb->k.knots[cnurb->k.k_size - cnurb->order + 1];
	point_t cstart = VINIT_ZERO;
	point_t cend = VINIT_ZERO;
	int reverse = 0;

	if (ZERO(t1 - t0))
	    return 0;
	if (!_ged_draw_nmg_cnurb_eval_point(cnurb, snurb, t0, cstart) ||
		!_ged_draw_nmg_cnurb_eval_point(cnurb, snurb, t1, cend))
	    return 0;
	reverse = (_ged_draw_nmg_point_dist_sq(cstart, end) +
		_ged_draw_nmg_point_dist_sq(cend, start) <
		_ged_draw_nmg_point_dist_sq(cstart, start) +
		_ged_draw_nmg_point_dist_sq(cend, end));

	for (size_t i = 1; i + 1 < sample_count; i++) {
	    fastf_t f = (fastf_t)i / (fastf_t)(sample_count - 1);
	    fastf_t t = reverse ? (t1 - (t1 - t0) * f) : (t0 + (t1 - t0) * f);

	    if (!_ged_draw_nmg_cnurb_eval_point(cnurb, snurb, t, samples[i]))
		return 0;
	}
    }

    return 1;
}


static int
_ged_draw_nmg_edge_line_count(const struct edgeuse *eu,
			      const struct face_g_snurb *snurb,
			      size_t *point_count)
{
    point_t point;
    point_t mate_point;

    if (point_count)
	*point_count = 0;
    if (!eu || !eu->eumate_p)
	return 0;

    NMG_CK_EDGEUSE(eu);
    if (_ged_draw_nmg_edgeuse_has_cnurb(eu))
	return _ged_draw_nmg_cnurb_sample_count(eu, snurb, point_count);

    if (!_ged_draw_nmg_vertexuse_coord(eu->vu_p, point) ||
	    !_ged_draw_nmg_vertexuse_coord(eu->eumate_p->vu_p, mate_point))
	return 0;
    if (point_count)
	*point_count = 2;
    return 1;
}


static void
_ged_draw_nmg_snurb_grid_free(struct face_g_snurb *row_refined,
			      struct face_g_snurb *grid,
			      struct knot_vector *tkv1,
			      struct knot_vector *tkv2,
			      struct knot_vector *tau1,
			      struct knot_vector *tau2)
{
    if (grid)
	nmg_nurb_free_snurb(grid);
    if (row_refined)
	nmg_nurb_free_snurb(row_refined);
    if (tau1 && tau1->knots)
	bu_free((char *)tau1->knots, "GED draw NMG snurb tau1 knots");
    if (tau2 && tau2->knots)
	bu_free((char *)tau2->knots, "GED draw NMG snurb tau2 knots");
    if (tkv1 && tkv1->knots)
	bu_free((char *)tkv1->knots, "GED draw NMG snurb tkv1 knots");
    if (tkv2 && tkv2->knots)
	bu_free((char *)tkv2->knots, "GED draw NMG snurb tkv2 knots");
}


static struct face_g_snurb *
_ged_draw_nmg_snurb_grid_refine(const struct face_g_snurb *fg,
				struct face_g_snurb **row_refined,
				struct knot_vector *tkv1,
				struct knot_vector *tkv2,
				struct knot_vector *tau1,
				struct knot_vector *tau2)
{
    struct face_g_snurb *grid = NULL;

    if (row_refined)
	*row_refined = NULL;
    if (!fg || !row_refined || !tkv1 || !tkv2 || !tau1 || !tau2)
	return NULL;

    NMG_CK_FACE_G_SNURB(fg);
    nmg_nurb_kvgen(tkv1, fg->u.knots[0],
	    fg->u.knots[fg->u.k_size - 1],
	    GED_DRAW_NMG_SNURB_INTERIOR_SAMPLES);
    nmg_nurb_kvgen(tkv2, fg->v.knots[0],
	    fg->v.knots[fg->v.k_size - 1],
	    GED_DRAW_NMG_SNURB_INTERIOR_SAMPLES);
    nmg_nurb_kvmerge(tau1, tkv1, &fg->u);
    nmg_nurb_kvmerge(tau2, tkv2, &fg->v);

    *row_refined = nmg_nurb_s_refine(fg, RT_NURB_SPLIT_COL, tau2);
    if (!*row_refined)
	return NULL;
    NMG_CK_SNURB(*row_refined);

    grid = nmg_nurb_s_refine(*row_refined, RT_NURB_SPLIT_ROW, tau1);
    if (!grid)
	return NULL;
    NMG_CK_SNURB(grid);
    return grid;
}


static int
_ged_draw_nmg_snurb_grid_line_count(const struct face_g_snurb *fg,
				    size_t *point_count)
{
    struct knot_vector tkv1 = {0, 0, NULL};
    struct knot_vector tkv2 = {0, 0, NULL};
    struct knot_vector tau1 = {0, 0, NULL};
    struct knot_vector tau2 = {0, 0, NULL};
    struct face_g_snurb *row_refined = NULL;
    struct face_g_snurb *grid = NULL;
    int ok = 0;

    if (point_count)
	*point_count = 0;
    if (!fg)
	return 0;

    grid = _ged_draw_nmg_snurb_grid_refine(fg, &row_refined, &tkv1, &tkv2,
	    &tau1, &tau2);
    if (!grid)
	goto cleanup;
    if (grid->s_size[0] <= 0 || grid->s_size[1] <= 0)
	goto cleanup;

    if (point_count)
	*point_count = (size_t)grid->s_size[0] * (size_t)grid->s_size[1] * 2;
    ok = 1;

cleanup:
    _ged_draw_nmg_snurb_grid_free(row_refined, grid, &tkv1, &tkv2, &tau1,
	    &tau2);
    return ok;
}


static int
_ged_draw_nmg_loop_line_count(const struct loopuse *lu,
			      const struct face_g_snurb *snurb,
			      size_t *point_count)
{
    const struct edgeuse *eu;
    size_t points = 0;
    int first = 1;

    if (point_count)
	*point_count = 0;
    if (!lu)
	return 0;

    NMG_CK_LOOPUSE(lu);
    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC) {
	point_t point;
	const struct vertexuse *vu = BU_LIST_FIRST(vertexuse, &lu->down_hd);
	if (!_ged_draw_nmg_vertexuse_coord(vu, point))
	    return 0;
	if (point_count)
	    *point_count = 2;
	return 1;
    }
    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
	return 0;

    for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
	size_t edge_points = 0;

	if (!_ged_draw_nmg_edge_line_count(eu, snurb, &edge_points))
	    return 0;
	if (edge_points < 2)
	    return 0;
	points += first ? edge_points : edge_points - 1;
	first = 0;
    }

    if (!points)
	return 0;
    if (point_count)
	*point_count = points;
    return 1;
}


static int
_ged_draw_nmg_loop_normal_line_count(const struct loopuse *lu, size_t *point_count)
{
    const struct edgeuse *eu;
    size_t vertices = 0;
    size_t vertex_normals = 0;

    if (point_count)
	*point_count = 0;
    if (!lu)
	return 0;

    NMG_CK_LOOPUSE(lu);
    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC)
	return 1;
    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
	return 0;

    for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
	point_t point;
	const struct vertexuse *vu = eu->vu_p;
	if (_ged_draw_nmg_edgeuse_has_cnurb(eu))
	    return 0;
	if (!_ged_draw_nmg_vertexuse_coord(vu, point))
	    return 0;
	if (vu->a.magic_p && *vu->a.magic_p == NMG_VERTEXUSE_A_PLANE_MAGIC)
	    vertex_normals++;
	vertices++;
    }

    if (vertices > 2 && point_count)
	*point_count = 2 + 2 * vertex_normals;
    return 1;
}


static int
_ged_draw_nmg_append_line(point_t *points,
			  int *commands,
			  size_t point_count,
			  size_t *point_idx,
			  const point_t point,
			  int command)
{
    if (!points || !commands || !point_idx || !point || *point_idx >= point_count)
	return 0;

    VMOVE(points[*point_idx], point);
    commands[*point_idx] = command;
    (*point_idx)++;
    return 1;
}


static int
_ged_draw_nmg_append_snurb_grid_lines(point_t *points,
				      int *commands,
				      size_t point_count,
				      size_t *point_idx,
				      const struct face_g_snurb *fg)
{
    struct knot_vector tkv1 = {0, 0, NULL};
    struct knot_vector tkv2 = {0, 0, NULL};
    struct knot_vector tau1 = {0, 0, NULL};
    struct knot_vector tau2 = {0, 0, NULL};
    struct face_g_snurb *row_refined = NULL;
    struct face_g_snurb *grid = NULL;
    int coords = 0;
    int ok = 0;

    if (!points || !commands || !point_idx || !fg)
	return 0;

    grid = _ged_draw_nmg_snurb_grid_refine(fg, &row_refined, &tkv1, &tkv2,
	    &tau1, &tau2);
    if (!grid)
	goto cleanup;
    if (grid->s_size[0] <= 0 || grid->s_size[1] <= 0)
	goto cleanup;

    coords = RT_NURB_EXTRACT_COORDS(grid->pt_type);
    if (coords < 3)
	goto cleanup;

    if (RT_NURB_IS_PT_RATIONAL(grid->pt_type)) {
	fastf_t *vp = grid->ctl_points;
	for (int i = 0; i < grid->s_size[0] * grid->s_size[1]; i++) {
	    fastf_t w = vp[3];
	    if (NEAR_ZERO(w, SMALL_FASTF))
		goto cleanup;
	    fastf_t one_over_w = 1.0 / w;
	    vp[0] *= one_over_w;
	    vp[1] *= one_over_w;
	    vp[2] *= one_over_w;
	    vp[3] *= one_over_w;
	    vp += coords;
	}
    }

    {
	fastf_t *vp = grid->ctl_points;
	for (int row = 0; row < grid->s_size[0]; row++) {
	    point_t point;
	    VSET(point, vp[0], vp[1], vp[2]);
	    if (!_ged_draw_nmg_append_line(points, commands, point_count,
			point_idx, point, BSG_GEOMETRY_LINE_MOVE))
		goto cleanup;
	    vp += coords;
	    for (int col = 1; col < grid->s_size[1]; col++) {
		VSET(point, vp[0], vp[1], vp[2]);
		if (!_ged_draw_nmg_append_line(points, commands, point_count,
			    point_idx, point, BSG_GEOMETRY_LINE_DRAW))
		    goto cleanup;
		vp += coords;
	    }
	}
    }

    for (int col = 0; col < grid->s_size[1]; col++) {
	int stride = grid->s_size[1] * coords;
	fastf_t *vp = &grid->ctl_points[col * coords];
	point_t point;

	VSET(point, vp[0], vp[1], vp[2]);
	if (!_ged_draw_nmg_append_line(points, commands, point_count,
		point_idx, point, BSG_GEOMETRY_LINE_MOVE))
	    goto cleanup;
	vp += stride;
	for (int row = 1; row < grid->s_size[0]; row++) {
	    VSET(point, vp[0], vp[1], vp[2]);
	    if (!_ged_draw_nmg_append_line(points, commands, point_count,
		    point_idx, point, BSG_GEOMETRY_LINE_DRAW))
		goto cleanup;
	    vp += stride;
	}
    }

    ok = 1;

cleanup:
    _ged_draw_nmg_snurb_grid_free(row_refined, grid, &tkv1, &tkv2, &tau1,
	    &tau2);
    return ok;
}


static int
_ged_draw_nmg_append_loop_normal_lines(point_t *points,
				       int *commands,
				       size_t point_count,
				       size_t *point_idx,
				       const struct loopuse *lu,
				       const vect_t normal)
{
    const struct edgeuse *eu;
    point_t centroid = VINIT_ZERO;
    point_t first = VINIT_ZERO;
    size_t vertices = 0;
    int is_first = 1;

    if (!lu)
	return 0;

    NMG_CK_LOOPUSE(lu);
    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC)
	return 1;
    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
	return 0;

    for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
	point_t point;
	if (_ged_draw_nmg_edgeuse_has_cnurb(eu))
	    return 0;
	if (!_ged_draw_nmg_vertexuse_coord(eu->vu_p, point))
	    return 0;
	if (is_first) {
	    VMOVE(first, point);
	    is_first = 0;
	}
	VADD2(centroid, centroid, point);
	vertices++;
    }

    if (vertices <= 2)
	return 1;

    {
	double scale;
	vect_t tocent;
	point_t tip;

	VSCALE(centroid, centroid, 1.0 / (double)vertices);
	VSUB2(tocent, first, centroid);
	scale = MAGNITUDE(tocent) * 0.5;

	if (!_ged_draw_nmg_append_line(points, commands, point_count, point_idx,
		    centroid, BSG_GEOMETRY_LINE_MOVE))
	    return 0;
	VJOIN1(tip, centroid, scale, normal);
	if (!_ged_draw_nmg_append_line(points, commands, point_count, point_idx,
		    tip, BSG_GEOMETRY_LINE_DRAW))
	    return 0;

	for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
	    const struct vertexuse *vu = eu->vu_p;
	    point_t point;
	    if (!vu->a.magic_p || *vu->a.magic_p != NMG_VERTEXUSE_A_PLANE_MAGIC)
		continue;
	    if (!_ged_draw_nmg_vertexuse_coord(vu, point))
		return 0;
	    if (!_ged_draw_nmg_append_line(points, commands, point_count,
			point_idx, point, BSG_GEOMETRY_LINE_MOVE))
		return 0;
	    VJOIN1(tip, point, scale, vu->a.plane_p->N);
	    if (!_ged_draw_nmg_append_line(points, commands, point_count,
			point_idx, tip, BSG_GEOMETRY_LINE_DRAW))
		return 0;
	}
    }

    return 1;
}


static int
_ged_draw_nmg_append_loop_lines(point_t *points,
				int *commands,
				size_t point_count,
				size_t *point_idx,
				const struct loopuse *lu,
				const struct face_g_snurb *snurb)
{
    const struct edgeuse *eu;
    int is_first = 1;

    if (!lu)
	return 0;

    NMG_CK_LOOPUSE(lu);
    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC) {
	point_t point;
	const struct vertexuse *vu = BU_LIST_FIRST(vertexuse, &lu->down_hd);
	if (!_ged_draw_nmg_vertexuse_coord(vu, point))
	    return 0;
	if (!_ged_draw_nmg_append_line(points, commands, point_count, point_idx,
		    point, BSG_GEOMETRY_LINE_MOVE))
	    return 0;
	return _ged_draw_nmg_append_line(points, commands, point_count, point_idx,
		point, BSG_GEOMETRY_LINE_DRAW);
    }
    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
	return 0;

    for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
	if (_ged_draw_nmg_edgeuse_has_cnurb(eu)) {
	    point_t samples[GED_DRAW_NMG_CNURB_SAMPLE_POINTS];
	    size_t sample_count = 0;
	    size_t start = is_first ? 0 : 1;

	    if (!_ged_draw_nmg_cnurb_sample_count(eu, snurb, &sample_count) ||
		    sample_count > GED_DRAW_NMG_CNURB_SAMPLE_POINTS ||
		    !_ged_draw_nmg_sample_cnurb_edge(eu, snurb, samples,
			sample_count))
		return 0;
	    for (size_t i = start; i < sample_count; i++) {
		if (!_ged_draw_nmg_append_line(points, commands, point_count,
			    point_idx, samples[i],
			    (is_first && i == 0) ? BSG_GEOMETRY_LINE_MOVE :
			    BSG_GEOMETRY_LINE_DRAW))
		    return 0;
	    }
	    is_first = 0;
	} else {
	    point_t point;
	    point_t mate_point;

	    if (!_ged_draw_nmg_vertexuse_coord(eu->vu_p, point) ||
		    !_ged_draw_nmg_vertexuse_coord(eu->eumate_p->vu_p,
			mate_point))
		return 0;
	    if (is_first) {
		if (!_ged_draw_nmg_append_line(points, commands, point_count,
			    point_idx, point, BSG_GEOMETRY_LINE_MOVE))
		    return 0;
		is_first = 0;
	    }
	    if (!_ged_draw_nmg_append_line(points, commands, point_count,
			point_idx, mate_point, BSG_GEOMETRY_LINE_DRAW))
		return 0;
	}
    }

    return is_first ? 0 : 1;
}


static int
_ged_draw_nmg_wire_edge_line_count(const struct bu_list *eu_hd, size_t *point_count)
{
    const struct edgeuse *eu;
    size_t points = 0;

    if (point_count)
	*point_count = 0;
    if (!eu_hd)
	return 0;

    for (BU_LIST_FOR(eu, edgeuse, eu_hd)) {
	size_t edge_points = 0;

	NMG_CK_EDGEUSE(eu);
	if (!_ged_draw_nmg_edge_line_count(eu,
		    _ged_draw_nmg_faceuse_snurb(nmg_find_fu_of_eu(eu)),
		    &edge_points))
	    return 0;
	points += edge_points;
    }

    if (point_count)
	*point_count = points;
    return 1;
}


static int
_ged_draw_nmg_append_wire_edge_lines(point_t *points,
				     int *commands,
				     size_t point_count,
				     size_t *point_idx,
				     const struct bu_list *eu_hd)
{
    const struct edgeuse *eu;

    if (!eu_hd)
	return 0;

    for (BU_LIST_FOR(eu, edgeuse, eu_hd)) {
	const struct face_g_snurb *snurb =
	    _ged_draw_nmg_faceuse_snurb(nmg_find_fu_of_eu(eu));

	NMG_CK_EDGEUSE(eu);
	if (_ged_draw_nmg_edgeuse_has_cnurb(eu)) {
	    point_t samples[GED_DRAW_NMG_CNURB_SAMPLE_POINTS];
	    size_t sample_count = 0;

	    if (!_ged_draw_nmg_cnurb_sample_count(eu, snurb, &sample_count) ||
		    sample_count > GED_DRAW_NMG_CNURB_SAMPLE_POINTS ||
		    !_ged_draw_nmg_sample_cnurb_edge(eu, snurb, samples,
			sample_count))
		return 0;
	    for (size_t i = 0; i < sample_count; i++) {
		if (!_ged_draw_nmg_append_line(points, commands, point_count,
			    point_idx, samples[i],
			    (i == 0) ? BSG_GEOMETRY_LINE_MOVE :
			    BSG_GEOMETRY_LINE_DRAW))
		    return 0;
	    }
	} else {
	    point_t point;
	    point_t mate_point;

	    if (!_ged_draw_nmg_vertexuse_coord(eu->vu_p, point) ||
		    !_ged_draw_nmg_vertexuse_coord(eu->eumate_p->vu_p,
			mate_point))
		return 0;
	    if (!_ged_draw_nmg_append_line(points, commands, point_count,
			point_idx, point, BSG_GEOMETRY_LINE_MOVE))
		return 0;
	    if (!_ged_draw_nmg_append_line(points, commands, point_count,
			point_idx, mate_point, BSG_GEOMETRY_LINE_DRAW))
		return 0;
	}
    }

    return 1;
}


static int
_ged_draw_nmg_surface_style(int style)
{
    return ((style & GED_DRAW_NMG_STYLE_POLYGON) &&
	    !(style & GED_DRAW_NMG_STYLE_NO_SURFACES));
}


static void
_ged_draw_scene_ref_copy_display_state(bsg_scene_ref dst, bsg_scene_ref src)
{
    unsigned char r = 0;
    unsigned char g = 0;
    unsigned char b = 0;
    unsigned char basecolor[3] = {0, 0, 0};
    mat_t mat;
    point_t bmin, bmax;
    vect_t center = VINIT_ZERO;

    if (bsg_scene_ref_is_null(dst) || bsg_scene_ref_is_null(src))
	return;

    bsg_scene_set_visible(dst, bsg_scene_visible(src));
    bsg_scene_color(src, &r, &g, &b);
    bsg_scene_set_color(dst, r, g, b);
    bsg_scene_material_get_rgb(src, &r, &g, &b);
    bsg_scene_material_set_rgb(dst, r, g, b);
    bsg_scene_material_set_revision(dst, bsg_scene_material_revision(src));

    MAT_IDN(mat);
    bsg_scene_transform(src, mat);
    bsg_scene_set_transform(dst, mat);
    MAT_IDN(mat);
    bsg_scene_draw_mat(src, mat);
    bsg_scene_set_draw_mat(dst, mat);

    if (bsg_scene_bounds(src, bmin, bmax))
	bsg_scene_set_bounds(dst, bmin, bmax, 1);
    bsg_scene_draw_center(src, center);
    bsg_scene_set_draw_center(dst, center);
    bsg_scene_set_draw_size(dst, bsg_scene_draw_size(src));

    bsg_scene_set_line_style(dst, bsg_scene_line_style(src));
    bsg_scene_set_line_width(dst, bsg_scene_line_width(src));
    bsg_scene_legacy_basecolor(src, basecolor);
    bsg_scene_set_legacy_color_info(dst, basecolor,
	    bsg_scene_legacy_user_color(src),
	    bsg_scene_legacy_default_color(src));
    bsg_scene_set_legacy_eval_flag(dst, bsg_scene_legacy_eval_flag(src));
    bsg_scene_set_legacy_region_id(dst, bsg_scene_legacy_region_id(src));
    bsg_scene_set_highlighted(dst, bsg_scene_highlighted(src));
    bsg_scene_set_dmode(dst, bsg_scene_dmode(src));
    bsg_scene_set_transparency(dst, bsg_scene_transparency(src));
    bsg_scene_set_changed(dst, bsg_scene_changed(src));

    const struct bsg_draw_intent *di = bsg_scene_draw_intent(src);
    const char *path = bsg_draw_intent_path(di);
    if (path && path[0]) {
	struct bsg_draw_intent *copy =
	    bsg_draw_intent_create(path, bsg_draw_intent_mode(di));
	if (copy) {
	    struct bsg_appearance_settings appearance;
	    copy->di_lod = bsg_draw_intent_lod(di);
	    copy->di_mixed = di->di_mixed;
	    if (bsg_draw_intent_appearance(di, &appearance))
		bsg_draw_intent_set_appearance(copy, &appearance);
	    bsg_scene_set_draw_intent(dst, copy);
	}
    }
}


static void
_ged_draw_scene_ref_set_semantic_path(bsg_scene_ref ref, const char *path)
{
    if (bsg_scene_ref_is_null(ref) || !path || !path[0])
	return;

    const char *semantic_path = path;
    while (*semantic_path == '/')
	semantic_path++;
    if (!semantic_path[0])
	semantic_path = path;

    struct bsg_draw_intent *di =
	bsg_draw_intent_create(semantic_path, BSG_DRAW_MODE_WIRE);
    if (di)
	bsg_scene_set_draw_intent(ref, di);
}


static bsg_scene_ref
_ged_draw_scene_ref_create_child_shape(bsg_scene_ref primary_ref,
				       const char *source_name,
				       const char *shape_name,
				       bsg_scene_ref *child_source_out)
{
    if (child_source_out)
	*child_source_out = bsg_scene_ref_null();

    if (bsg_scene_ref_is_null(primary_ref))
	return bsg_scene_ref_null();

    ged_draw_shape_state *shape_data =
	ged_draw_scene_ref_shape_state(primary_ref);
    bsg_scene_ref parent_source_ref = ged_draw_shape_source_ref(primary_ref);
    struct bsg_view *v = bsg_scene_view(primary_ref);
    if (!v)
	v = bsg_scene_view(parent_source_ref);

    if (!shape_data || !shape_data->gedp || !v ||
	    bsg_scene_ref_is_null(parent_source_ref))
	return bsg_scene_ref_null();

    bsg_database_source_ref child_source =
	bsg_database_source_ref_create(v,
		source_name ? source_name : "_submodel_source");
    bsg_scene_ref child_source_ref =
	bsg_database_source_ref_as_scene(child_source);
    if (bsg_scene_ref_is_null(child_source_ref))
	return bsg_scene_ref_null();

    bsg_geometry_ref child_geometry =
	bsg_geometry_ref_create(v, shape_name ? shape_name : "submodel_leaf");
    bsg_scene_ref child_shape_ref =
	bsg_geometry_ref_as_scene(child_geometry);
    if (bsg_scene_ref_is_null(child_shape_ref)) {
	ged_draw_scene_ref_release(child_source_ref);
	return bsg_scene_ref_null();
    }

    bsg_scene_append_child(child_source_ref, child_shape_ref);
    if (!ged_draw_scene_ref_prepare(shape_data->gedp, child_shape_ref)) {
	ged_draw_scene_ref_release(child_source_ref);
	return bsg_scene_ref_null();
    }

    ged_draw_scene_ref_set_source_ref(child_shape_ref, child_source_ref);
    ged_draw_scene_ref_geometry_clear(child_shape_ref);

    _ged_draw_scene_ref_copy_display_state(child_source_ref, primary_ref);
    _ged_draw_scene_ref_copy_display_state(child_shape_ref, primary_ref);
    bsg_scene_mark_db_object(child_shape_ref);
    bsg_scene_set_non_database_source(child_source_ref, 0);
    bsg_scene_set_non_database_source(child_shape_ref, 0);

    bsg_scene_append_child(parent_source_ref, child_source_ref);
    if (child_source_out)
	*child_source_out = child_source_ref;
    return child_shape_ref;
}


static void
_ged_draw_scene_ref_clear_child_sources(bsg_scene_ref primary_ref)
{
    bsg_scene_ref source_ref = ged_draw_shape_source_ref(primary_ref);
    if (bsg_scene_ref_is_null(primary_ref) || bsg_scene_ref_is_null(source_ref))
	return;

    size_t child_count = bsg_scene_child_count(source_ref);
    while (child_count > 0) {
	child_count--;
	bsg_scene_ref child_ref = bsg_scene_child_at(source_ref, child_count);
	if (bsg_scene_ref_equal(child_ref, primary_ref))
	    continue;
	bsg_scene_remove_child(source_ref, child_ref);
	ged_draw_scene_ref_release(child_ref);
    }
    bsg_scene_invalidate(source_ref);
}


struct ged_draw_submodel_publish_ctx {
    bsg_scene_ref parent_ref;
    const struct bg_tess_tol *ttol;
    const struct bn_tol *tol;
    struct bsg_view *view;
    size_t child_count;
    int failed;
};


static union tree *
_ged_draw_submodel_wireframe_leaf(struct db_tree_state *tsp,
				  const struct db_full_path *pathp,
				  struct rt_db_internal *ip,
				  void *client_data)
{
    struct ged_draw_submodel_publish_ctx *ctx =
	(struct ged_draw_submodel_publish_ctx *)client_data;
    char *path_name = NULL;
    const char *name = "submodel_leaf";
    bsg_scene_ref child_source_ref = bsg_scene_ref_null();

    if (!ctx || ctx->failed || !tsp || !ip)
	return TREE_NULL;

    if (pathp && pathp->fp_len > 0) {
	path_name = db_path_to_string(pathp);
	if (path_name && path_name[0])
	    name = path_name;
    } else if (ip->idb_meth) {
	name = ip->idb_meth->ft_name;
    }

    bsg_scene_ref child_shape_ref =
	_ged_draw_scene_ref_create_child_shape(ctx->parent_ref, name, name,
		&child_source_ref);
    if (bsg_scene_ref_is_null(child_shape_ref)) {
	ctx->failed = 1;
	if (path_name)
	    bu_free(path_name, "submodel leaf path string");
	return TREE_NULL;
    }

    _ged_draw_scene_ref_set_semantic_path(child_source_ref, name);
    _ged_draw_scene_ref_set_semantic_path(child_shape_ref, name);

    int ret = ged_draw_scene_ref_publish_primitive_wireframe(child_shape_ref,
	    ip, ctx->ttol ? ctx->ttol : tsp->ts_ttol,
	    ctx->tol ? ctx->tol : tsp->ts_tol, ctx->view, 0);
    if (ret < 0) {
	bu_log("ged draw submodel leaf %s (%s) wireframe failure\n",
		name, ip->idb_meth ? ip->idb_meth->ft_name : "unknown");
	bsg_scene_ref source_ref = ged_draw_shape_source_ref(ctx->parent_ref);
	if (!bsg_scene_ref_is_null(source_ref) &&
		!bsg_scene_ref_is_null(child_source_ref))
	    bsg_scene_remove_child(source_ref, child_source_ref);
	if (!bsg_scene_ref_is_null(child_source_ref))
	    ged_draw_scene_ref_release(child_source_ref);
	ctx->failed = 1;
	if (path_name)
	    bu_free(path_name, "submodel leaf path string");
	return TREE_NULL;
    }

    (void)ged_draw_scene_ref_update_bounds_from_geometry(child_shape_ref, NULL);
    if (!bsg_scene_ref_is_null(child_source_ref))
	(void)bsg_scene_update_bounds(child_source_ref, ctx->view);
    ctx->child_count++;

    if (path_name)
	bu_free(path_name, "submodel leaf path string");

    union tree *curtree;
    BU_GET(curtree, union tree);
    RT_TREE_INIT(curtree);
    curtree->tr_op = OP_NOP;
    return curtree;
}


static int
ged_draw_scene_ref_publish_submodel_wireframe_children(bsg_scene_ref ref,
						       struct rt_db_internal *ip,
						       const struct bg_tess_tol *ttol,
						       const struct bn_tol *tol,
						       struct bsg_view *v)
{
    if (bsg_scene_ref_is_null(ref) || !ip || ip->idb_type != ID_SUBMODEL ||
	    !ip->idb_ptr)
	return 0;

    struct rt_submodel_internal *sip =
	(struct rt_submodel_internal *)ip->idb_ptr;
    RT_SUBMODEL_CK_MAGIC(sip);

    _ged_draw_scene_ref_clear_child_sources(ref);
    ged_draw_scene_ref_geometry_clear(ref);

    struct db_i *dbip = DBI_NULL;
    int close_db = 0;
    if (bu_vls_strlen(&sip->file) != 0) {
	dbip = db_open(bu_vls_addr(&sip->file), DB_OPEN_READONLY);
	if (dbip == DBI_NULL) {
	    bu_log("Cannot open geometry database file (%s) to store plot\n",
		    bu_vls_addr(&sip->file));
	    return 0;
	}
	close_db = 1;
	if (!db_is_directory_non_empty(dbip) && db_dirbuild(dbip) < 0) {
	    bu_log("ged draw submodel db_dirbuild() failure\n");
	    db_close(dbip);
	    return 0;
	}
    } else {
	RT_CK_DBI(sip->dbip);
	dbip = (struct db_i *)sip->dbip;
    }

    struct db_tree_state state;
    RT_DBTS_INIT(&state);
    state.ts_ttol = ttol;
    state.ts_tol = tol;
    MAT_COPY(state.ts_mat, sip->root2leaf);

    struct ged_draw_submodel_publish_ctx ctx;
    ctx.parent_ref = ref;
    ctx.ttol = ttol;
    ctx.tol = tol;
    ctx.view = v ? v : bsg_scene_view(ref);
    ctx.child_count = 0;
    ctx.failed = 0;

    const char *argv[2];
    argv[0] = bu_vls_addr(&sip->treetop);
    argv[1] = NULL;
    int ret = db_walk_tree(dbip, 1, argv, 1, &state, 0, NULL,
	    _ged_draw_submodel_wireframe_leaf, (void *)&ctx);

    if (close_db)
	db_close(dbip);

    if (ret < 0 || ctx.failed) {
	bu_log("ged draw submodel db_walk_tree(%s) failure\n",
		bu_vls_addr(&sip->treetop));
	return 0;
    }

    bsg_scene_ref source_ref = ged_draw_shape_source_ref(ref);
    if (!bsg_scene_ref_is_null(source_ref))
	(void)bsg_scene_update_bounds(source_ref, ctx.view);
    (void)bsg_scene_update_bounds(ref, ctx.view);
    return 1;
}


static bsg_scene_ref
_ged_draw_scene_ref_create_aux_geometry(bsg_scene_ref primary_ref,
					const char *name)
{
    bsg_scene_ref source_ref = ged_draw_shape_source_ref(primary_ref);
    bsg_geometry_ref geometry_ref;
    bsg_scene_ref aux_ref;

    if (bsg_scene_ref_is_null(primary_ref) ||
	    bsg_scene_ref_is_null(source_ref))
	return bsg_scene_ref_null();

    geometry_ref = bsg_geometry_ref_create(bsg_scene_view(primary_ref), name);
    aux_ref = bsg_geometry_ref_as_scene(geometry_ref);
    if (bsg_scene_ref_is_null(aux_ref))
	return bsg_scene_ref_null();

    _ged_draw_scene_ref_copy_display_state(aux_ref, primary_ref);
    bsg_scene_mark_db_object(aux_ref);
    bsg_scene_set_non_database_source(aux_ref, 0);
    bsg_scene_append_child(source_ref, aux_ref);
    return aux_ref;
}


static int
_ged_draw_nmg_line_set_stats(const struct nmgregion *r,
			     int style,
			     size_t *point_count)
{
    const struct shell *s;
    const int surface_style = _ged_draw_nmg_surface_style(style);
    size_t points = 0;

    if (point_count)
	*point_count = 0;
    if (!r)
	return 0;

    if (surface_style && (style & GED_DRAW_NMG_STYLE_VISUALIZE_NORMALS))
	return 0;

    NMG_CK_REGION(r);
    for (BU_LIST_FOR(s, shell, &r->s_hd)) {
	const struct faceuse *fu;
	const struct loopuse *lu;
	size_t shell_points = 0;

	NMG_CK_SHELL(s);
	for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
	    const struct face_g_snurb *snurb = NULL;

	    if (fu->orientation != OT_SAME)
		continue;
	    if (surface_style)
		return 0;

	    NMG_CK_FACEUSE(fu);
	    NMG_CK_FACE(fu->f_p);
	    if (!fu->f_p->g.magic_p)
		return 0;
	    if (*fu->f_p->g.magic_p == NMG_FACE_G_SNURB_MAGIC) {
		if (style & GED_DRAW_NMG_STYLE_VISUALIZE_NORMALS)
		    return 0;
		snurb = _ged_draw_nmg_faceuse_snurb(fu);
		if (!(style & GED_DRAW_NMG_STYLE_NO_SURFACES)) {
		    size_t snurb_points = 0;
		    if (!_ged_draw_nmg_snurb_grid_line_count(snurb,
				&snurb_points))
			return 0;
		    shell_points += snurb_points;
		}
	    } else if (*fu->f_p->g.magic_p == NMG_FACE_G_PLANE_MAGIC) {
		NMG_CK_FACE_G_PLANE(fu->f_p->g.plane_p);
	    } else {
		return 0;
	    }

	    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		size_t loop_points = 0;
		if (!_ged_draw_nmg_loop_line_count(lu, snurb, &loop_points))
		    return 0;
		shell_points += loop_points;
		if (style & GED_DRAW_NMG_STYLE_VISUALIZE_NORMALS) {
		    size_t normal_points = 0;
		    if (!_ged_draw_nmg_loop_normal_line_count(lu, &normal_points))
			return 0;
		    shell_points += normal_points;
		}
	    }
	}

	for (BU_LIST_FOR(lu, loopuse, &s->lu_hd)) {
	    size_t loop_points = 0;
	    if (!_ged_draw_nmg_loop_line_count(lu, NULL, &loop_points))
		return 0;
	    shell_points += loop_points;
	}

	{
	    size_t edge_points = 0;
	    if (!_ged_draw_nmg_wire_edge_line_count(&s->eu_hd, &edge_points))
		return 0;
	    shell_points += edge_points;
	}

	if (s->vu_p) {
	    point_t point;
	    if (!_ged_draw_nmg_vertexuse_coord(s->vu_p, point))
		return 0;
	    shell_points += 2;
	}

	points += shell_points;
    }

    if (!points)
	return 0;
    if (point_count)
	*point_count = points;
    return 1;
}


static int
_ged_draw_nmg_normal_line_stats(const struct nmgregion *r,
				int style,
				size_t *point_count)
{
    const struct shell *s;
    size_t points = 0;

    if (point_count)
	*point_count = 0;
    if (!r || !_ged_draw_nmg_surface_style(style) ||
	    !(style & GED_DRAW_NMG_STYLE_VISUALIZE_NORMALS))
	return 0;

    NMG_CK_REGION(r);
    for (BU_LIST_FOR(s, shell, &r->s_hd)) {
	const struct faceuse *fu;
	NMG_CK_SHELL(s);
	for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
	    const struct loopuse *lu;
	    if (fu->orientation != OT_SAME)
		continue;
	    NMG_CK_FACEUSE(fu);
	    NMG_CK_FACE(fu->f_p);
	    if (!fu->f_p->g.magic_p ||
		    *fu->f_p->g.magic_p == NMG_FACE_G_SNURB_MAGIC)
		return 0;
	    NMG_CK_FACE_G_PLANE(fu->f_p->g.plane_p);

	    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		size_t loop_points = 0;
		if (!_ged_draw_nmg_loop_normal_line_count(lu, &loop_points))
		    return 0;
		points += loop_points;
	    }
	}
    }

    if (!points)
	return 0;
    if (point_count)
	*point_count = points;
    return 1;
}


static int
_ged_draw_nmg_wire_remainder_line_stats(const struct nmgregion *r,
					size_t *point_count)
{
    const struct shell *s;
    size_t points = 0;

    if (point_count)
	*point_count = 0;
    if (!r)
	return 0;

    NMG_CK_REGION(r);
    for (BU_LIST_FOR(s, shell, &r->s_hd)) {
	const struct loopuse *lu;
	size_t edge_points = 0;

	NMG_CK_SHELL(s);
	for (BU_LIST_FOR(lu, loopuse, &s->lu_hd)) {
	    size_t loop_points = 0;
	    if (!_ged_draw_nmg_loop_line_count(lu, NULL, &loop_points))
		return 0;
	    points += loop_points;
	}

	if (!_ged_draw_nmg_wire_edge_line_count(&s->eu_hd, &edge_points))
	    return 0;
	points += edge_points;

	if (s->vu_p) {
	    point_t point;
	    if (!_ged_draw_nmg_vertexuse_coord(s->vu_p, point))
		return 0;
	    points += 2;
	}
    }

    if (point_count)
	*point_count = points;
    return 1;
}


static int
_ged_draw_nmg_fill_normal_line_set(const struct nmgregion *r,
				   int style,
				   point_t *points,
				   int *commands,
				   size_t point_count)
{
    const struct shell *s;
    size_t point_idx = 0;

    if (!r || !points || !commands || !_ged_draw_nmg_surface_style(style) ||
	    !(style & GED_DRAW_NMG_STYLE_VISUALIZE_NORMALS))
	return 0;

    for (BU_LIST_FOR(s, shell, &r->s_hd)) {
	const struct faceuse *fu;
	for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
	    const struct loopuse *lu;
	    vect_t face_normal = VINIT_ZERO;

	    if (fu->orientation != OT_SAME)
		continue;
	    NMG_GET_FU_NORMAL(face_normal, fu);
	    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		if (!_ged_draw_nmg_append_loop_normal_lines(points, commands,
			    point_count, &point_idx, lu, face_normal))
		    return 0;
	    }
	}
    }

    return (point_idx == point_count) ? 1 : 0;
}


static int
_ged_draw_nmg_fill_wire_remainder_line_set(const struct nmgregion *r,
					   point_t *points,
					   int *commands,
					   size_t point_count)
{
    const struct shell *s;
    size_t point_idx = 0;

    if (!r || !points || !commands)
	return 0;

    for (BU_LIST_FOR(s, shell, &r->s_hd)) {
	const struct loopuse *lu;

	for (BU_LIST_FOR(lu, loopuse, &s->lu_hd)) {
	    if (!_ged_draw_nmg_append_loop_lines(points, commands, point_count,
			&point_idx, lu, NULL))
		return 0;
	}

	if (!_ged_draw_nmg_append_wire_edge_lines(points, commands, point_count,
		    &point_idx, &s->eu_hd))
	    return 0;

	if (s->vu_p) {
	    point_t point;
	    if (!_ged_draw_nmg_vertexuse_coord(s->vu_p, point))
		return 0;
	    if (!_ged_draw_nmg_append_line(points, commands, point_count,
			&point_idx, point, BSG_GEOMETRY_LINE_MOVE))
		return 0;
	    if (!_ged_draw_nmg_append_line(points, commands, point_count,
			&point_idx, point, BSG_GEOMETRY_LINE_DRAW))
		return 0;
	}
    }

    return (point_idx == point_count) ? 1 : 0;
}


static int
_ged_draw_nmg_append_region_line_set(const struct nmgregion *r,
				     int style,
				     point_t *points,
				     int *commands,
				     size_t point_count,
				     size_t *point_idx)
{
    const struct shell *s;
    const int surface_style = _ged_draw_nmg_surface_style(style);

    if (!r || !points || !commands || !point_idx)
	return 0;

    for (BU_LIST_FOR(s, shell, &r->s_hd)) {
	const struct faceuse *fu;
	const struct loopuse *lu;

	for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
	    const struct face_g_snurb *snurb = NULL;
	    vect_t face_normal = VINIT_ZERO;

	    if (fu->orientation != OT_SAME)
		continue;
	    if (surface_style)
		return 0;
	    snurb = _ged_draw_nmg_faceuse_snurb(fu);
	    if (!snurb) {
		NMG_GET_FU_NORMAL(face_normal, fu);
	    } else if (style & GED_DRAW_NMG_STYLE_VISUALIZE_NORMALS) {
		return 0;
	    } else if (!(style & GED_DRAW_NMG_STYLE_NO_SURFACES)) {
		if (!_ged_draw_nmg_append_snurb_grid_lines(points, commands,
			    point_count, point_idx, snurb))
		    return 0;
	    }
	    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		if (!_ged_draw_nmg_append_loop_lines(points, commands,
			    point_count, point_idx, lu, snurb))
		    return 0;
		if ((style & GED_DRAW_NMG_STYLE_VISUALIZE_NORMALS) &&
			!_ged_draw_nmg_append_loop_normal_lines(points, commands,
			    point_count, point_idx, lu, face_normal))
		    return 0;
	    }
	}

	for (BU_LIST_FOR(lu, loopuse, &s->lu_hd)) {
	    if (!_ged_draw_nmg_append_loop_lines(points, commands, point_count,
			point_idx, lu, NULL))
		return 0;
	}

	if (!_ged_draw_nmg_append_wire_edge_lines(points, commands, point_count,
		    point_idx, &s->eu_hd))
	    return 0;

	if (s->vu_p) {
	    point_t point;
	    if (!_ged_draw_nmg_vertexuse_coord(s->vu_p, point))
		return 0;
	    if (!_ged_draw_nmg_append_line(points, commands, point_count,
			point_idx, point, BSG_GEOMETRY_LINE_MOVE))
		return 0;
	    if (!_ged_draw_nmg_append_line(points, commands, point_count,
			point_idx, point, BSG_GEOMETRY_LINE_DRAW))
		return 0;
	}
    }

    return 1;
}


static int
_ged_draw_nmg_fill_line_set(const struct nmgregion *r,
			    int style,
			    point_t *points,
			    int *commands,
			    size_t point_count)
{
    size_t point_idx = 0;

    if (!_ged_draw_nmg_append_region_line_set(r, style, points, commands,
	    point_count, &point_idx))
	return 0;
    return (point_idx == point_count) ? 1 : 0;
}


static int
_ged_draw_nmg_model_line_set_stats(const struct model *m,
				   int style,
				   size_t *point_count)
{
    const struct nmgregion *r;
    size_t points = 0;
    size_t regions = 0;

    if (point_count)
	*point_count = 0;
    if (!m)
	return 0;

    NMG_CK_MODEL(m);
    for (BU_LIST_FOR(r, nmgregion, &m->r_hd)) {
	size_t region_points = 0;
	NMG_CK_REGION(r);
	if (!_ged_draw_nmg_line_set_stats(r, style, &region_points))
	    return 0;
	points += region_points;
	regions++;
    }

    if (!regions || !points)
	return 0;
    if (point_count)
	*point_count = points;
    return 1;
}


static int
_ged_draw_nmg_fill_model_line_set(const struct model *m,
				  int style,
				  point_t *points,
				  int *commands,
				  size_t point_count)
{
    const struct nmgregion *r;
    size_t point_idx = 0;

    if (!m || !points || !commands)
	return 0;

    NMG_CK_MODEL(m);
    for (BU_LIST_FOR(r, nmgregion, &m->r_hd)) {
	NMG_CK_REGION(r);
	if (!_ged_draw_nmg_append_region_line_set(r, style, points, commands,
		point_count, &point_idx))
	    return 0;
    }

    return (point_idx == point_count) ? 1 : 0;
}


static int
_ged_draw_nmg_loop_vertex_count(const struct loopuse *lu, size_t *count)
{
    const struct edgeuse *eu;
    size_t n = 0;

    if (count)
	*count = 0;
    if (!lu)
	return 0;

    NMG_CK_LOOPUSE(lu);
    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
	return 0;

    for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
	NMG_CK_EDGEUSE(eu);
	NMG_CK_VERTEXUSE(eu->vu_p);
	NMG_CK_VERTEX(eu->vu_p->v_p);
	if (!eu->vu_p->v_p->vg_p)
	    return 0;
	NMG_CK_VERTEX_G(eu->vu_p->v_p->vg_p);
	n++;
    }

    if (n < 3)
	return 0;
    if (count)
	*count = n;
    return 1;
}


static int
_ged_draw_nmg_indexed_face_stats(const struct nmgregion *r,
				 int style,
				 size_t *point_count,
				 size_t *index_count)
{
    const struct shell *s;
    size_t points = 0;
    size_t indices = 0;

    if (point_count)
	*point_count = 0;
    if (index_count)
	*index_count = 0;
    if (!r)
	return 0;

    if (!_ged_draw_nmg_surface_style(style))
	return 0;

    NMG_CK_REGION(r);
    for (BU_LIST_FOR(s, shell, &r->s_hd)) {
	const struct faceuse *fu;

	NMG_CK_SHELL(s);
	for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
	    const struct loopuse *lu;

	    NMG_CK_FACEUSE(fu);
	    if (fu->orientation != OT_SAME)
		continue;
	    NMG_CK_FACE(fu->f_p);
	    if (!fu->f_p->g.magic_p ||
		    *fu->f_p->g.magic_p == NMG_FACE_G_SNURB_MAGIC)
		return 0;
	    NMG_CK_FACE_G_PLANE(fu->f_p->g.plane_p);

	    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		size_t n = 0;
		if (!_ged_draw_nmg_loop_vertex_count(lu, &n))
		    return 0;
		points += n;
		indices += n + 1;
	    }
	}
    }

    if (!points || !indices)
	return 0;
    if (point_count)
	*point_count = points;
    if (index_count)
	*index_count = indices;
    return 1;
}


static int
_ged_draw_nmg_fill_indexed_face_set(const struct nmgregion *r,
				    int style,
				    point_t *points,
				    vect_t *normals,
				    int *indices,
				    size_t point_count,
				    size_t index_count)
{
    const struct shell *s;
    size_t point_idx = 0;
    size_t index_idx = 0;

    if (!r || !points || !normals || !indices)
	return 0;

    for (BU_LIST_FOR(s, shell, &r->s_hd)) {
	const struct faceuse *fu;

	for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
	    const struct loopuse *lu;
	    vect_t face_normal = VINIT_ZERO;

	    if (fu->orientation != OT_SAME)
		continue;
	    NMG_GET_FU_NORMAL(face_normal, fu);

	    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		const struct edgeuse *eu;

		for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
		    const struct vertexuse *vu = eu->vu_p;
		    vect_t normal = VINIT_ZERO;

		    if (point_idx >= point_count || index_idx >= index_count)
			return 0;

		    VMOVE(points[point_idx], vu->v_p->vg_p->coord);
		    if ((style & GED_DRAW_NMG_STYLE_USE_VU_NORMALS) &&
			    vu->a.magic_p &&
			    *vu->a.magic_p == NMG_VERTEXUSE_A_PLANE_MAGIC) {
			VMOVE(normal, vu->a.plane_p->N);
		    } else {
			VMOVE(normal, face_normal);
		    }
		    VMOVE(normals[point_idx], normal);
		    indices[index_idx++] = (int)point_idx++;
		}

		if (index_idx >= index_count)
		    return 0;
		indices[index_idx++] = -1;
	    }
	}
    }

    return (point_idx == point_count && index_idx == index_count) ? 1 : 0;
}


static int
_ged_draw_scene_ref_publish_nmg_wire_remainders(bsg_scene_ref primary_ref,
						const struct nmgregion *r)
{
    bsg_scene_ref source_ref = ged_draw_shape_source_ref(primary_ref);
    bsg_scene_ref wire_ref = bsg_scene_ref_null();
    point_t *points = NULL;
    int *commands = NULL;
    size_t point_count = 0;
    int ok = 0;

    if (!_ged_draw_nmg_wire_remainder_line_stats(r, &point_count))
	return 0;
    if (!point_count)
	return 1;

    points = (point_t *)bu_calloc(point_count, sizeof(point_t),
	    "GED draw NMG mixed-wire points");
    commands = (int *)bu_calloc(point_count, sizeof(int),
	    "GED draw NMG mixed-wire commands");

    if (!_ged_draw_nmg_fill_wire_remainder_line_set(r, points, commands,
		point_count))
	goto cleanup;

    wire_ref = _ged_draw_scene_ref_create_aux_geometry(primary_ref,
	    "nmg_mixed_wire");
    if (bsg_scene_ref_is_null(wire_ref))
	goto cleanup;

    ok = bsg_geometry_ref_set_line_set(bsg_scene_ref_as_geometry(wire_ref),
	    (const point_t *)points, commands, point_count);
    if (!ok)
	goto cleanup;

    (void)ged_draw_scene_ref_update_bounds_from_geometry(wire_ref, NULL);
    bsg_scene_invalidate(wire_ref);

cleanup:
    if (!ok && !bsg_scene_ref_is_null(wire_ref)) {
	if (!bsg_scene_ref_is_null(source_ref))
	    bsg_scene_remove_child(source_ref, wire_ref);
	bsg_scene_ref_destroy(wire_ref);
    }
    if (points)
	bu_free(points, "GED draw NMG mixed-wire points");
    if (commands)
	bu_free(commands, "GED draw NMG mixed-wire commands");
    return ok;
}


static int
_ged_draw_scene_ref_publish_nmg_normal_lines(bsg_scene_ref primary_ref,
					     const struct nmgregion *r,
					     int style)
{
    bsg_scene_ref source_ref = ged_draw_shape_source_ref(primary_ref);
    bsg_scene_ref normal_ref = bsg_scene_ref_null();
    point_t *points = NULL;
    int *commands = NULL;
    size_t point_count = 0;
    int ok = 0;

    if (!_ged_draw_nmg_normal_line_stats(r, style, &point_count))
	return 0;

    points = (point_t *)bu_calloc(point_count, sizeof(point_t),
	    "GED draw NMG normal-line points");
    commands = (int *)bu_calloc(point_count, sizeof(int),
	    "GED draw NMG normal-line commands");

    if (!_ged_draw_nmg_fill_normal_line_set(r, style, points, commands,
		point_count))
	goto cleanup;

    normal_ref = _ged_draw_scene_ref_create_aux_geometry(primary_ref,
	    "nmg_surface_normals");
    if (bsg_scene_ref_is_null(normal_ref))
	goto cleanup;

    ok = bsg_geometry_ref_set_line_set(bsg_scene_ref_as_geometry(normal_ref),
	    (const point_t *)points, commands, point_count);
    if (!ok)
	goto cleanup;

    (void)ged_draw_scene_ref_update_bounds_from_geometry(normal_ref, NULL);
    bsg_scene_invalidate(normal_ref);

cleanup:
    if (!ok && !bsg_scene_ref_is_null(normal_ref)) {
	if (!bsg_scene_ref_is_null(source_ref))
	    bsg_scene_remove_child(source_ref, normal_ref);
	bsg_scene_ref_destroy(normal_ref);
    }
    if (points)
	bu_free(points, "GED draw NMG normal-line points");
    if (commands)
	bu_free(commands, "GED draw NMG normal-line commands");
    return ok;
}


int
ged_draw_scene_ref_geometry_publish_nmg_region(bsg_scene_ref ref,
					       const struct nmgregion *r,
					       int style)
{
    ged_draw_shape_state *shape_data = ged_draw_shape_state_get_scene_ref(ref);
    point_t *points = NULL;
    int *commands = NULL;
    vect_t *normals = NULL;
    int *indices = NULL;
    size_t point_count = 0;
    size_t index_count = 0;
    int ok = 0;

    if (!shape_data || !r)
	return 0;

    if (_ged_draw_nmg_indexed_face_stats(r, style, &point_count, &index_count)) {
	points = (point_t *)bu_calloc(point_count, sizeof(point_t),
		"GED draw NMG indexed-face points");
	normals = (vect_t *)bu_calloc(point_count, sizeof(vect_t),
		"GED draw NMG indexed-face normals");
	indices = (int *)bu_calloc(index_count, sizeof(int),
		"GED draw NMG indexed-face indices");

	if (!_ged_draw_nmg_fill_indexed_face_set(r, style, points, normals, indices,
		    point_count, index_count))
	    goto cleanup;

	ok = bsg_geometry_ref_set_indexed_face_set(bsg_scene_ref_as_geometry(ref),
		(const point_t *)points, point_count,
		(const vect_t *)normals, point_count,
		indices, index_count);
	if (!ok)
	    goto cleanup;

	shape_data->geometry_command_count = point_count;
	shape_data->geometry_revision++;
	bsg_scene_invalidate(ref);
	if ((style & GED_DRAW_NMG_STYLE_VISUALIZE_NORMALS) &&
		!_ged_draw_scene_ref_publish_nmg_normal_lines(ref, r, style)) {
	    ok = 0;
	    goto cleanup;
	}
	if (!_ged_draw_scene_ref_publish_nmg_wire_remainders(ref, r)) {
	    ok = 0;
	    goto cleanup;
	}
	goto cleanup;
    }

    if (!_ged_draw_nmg_line_set_stats(r, style, &point_count))
	return 0;

    points = (point_t *)bu_calloc(point_count, sizeof(point_t),
	    "GED draw NMG line-set points");
    commands = (int *)bu_calloc(point_count, sizeof(int),
	    "GED draw NMG line-set commands");

    if (!_ged_draw_nmg_fill_line_set(r, style, points, commands, point_count))
	goto cleanup;

    ok = bsg_geometry_ref_set_line_set(bsg_scene_ref_as_geometry(ref),
	    (const point_t *)points, commands, point_count);
    if (!ok)
	goto cleanup;

    shape_data->geometry_command_count = point_count;
    shape_data->geometry_revision++;
    bsg_scene_invalidate(ref);

cleanup:
    if (points)
	bu_free(points, "GED draw NMG geometry points");
    if (commands)
	bu_free(commands, "GED draw NMG line-set commands");
    if (normals)
	bu_free(normals, "GED draw NMG indexed-face normals");
    if (indices)
	bu_free(indices, "GED draw NMG indexed-face indices");
    return ok;
}


int
ged_draw_scene_ref_geometry_publish_nmg_model(bsg_scene_ref ref,
					     const struct model *m,
					     int style)
{
    point_t *points = NULL;
    int *commands = NULL;
    size_t point_count = 0;
    int ok = 0;

    if (bsg_scene_ref_is_null(ref) || !m)
	return 0;

    if (!_ged_draw_nmg_model_line_set_stats(m, style, &point_count))
	return 0;

    points = (point_t *)bu_calloc(point_count, sizeof(point_t),
	    "GED draw NMG model line-set points");
    commands = (int *)bu_calloc(point_count, sizeof(int),
	    "GED draw NMG model line-set commands");

    if (!_ged_draw_nmg_fill_model_line_set(m, style, points, commands,
		point_count))
	goto cleanup;

    ok = ged_draw_scene_ref_publish_line_set(ref, (const point_t *)points,
	    commands, point_count);

cleanup:
    if (points)
	bu_free(points, "GED draw NMG model line-set points");
    if (commands)
	bu_free(commands, "GED draw NMG model line-set commands");
    return ok;
}


static void
_ged_draw_scene_ref_geometry_set_command_count(bsg_scene_ref ref, size_t vlen)
{
    ged_draw_shape_state *shape_data =
	ged_draw_shape_state_get_scene_ref(ref);
    if (!shape_data)
	return;
    shape_data->geometry_command_count = vlen;
    shape_data->geometry_revision++;
}


int
ged_draw_source_primitive_has_lod_realize(const struct rt_db_internal *ip)
{
    return (ip && ip->idb_meth && ip->idb_meth->ft_lod_realize) ? 1 : 0;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
