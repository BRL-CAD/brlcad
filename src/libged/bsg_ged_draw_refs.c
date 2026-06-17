/*                B S G _ G E D _ D R A W _ R E F S . C
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file libged/b_s_g___g_e_d___d_r_a_w___r_e_f_s_._c.c
 *
 * Draw shape-ref mutation compatibility bridge.
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#include "bu/malloc.h"
#include "bu/ptbl.h"
#include "bu/str.h"
#include "bu/color.h"
#include "bu/hash.h"
#include "bsg/appearance.h"
#include "bsg/defines.h"
#include "bsg/database_source.h"
#include "bsg/draw_ctx.h"
#include "bsg/draw_intent.h"
#include "bsg/draw_set.h"
#include "bsg/draw_source.h"
#include "bsg/field.h"
#include "bsg/geometry.h"
#include "bsg/interaction.h"
#include "bsg/lod.h"
#include "bsg/material.h"
#include "bsg/payload.h"
#include "bsg/plot3.h"
#include "bsg/scene_builder.h"
#include "bsg/scene_object.h"
#include "bsg/selection.h"
#include "bsg/view_set.h"
#include "bsg/view_state.h"
#include "bsg/util.h"
#include "bg/clip.h"

#include "ged.h"
#include "ged/bsg_ged_draw.h"
#include "./ged_private.h"

int
ged_draw_shape_ref_set_flag(struct ged *gedp, ged_draw_shape_ref ref, int flag)
{
    bsg_scene_ref shape_ref = ged_draw_registry_shape_scene_ref(gedp, ref);
    if (bsg_scene_ref_is_null(shape_ref))
	return 0;
    bsg_scene_set_visible(shape_ref, flag ? 1 : 0);
    return 1;
}


int
ged_draw_shape_ref_set_work_flag(struct ged *gedp, ged_draw_shape_ref ref, int wflag)
{
    bsg_scene_ref shape_ref = ged_draw_registry_shape_scene_ref(gedp, ref);
    if (bsg_scene_ref_is_null(shape_ref))
	return 0;
    ged_draw_scene_ref_set_work_flag(shape_ref, wflag);
    return 1;
}


int
ged_draw_shape_ref_line_style(struct ged *gedp, ged_draw_shape_ref ref)
{
    bsg_scene_ref shape_ref = ged_draw_registry_shape_scene_ref(gedp, ref);
    if (bsg_scene_ref_is_null(shape_ref))
	return -1;
    return ged_draw_scene_ref_line_style(shape_ref);
}


static void
_ged_draw_shape_ref_geometry_changed(bsg_scene_ref shape_ref)
{
    ged_draw_shape_state *shape_data = ged_draw_scene_ref_shape_state(shape_ref);
    if (shape_data)
	shape_data->geometry_revision++;
    bsg_scene_invalidate(shape_ref);
}


static int
_ged_draw_geometry_last_face_point(bsg_geometry_ref geometry, point_t out)
{
    bsg_field_ref coords = bsg_geometry_ref_coordinates_field(geometry);
    bsg_field_ref indices = bsg_geometry_ref_indices_field(geometry);
    size_t index_count = bsg_field_multi_count(indices);
    int first = -1;
    int vert = 0;
    int have = 0;

    for (size_t i = 0; i < index_count; i++) {
	int idx = -1;
	if (!bsg_field_multi_int_at(indices, i, &idx))
	    continue;
	if (idx < 0) {
	    if (first >= 0 && vert >= 3 && bsg_field_multi_point_at(coords, (size_t)first, out))
		have = 1;
	    first = -1;
	    vert = 0;
	    continue;
	}
	if (first < 0)
	    first = idx;
	vert++;
    }

    if (first >= 0 && vert >= 3 && bsg_field_multi_point_at(coords, (size_t)first, out))
	have = 1;

    return have;
}


int
ged_draw_shape_ref_last_point(struct ged *gedp, ged_draw_shape_ref ref, point_t out)
{
    bsg_scene_ref shape_ref = ged_draw_registry_shape_scene_ref(gedp, ref);
    if (bsg_scene_ref_is_null(shape_ref) || !out)
	return 0;

    bsg_geometry_ref geometry = bsg_scene_ref_as_geometry(shape_ref);
    if (bsg_node_ref_is_null(geometry.shape.node))
	return 0;

    if (bsg_geometry_ref_kind(geometry) == BSG_GEOMETRY_NODE_INDEXED_FACE_SET)
	return _ged_draw_geometry_last_face_point(geometry, out);

    bsg_field_ref coords = bsg_geometry_ref_coordinates_field(geometry);
    size_t count = bsg_field_multi_count(coords);
    if (!count)
	return 0;
    return bsg_field_multi_point_at(coords, count - 1, out);
}


static int
_ged_draw_translate_line_set(bsg_geometry_ref geometry, const vect_t xlate)
{
    bsg_field_ref coords = bsg_geometry_ref_coordinates_field(geometry);
    bsg_field_ref primitives = bsg_geometry_ref_primitive_sets_field(geometry);
    size_t count = bsg_field_multi_count(coords);
    point_t *points = NULL;
    int *commands = NULL;
    int ok = 0;

    if (!count)
	return 0;

    points = (point_t *)bu_calloc(count, sizeof(point_t), "translated line-set points");
    commands = (int *)bu_calloc(count, sizeof(int), "translated line-set commands");
    for (size_t i = 0; i < count; i++) {
	point_t pt;
	int cmd = BSG_GEOMETRY_LINE_MOVE;
	if (!bsg_field_multi_point_at(coords, i, pt))
	    goto cleanup;
	(void)bsg_field_multi_int_at(primitives, i, &cmd);
	VADD2(points[i], pt, xlate);
	commands[i] = cmd;
    }

    ok = bsg_geometry_ref_set_line_set(geometry, (const point_t *)points,
	    commands, count);

cleanup:
    if (points)
	bu_free(points, "translated line-set points");
    if (commands)
	bu_free(commands, "translated line-set commands");
    return ok;
}


static int
_ged_draw_translate_point_set(bsg_geometry_ref geometry, const vect_t xlate)
{
    bsg_field_ref coords = bsg_geometry_ref_coordinates_field(geometry);
    size_t count = bsg_field_multi_count(coords);
    point_t *points = NULL;
    int ok = 0;

    if (!count)
	return 0;

    points = (point_t *)bu_calloc(count, sizeof(point_t), "translated point-set points");
    for (size_t i = 0; i < count; i++) {
	point_t pt;
	if (!bsg_field_multi_point_at(coords, i, pt))
	    goto cleanup;
	VADD2(points[i], pt, xlate);
    }

    ok = bsg_geometry_ref_set_point_set(geometry, (const point_t *)points, count);

cleanup:
    if (points)
	bu_free(points, "translated point-set points");
    return ok;
}


static int
_ged_draw_translate_face_set(bsg_geometry_ref geometry, const vect_t xlate)
{
    bsg_field_ref coords = bsg_geometry_ref_coordinates_field(geometry);
    bsg_field_ref normals_field = bsg_geometry_ref_normals_field(geometry);
    bsg_field_ref indices_field = bsg_geometry_ref_indices_field(geometry);
    size_t point_count = bsg_field_multi_count(coords);
    size_t normal_count = bsg_field_multi_count(normals_field);
    size_t index_count = bsg_field_multi_count(indices_field);
    point_t *points = NULL;
    vect_t *normals = NULL;
    int *indices = NULL;
    int ok = 0;

    if (!point_count || !index_count)
	return 0;

    points = (point_t *)bu_calloc(point_count, sizeof(point_t),
	    "translated face-set points");
    normals = normal_count ? (vect_t *)bu_calloc(normal_count, sizeof(vect_t),
	    "translated face-set normals") : NULL;
    indices = (int *)bu_calloc(index_count, sizeof(int),
	    "translated face-set indices");

    for (size_t i = 0; i < point_count; i++) {
	point_t pt;
	if (!bsg_field_multi_point_at(coords, i, pt))
	    goto cleanup;
	VADD2(points[i], pt, xlate);
    }
    for (size_t i = 0; i < normal_count; i++) {
	vect_t normal;
	if (!bsg_field_multi_point_at(normals_field, i, normal))
	    goto cleanup;
	VMOVE(normals[i], normal);
    }
    for (size_t i = 0; i < index_count; i++) {
	if (!bsg_field_multi_int_at(indices_field, i, &indices[i]))
	    goto cleanup;
    }

    ok = bsg_geometry_ref_set_indexed_face_set(geometry,
	    (const point_t *)points, point_count,
	    (const vect_t *)normals, normal_count,
	    indices, index_count);

cleanup:
    if (points)
	bu_free(points, "translated face-set points");
    if (normals)
	bu_free(normals, "translated face-set normals");
    if (indices)
	bu_free(indices, "translated face-set indices");
    return ok;
}


int
ged_draw_shape_ref_translate_geometry(struct ged *gedp, ged_draw_shape_ref ref,
				      const vect_t xlate)
{
    bsg_scene_ref shape_ref = ged_draw_registry_shape_scene_ref(gedp, ref);
    if (bsg_scene_ref_is_null(shape_ref) || !xlate)
	return 0;

    bsg_geometry_ref geometry = bsg_scene_ref_as_geometry(shape_ref);
    if (bsg_node_ref_is_null(geometry.shape.node))
	return 0;

    int ok = 0;
    switch (bsg_geometry_ref_kind(geometry)) {
	case BSG_GEOMETRY_NODE_LINE_SET:
	    ok = _ged_draw_translate_line_set(geometry, xlate);
	    break;
	case BSG_GEOMETRY_NODE_POINT_SET:
	    ok = _ged_draw_translate_point_set(geometry, xlate);
	    break;
	case BSG_GEOMETRY_NODE_INDEXED_FACE_SET:
	    ok = _ged_draw_translate_face_set(geometry, xlate);
	    break;
	default:
	    return 0;
    }
    if (ok)
	_ged_draw_shape_ref_geometry_changed(shape_ref);
    return ok;
}


int
ged_draw_shape_ref_set_center(struct ged *gedp, ged_draw_shape_ref ref,
			      const point_t center)
{
    bsg_scene_ref shape_ref = ged_draw_registry_shape_scene_ref(gedp, ref);
    if (bsg_scene_ref_is_null(shape_ref) || !center)
	return 0;
    ged_draw_scene_ref_set_draw_center(shape_ref, center);
    return 1;
}


int
ged_draw_shape_ref_geometry_clear(struct ged *gedp, ged_draw_shape_ref ref)
{
    bsg_scene_ref shape_ref = ged_draw_registry_shape_scene_ref(gedp, ref);
    if (bsg_scene_ref_is_null(shape_ref))
	return 0;
    return ged_draw_scene_ref_geometry_clear(shape_ref);
}


int
ged_draw_shape_ref_update_bounds_from_geometry(struct ged *gedp,
					       ged_draw_shape_ref ref,
					       int *bad_cmd)
{
    bsg_scene_ref shape_ref = ged_draw_registry_shape_scene_ref(gedp, ref);
    return ged_draw_scene_ref_update_bounds_from_geometry(shape_ref, bad_cmd);
}


int
ged_draw_shape_ref_publish_primitive_wireframe(struct ged *gedp,
					       ged_draw_shape_ref ref,
					       struct rt_db_internal *ip,
					       const struct bg_tess_tol *ttol,
					       const struct bn_tol *tol,
					       struct bsg_view *v,
					       int adaptive)
{
    bsg_scene_ref shape_ref = ged_draw_registry_shape_scene_ref(gedp, ref);
    if (bsg_scene_ref_is_null(shape_ref))
	return -1;
    return ged_draw_scene_ref_publish_primitive_wireframe(shape_ref, ip, ttol,
	    tol, v, adaptive);
}


int
ged_draw_shape_ref_release(struct ged *gedp, ged_draw_shape_ref ref)
{
    bsg_scene_ref shape_ref = ged_draw_registry_shape_scene_ref(gedp, ref);
    if (bsg_scene_ref_is_null(shape_ref))
	return 0;

    bsg_scene_ref release_ref = ged_draw_shape_source_ref(shape_ref);
    if (bsg_scene_ref_is_null(release_ref))
	release_ref = shape_ref;
    bsg_scene_ref parent_ref = bsg_scene_parent(release_ref);
    if (!bsg_scene_ref_is_null(parent_ref))
	bsg_scene_remove_child(parent_ref, release_ref);
    bsg_scene_ref_destroy(release_ref);
    return 1;
}


int
ged_draw_shape_ref_realize(struct ged *gedp, ged_draw_shape_ref ref,
			   struct bsg_view *v)
{
    bsg_scene_ref shape_ref = ged_draw_registry_shape_scene_ref(gedp, ref);
    if (bsg_scene_ref_is_null(shape_ref))
	return 0;
    ged_draw_scene_ref_realize(shape_ref, v);
    return 1;
}


int
ged_draw_shape_ref_set_view(struct ged *gedp, ged_draw_shape_ref ref,
			    struct bsg_view *v)
{
    bsg_scene_ref shape_ref = ged_draw_registry_shape_scene_ref(gedp, ref);
    if (bsg_scene_ref_is_null(shape_ref))
	return 0;
    bsg_scene_set_view(shape_ref, v);
    return 1;
}


int
ged_draw_shape_ref_reset_node(struct ged *gedp, ged_draw_shape_ref ref)
{
    bsg_scene_ref shape_ref = ged_draw_registry_shape_scene_ref(gedp, ref);
    if (bsg_scene_ref_is_null(shape_ref))
	return 0;
    ged_draw_scene_ref_geometry_clear(shape_ref);
    bsg_mesh_lod_free_scene_ref(shape_ref);
    ged_draw_scene_ref_realization_reset(shape_ref);
    bsg_scene_invalidate(shape_ref);
    return 1;
}


int
ged_draw_shape_ref_set_visible(struct ged *gedp, ged_draw_shape_ref ref,
			       int visible)
{
    bsg_scene_ref shape_ref = ged_draw_registry_shape_scene_ref(gedp, ref);
    if (bsg_scene_ref_is_null(shape_ref))
	return 0;
    bsg_scene_set_visible(shape_ref, visible ? 1 : 0);
    return 1;
}


int
ged_draw_shape_ref_get_color(struct ged *gedp, ged_draw_shape_ref ref,
			     unsigned char rgb[3])
{
    bsg_scene_ref shape_ref = ged_draw_registry_shape_scene_ref(gedp, ref);
    if (bsg_scene_ref_is_null(shape_ref) || !rgb)
	return 0;
    bsg_scene_color(shape_ref, &rgb[0], &rgb[1], &rgb[2]);
    return 1;
}


int
ged_draw_shape_ref_set_color(struct ged *gedp, ged_draw_shape_ref ref,
			     const unsigned char rgb[3])
{
    bsg_scene_ref shape_ref = ged_draw_registry_shape_scene_ref(gedp, ref);
    if (bsg_scene_ref_is_null(shape_ref) || !rgb)
	return 0;
    bsg_scene_set_color(shape_ref, rgb[0], rgb[1], rgb[2]);
    return 1;
}


int
ged_draw_shape_ref_sync_settings(struct ged *gedp, ged_draw_shape_ref ref,
				 struct bsg_appearance_settings *settings,
				 int current_mode,
				 int *changed)
{
    bsg_scene_ref shape_ref = ged_draw_registry_shape_scene_ref(gedp, ref);
    if (changed)
	*changed = 0;
    if (bsg_scene_ref_is_null(shape_ref))
	return 0;
    if (!settings || settings->draw_mode != current_mode)
	return 1;

    if (ged_draw_scene_ref_line_style(shape_ref) && settings->draw_non_subtract_only) {
	if (bsg_scene_visible(shape_ref)) {
	    bsg_scene_set_visible(shape_ref, 0);
	    if (changed)
		*changed = 1;
	}
    } else {
	if (!bsg_scene_visible(shape_ref)) {
	    bsg_scene_set_visible(shape_ref, 1);
	    if (changed)
		*changed = 1;
	}
    }

    if (ged_draw_scene_ref_apply_settings(shape_ref, settings) && changed)
	*changed = 1;
    return 1;
}


int
ged_draw_shape_ref_refresh_material(struct ged *gedp, ged_draw_shape_ref ref,
				    unsigned char rgb[3])
{
    bsg_scene_ref shape_ref = ged_draw_registry_shape_scene_ref(gedp, ref);
    if (bsg_scene_ref_is_null(shape_ref) || !rgb)
	return 0;
    ged_draw_scene_ref_set_material_rgb(shape_ref, rgb);
    return 1;
}


int
ged_draw_shape_ref_adaptive_sync(struct ged *gedp, ged_draw_shape_ref ref,
				 struct bsg_view **views,
				 size_t view_count,
				 int *changed)
{
    bsg_scene_ref shape_ref = ged_draw_registry_shape_scene_ref(gedp, ref);

    if (changed)
	*changed = 0;
    if (bsg_scene_ref_is_null(shape_ref))
	return 0;
    if (!ged_draw_scene_ref_realization_pipeline_candidate(shape_ref))
	return 1;

    for (size_t i = 0; i < view_count; i++) {
	struct bsg_view *v = views ? views[i] : NULL;
	if (!v)
	    continue;
	ged_draw_scene_ref_mark_view_inputs_changed(shape_ref);
	bsg_scene_invalidate(shape_ref);
	if (changed)
	    *changed = 1;
	break;
    }
    return 1;
}


int
ged_draw_shape_ref_lod_ensure(struct ged *gedp, ged_draw_shape_ref ref,
			      struct bsg_view *first_view,
			      struct bsg_view **views,
			      size_t view_count)
{
    bsg_scene_ref shape_ref = ged_draw_registry_shape_scene_ref(gedp, ref);
    if (bsg_scene_ref_is_null(shape_ref) || !first_view)
	return 0;

    struct bsg_lod_source_policy_settings policy;
    memset(&policy, 0, sizeof(policy));
    (void)bsg_view_lod_source_policy_get(first_view, &policy);
    int candidate = ged_draw_scene_ref_realization_pipeline_candidate(shape_ref);
    int source_backed = ged_draw_scene_ref_source_data(shape_ref) ? 1 : 0;
    if (!candidate && !source_backed)
	return 1;
    if (!candidate) {
	struct directory *dp = ged_draw_scene_ref_leaf_dp(shape_ref);
	int mode = ged_draw_scene_ref_draw_mode(shape_ref);
	int mesh_ref = dp && policy.mesh_enabled &&
	    ((dp->d_minor_type == DB5_MINORTYPE_BRLCAD_BOT &&
	      (mode == 0 || mode == 1)) ||
	     (dp->d_minor_type == DB5_MINORTYPE_BRLCAD_BREP &&
	      mode == 1));
	if (!mesh_ref && !(policy.csg_enabled && mode == 0))
	    return 1;
    }
    int realized = 0;
    for (size_t i = 0; i < view_count; i++) {
	if (views && views[i]) {
	    ged_draw_scene_ref_mark_view_inputs_changed(shape_ref);
	    bsg_scene_invalidate(shape_ref);
	    ged_draw_scene_ref_realize(shape_ref, views[i]);
	    realized = 1;
	}
    }
    if (!realized) {
	ged_draw_scene_ref_mark_view_inputs_changed(shape_ref);
	bsg_scene_invalidate(shape_ref);
	ged_draw_scene_ref_realize(shape_ref, first_view);
    }
    return 1;
}


int
ged_draw_shape_ref_pipeline_candidate(struct ged *gedp, ged_draw_shape_ref ref)
{
    bsg_scene_ref shape_ref = ged_draw_registry_shape_scene_ref(gedp, ref);
    if (bsg_scene_ref_is_null(shape_ref))
	return 0;
    return ged_draw_scene_ref_realization_pipeline_candidate(shape_ref) ? 1 : 0;
}


struct bsg_view *
ged_draw_shape_ref_view(struct ged *gedp, ged_draw_shape_ref ref)
{
    bsg_scene_ref shape_ref = ged_draw_registry_shape_scene_ref(gedp, ref);
    return bsg_scene_ref_is_null(shape_ref) ? NULL : bsg_scene_view(shape_ref);
}


struct bsg_interaction_record *
ged_draw_shape_ref_selection_record(struct ged *gedp, ged_draw_shape_ref ref,
				    struct bsg_view *fallback_view)
{
    bsg_scene_ref shape_ref = ged_draw_registry_shape_scene_ref(gedp, ref);
    struct ged_draw_shape_record rec;
    struct bsg_view *v;
    const char *source_path = NULL;
    char *path = NULL;

    if (bsg_scene_ref_is_null(shape_ref))
	return NULL;

    v = bsg_scene_view(shape_ref);
    if (!v)
	v = fallback_view;

    if (ged_draw_shape_record_get(gedp, ref, &rec) && rec.fullpath) {
	path = db_path_to_string(rec.fullpath);
	if (path)
	    source_path = path;
    }
    if (!source_path)
	source_path = bsg_scene_name(shape_ref);

    bsg_feature_ref feature = BSG_FEATURE_REF_NULL_INIT;
    struct bsg_interaction_record *record =
	bsg_interaction_record_create_ref(v, BSG_INTERACTION_SELECTED_PATH,
		feature, ged_draw_dbpath_skip_lead_slash(source_path), NULL);

    if (path)
	bu_free(path, "ged draw shape selection path");
    return record;
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
