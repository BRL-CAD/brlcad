/*                B S G _ G E D _ D R A W _ D R A F T . C
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
/** @file libged/b_s_g___g_e_d___d_r_a_w___d_r_a_f_t_._c.c
 *
 * GED draw-shape draft construction and commit helpers.
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>

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
#include "bsg/node.h"
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

struct ged_draw_shape_draft {
    struct ged *gedp;
    struct bsg_view *view;
    bsg_scene_ref source_ref;
    bsg_scene_ref shape_ref;
    int committed;
};


static void
_ged_draw_shape_draft_destroy_refs(bsg_scene_ref source_ref, bsg_scene_ref shape_ref)
{
    if (!bsg_scene_ref_is_null(source_ref) &&
	    !bsg_scene_ref_is_null(shape_ref))
	bsg_scene_remove_child(source_ref, shape_ref);
    if (!bsg_scene_ref_is_null(shape_ref))
	bsg_scene_ref_destroy(shape_ref);
    if (!bsg_scene_ref_is_null(source_ref))
	bsg_scene_ref_destroy(source_ref);
}


static void
_ged_draw_shape_draft_copy_aux_draw_intent(bsg_scene_ref dst, bsg_scene_ref src)
{
    const struct bsg_draw_intent *di = bsg_scene_draw_intent(src);
    const char *path = bsg_draw_intent_path(di);

    if (!path || !path[0])
	return;

    struct bsg_draw_intent *copy =
	bsg_draw_intent_create(path, bsg_draw_intent_mode(di));
    if (!copy)
	return;

    struct bsg_appearance_settings appearance;
    copy->di_lod = bsg_draw_intent_lod(di);
    copy->di_mixed = di->di_mixed;
    if (bsg_draw_intent_appearance(di, &appearance))
	bsg_draw_intent_set_appearance(copy, &appearance);
    bsg_scene_set_draw_intent(dst, copy);
}


static void
_ged_draw_shape_draft_copy_aux_display_state(bsg_scene_ref dst,
					     bsg_scene_ref src)
{
    unsigned char r = 0;
    unsigned char g = 0;
    unsigned char b = 0;
    unsigned char basecolor[3] = {0, 0, 0};
    mat_t mat;

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

    _ged_draw_shape_draft_copy_aux_draw_intent(dst, src);
    bsg_scene_set_non_database_source(dst, 0);
}


static void
_ged_draw_shape_draft_sync_aux_geometry(ged_draw_shape_draft *draft)
{
    if (!draft || bsg_scene_ref_is_null(draft->source_ref) ||
	    bsg_scene_ref_is_null(draft->shape_ref))
	return;

    size_t child_count = bsg_scene_child_count(draft->source_ref);
    for (size_t i = 0; i < child_count; i++) {
	bsg_scene_ref child_ref = bsg_scene_child_at(draft->source_ref, i);
	if (bsg_scene_ref_equal(child_ref, draft->shape_ref))
	    continue;
	_ged_draw_shape_draft_copy_aux_display_state(child_ref,
		draft->shape_ref);
    }
}


ged_draw_shape_draft *
ged_draw_shape_draft_create(struct ged *gedp, struct bsg_view *v, int registered)
{
    (void)registered;

    if (!gedp || !v)
	return NULL;
    if (bsg_scene_ref_is_null(ged_draw_ensure_root(gedp)))
	return NULL;

    bsg_database_source_ref source_ref =
	bsg_database_source_ref_create(v, "_db_source");
    bsg_scene_ref source_scene = bsg_database_source_ref_as_scene(source_ref);
    if (bsg_scene_ref_is_null(source_scene))
	return NULL;

    bsg_geometry_ref geometry_ref = bsg_geometry_ref_create(v, "geometry");
    bsg_scene_ref shape_scene = bsg_geometry_ref_as_scene(geometry_ref);
    if (bsg_scene_ref_is_null(shape_scene)) {
	_ged_draw_shape_draft_destroy_refs(source_scene, shape_scene);
	return NULL;
    }

    bsg_scene_append_child(source_scene, shape_scene);

    if (!ged_draw_scene_ref_prepare(gedp, shape_scene)) {
	_ged_draw_shape_draft_destroy_refs(source_scene, shape_scene);
	return NULL;
    }
    ged_draw_scene_ref_set_source_ref(shape_scene, source_scene);
    ged_draw_scene_ref_geometry_clear(shape_scene);

    ged_draw_shape_draft *draft;
    BU_GET(draft, ged_draw_shape_draft);
    draft->gedp = gedp;
    draft->view = v;
    draft->source_ref = source_scene;
    draft->shape_ref = shape_scene;
    draft->committed = 0;
    return draft;
}


void
ged_draw_shape_draft_destroy(ged_draw_shape_draft *draft)
{
    if (!draft)
	return;
    if (!draft->committed)
	_ged_draw_shape_draft_destroy_refs(draft->source_ref, draft->shape_ref);
    draft->source_ref = bsg_scene_ref_null();
    draft->shape_ref = bsg_scene_ref_null();
    BU_PUT(draft, ged_draw_shape_draft);
}


int
ged_draw_shape_draft_set_fullpath(ged_draw_shape_draft *draft,
				  const struct db_full_path *path)
{
    if (!draft || bsg_scene_ref_is_null(draft->shape_ref))
	return 0;
    return ged_draw_scene_ref_set_fullpath(draft->gedp, draft->shape_ref, path);
}


int
ged_draw_shape_draft_set_region(ged_draw_shape_draft *draft,
				int region_id,
				int aircode,
				int los,
				int material_id)
{
    if (!draft || bsg_scene_ref_is_null(draft->shape_ref))
	return 0;
    return ged_draw_scene_ref_set_region(draft->gedp, draft->shape_ref,
	    region_id, aircode, los, material_id);
}


int
ged_draw_shape_draft_publish_line_set(ged_draw_shape_draft *draft,
				      const point_t *points,
				      const int *commands,
				      size_t point_count)
{
    if (!draft || bsg_scene_ref_is_null(draft->shape_ref))
	return 0;
    return ged_draw_scene_ref_publish_line_set(draft->shape_ref, points,
	    commands, point_count);
}


int
ged_draw_shape_draft_publish_bot_wireframe_line_set(ged_draw_shape_draft *draft,
						    const struct rt_bot_internal *bot)
{
    if (!draft || bsg_scene_ref_is_null(draft->shape_ref) || !bot)
	return 0;
    return ged_draw_scene_ref_publish_bot_wireframe_line_set(draft->shape_ref,
	    bot);
}


int
ged_draw_shape_draft_publish_primitive_face_set(ged_draw_shape_draft *draft,
						struct rt_db_internal *ip,
						const struct bg_tess_tol *ttol,
						const struct bn_tol *tol,
						const struct bsg_view *v)
{
    if (!draft || bsg_scene_ref_is_null(draft->shape_ref) || !ip)
	return 0;
    return ged_draw_scene_ref_publish_primitive_face_set(draft->shape_ref, ip,
	    ttol, tol, v);
}


int
ged_draw_shape_draft_publish_brep_wireframe_line_set(ged_draw_shape_draft *draft,
						     const struct rt_brep_internal *bi,
						     const struct bn_tol *tol)
{
    if (!draft || bsg_scene_ref_is_null(draft->shape_ref) || !bi)
	return 0;
    return ged_draw_scene_ref_publish_brep_wireframe_line_set(draft->shape_ref,
	    bi, tol);
}


int
ged_draw_shape_draft_publish_poly_wireframe_line_set(ged_draw_shape_draft *draft,
						     const struct rt_pg_internal *pg)
{
    if (!draft || bsg_scene_ref_is_null(draft->shape_ref) || !pg)
	return 0;
    return ged_draw_scene_ref_publish_poly_wireframe_line_set(draft->shape_ref,
	    pg);
}


int
ged_draw_shape_draft_publish_nmg_region(ged_draw_shape_draft *draft,
					const struct nmgregion *r,
					int style)
{
    if (!draft || bsg_scene_ref_is_null(draft->shape_ref) || !r)
	return 0;
    return ged_draw_scene_ref_geometry_publish_nmg_region(draft->shape_ref, r, style);
}


int
ged_draw_shape_draft_clear_geometry(ged_draw_shape_draft *draft)
{
    if (!draft || bsg_scene_ref_is_null(draft->shape_ref))
	return 0;
    return ged_draw_scene_ref_geometry_clear(draft->shape_ref);
}


int
ged_draw_shape_draft_update_scene_bounds(ged_draw_shape_draft *draft)
{
    if (!draft || bsg_scene_ref_is_null(draft->shape_ref))
	return 0;
    return bsg_scene_update_bounds(draft->shape_ref, draft->view);
}


int
ged_draw_shape_draft_update_bounds_from_geometry(ged_draw_shape_draft *draft,
						 int *bad_cmd)
{
    if (!draft || bsg_scene_ref_is_null(draft->shape_ref))
	return 0;
    return ged_draw_scene_ref_update_bounds_from_geometry(draft->shape_ref, bad_cmd);
}


int
ged_draw_shape_draft_set_bounds_from_minmax(ged_draw_shape_draft *draft,
					    const point_t min,
					    const point_t max,
					    int set_node_bounds)
{
    if (!draft || bsg_scene_ref_is_null(draft->shape_ref))
	return 0;
    return ged_draw_scene_ref_set_bounds_from_minmax(draft->shape_ref, min, max,
	    set_node_bounds);
}


int
ged_draw_shape_draft_set_center_size(ged_draw_shape_draft *draft,
				     const point_t center,
				     fastf_t size)
{
    if (!draft || bsg_scene_ref_is_null(draft->shape_ref) || !center)
	return 0;
    bsg_scene_set_draw_center(draft->shape_ref, center);
    bsg_scene_set_draw_size(draft->shape_ref, size);
    return 1;
}


int
ged_draw_shape_draft_publish_primitive_wireframe(ged_draw_shape_draft *draft,
						 struct rt_db_internal *ip,
						 const struct bg_tess_tol *ttol,
						 const struct bn_tol *tol,
						 struct bsg_view *v,
						 int adaptive)
{
    if (!draft || bsg_scene_ref_is_null(draft->shape_ref))
	return -1;
    return ged_draw_scene_ref_publish_primitive_wireframe(draft->shape_ref, ip,
	    ttol, tol, v, adaptive);
}


int
ged_draw_shape_draft_set_highlighted(ged_draw_shape_draft *draft,
				     int highlighted)
{
    if (!draft || bsg_scene_ref_is_null(draft->shape_ref))
	return 0;
    bsg_scene_set_highlighted(draft->shape_ref, highlighted);
    return 1;
}


int
ged_draw_shape_draft_set_line_style(ged_draw_shape_draft *draft, int dashed)
{
    if (!draft || bsg_scene_ref_is_null(draft->shape_ref))
	return 0;
    bsg_scene_set_line_style(draft->shape_ref, dashed);
    return 1;
}


int
ged_draw_shape_draft_set_line_width(ged_draw_shape_draft *draft, int line_width)
{
    if (!draft || bsg_scene_ref_is_null(draft->shape_ref))
	return 0;
    bsg_scene_set_line_width(draft->shape_ref, line_width);
    return 1;
}


int
ged_draw_shape_draft_set_legacy_color_info(ged_draw_shape_draft *draft,
					   const unsigned char basecolor[3],
					   int user_color,
					   int default_color)
{
    if (!draft || bsg_scene_ref_is_null(draft->shape_ref) || !basecolor)
	return 0;
    bsg_scene_set_legacy_color_info(draft->shape_ref, basecolor,
	    user_color, default_color);
    return 1;
}


int
ged_draw_shape_draft_set_legacy_uses_default_color(ged_draw_shape_draft *draft,
						   int default_color)
{
    if (!draft || bsg_scene_ref_is_null(draft->shape_ref))
	return 0;
    bsg_scene_set_legacy_uses_default_color(draft->shape_ref, default_color);
    return 1;
}


int
ged_draw_shape_draft_set_legacy_eval_flag(ged_draw_shape_draft *draft,
					  int eflag)
{
    if (!draft || bsg_scene_ref_is_null(draft->shape_ref))
	return 0;
    bsg_scene_set_legacy_eval_flag(draft->shape_ref, eflag);
    return 1;
}


int
ged_draw_shape_draft_set_legacy_region_id(ged_draw_shape_draft *draft,
					  int region_id)
{
    if (!draft || bsg_scene_ref_is_null(draft->shape_ref))
	return 0;
    bsg_scene_set_legacy_region_id(draft->shape_ref, region_id);
    return 1;
}


int
ged_draw_shape_draft_color_soltab(ged_draw_shape_draft *draft, struct db_i *dbip)
{
    if (!draft || bsg_scene_ref_is_null(draft->shape_ref))
	return 0;
    color_soltab_scene_ref(dbip, draft->shape_ref);
    return 1;
}


int
ged_draw_shape_draft_set_name(ged_draw_shape_draft *draft, const char *name)
{
    if (!draft || bsg_scene_ref_is_null(draft->shape_ref) || !name)
	return 0;
    bsg_scene_set_name(draft->shape_ref, name);
    if (!bsg_scene_ref_is_null(draft->source_ref)) {
	struct bu_vls source_name = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&source_name, "%s:source", name);
	bsg_scene_set_name(draft->source_ref, bu_vls_cstr(&source_name));
	bu_vls_free(&source_name);
    }
    return 1;
}


int
ged_draw_shape_draft_set_source_state(ged_draw_shape_draft *draft,
				      struct ged_draw_source_state *data)
{
    if (!draft || bsg_scene_ref_is_null(draft->shape_ref))
	return 0;
    ged_draw_scene_ref_source_data_set(draft->shape_ref, data);
    return 1;
}


int
ged_draw_shape_draft_mark_db_object(ged_draw_shape_draft *draft)
{
    if (!draft || bsg_scene_ref_is_null(draft->shape_ref))
	return 0;
    bsg_scene_mark_db_object(draft->shape_ref);
    return 1;
}


int
ged_draw_shape_draft_apply_settings(ged_draw_shape_draft *draft,
				    const struct bsg_appearance_settings *settings)
{
    if (!draft || bsg_scene_ref_is_null(draft->shape_ref) || !settings)
	return 0;
    return bsg_scene_apply_appearance_settings(draft->shape_ref, settings);
}


int
ged_draw_shape_draft_bump_appearance_revision(ged_draw_shape_draft *draft)
{
    if (!draft || bsg_scene_ref_is_null(draft->shape_ref))
	return 0;
    bsg_scene_set_changed(draft->shape_ref,
	    bsg_scene_changed(draft->shape_ref) + 1);
    return 1;
}


int
ged_draw_shape_draft_set_material_rgb(ged_draw_shape_draft *draft,
				      unsigned char r,
				      unsigned char g,
				      unsigned char b)
{
    if (!draft || bsg_scene_ref_is_null(draft->shape_ref))
	return 0;
    bsg_scene_set_color(draft->shape_ref, r, g, b);
    bsg_scene_material_set_rgb(draft->shape_ref, r, g, b);
    return 1;
}


int
ged_draw_shape_draft_set_transform(ged_draw_shape_draft *draft, const mat_t mat)
{
    if (!draft || bsg_scene_ref_is_null(draft->shape_ref) || !mat)
	return 0;
    bsg_scene_set_transform(draft->shape_ref, mat);
    return 1;
}


int
ged_draw_shape_draft_set_bounds(ged_draw_shape_draft *draft,
				const point_t min,
				const point_t max)
{
    if (!draft || bsg_scene_ref_is_null(draft->shape_ref) || !min || !max)
	return 0;
    bsg_scene_set_bounds(draft->shape_ref, min, max, 1);
    return 1;
}


int
ged_draw_shape_draft_set_draw_size(ged_draw_shape_draft *draft, fastf_t size)
{
    if (!draft || bsg_scene_ref_is_null(draft->shape_ref))
	return 0;
    bsg_scene_set_draw_size(draft->shape_ref, size);
    return 1;
}


int
ged_draw_shape_draft_set_visible(ged_draw_shape_draft *draft, int visible)
{
    if (!draft || bsg_scene_ref_is_null(draft->shape_ref))
	return 0;
    bsg_scene_set_visible(draft->shape_ref, visible ? 1 : 0);
    return 1;
}


int
ged_draw_shape_draft_set_transparency(ged_draw_shape_draft *draft,
				      fastf_t transparency)
{
    if (!draft || bsg_scene_ref_is_null(draft->shape_ref))
	return 0;
    bsg_scene_set_transparency(draft->shape_ref, transparency);
    return 1;
}


int
ged_draw_shape_draft_set_draw_mode(ged_draw_shape_draft *draft, int draw_mode)
{
    if (!draft || bsg_scene_ref_is_null(draft->shape_ref))
	return 0;
    bsg_scene_set_dmode(draft->shape_ref, draw_mode);
    struct bsg_draw_intent *di = bsg_scene_draw_intent(draft->shape_ref);
    if (di)
	bsg_draw_intent_set_mode(di, (bsg_draw_mode)draw_mode);
    return 1;
}


int
ged_draw_shape_draft_set_draw_mat(ged_draw_shape_draft *draft, const mat_t mat)
{
    if (!draft || bsg_scene_ref_is_null(draft->shape_ref) || !mat)
	return 0;
    bsg_scene_set_draw_mat(draft->shape_ref, mat);
    return 1;
}


ged_draw_shape_ref
ged_draw_shape_draft_commit_to_group(ged_draw_shape_draft *draft,
				     ged_draw_group_ref group_ref)
{
    if (!draft || bsg_scene_ref_is_null(draft->shape_ref))
	return GED_DRAW_SHAPE_REF_NULL;

    _ged_draw_shape_draft_sync_aux_geometry(draft);

    if (!ged_draw_group_ref_append_scene_ref(draft->gedp, group_ref, draft->shape_ref)) {
	ged_draw_shape_draft_destroy(draft);
	return GED_DRAW_SHAPE_REF_NULL;
    }
    ged_draw_shape_ref ref =
	ged_draw_shape_ref_from_scene_ref(draft->gedp, draft->shape_ref);
    if (ged_draw_shape_ref_is_null(ref)) {
	ged_draw_shape_draft_destroy(draft);
	return GED_DRAW_SHAPE_REF_NULL;
    }
    ref.scene_revision = 0;

    draft->committed = 1;
    ged_draw_shape_draft_destroy(draft);
    return ref;
}


bsg_scene_ref
ged_draw_shape_draft_commit_to_scene_ref(ged_draw_shape_draft *draft,
					 bsg_scene_ref parent_ref)
{
    if (!draft || bsg_scene_ref_is_null(draft->shape_ref) ||
	    bsg_scene_ref_is_null(parent_ref))
	return bsg_scene_ref_null();

    _ged_draw_shape_draft_sync_aux_geometry(draft);

    bsg_scene_ref shape_ref = draft->shape_ref;
    bsg_scene_append_child(parent_ref, draft->source_ref);
    draft->committed = 1;
    ged_draw_shape_draft_destroy(draft);
    return shape_ref;
}


ged_draw_shape_ref
ged_draw_shape_draft_commit_to_last_group(ged_draw_shape_draft *draft)
{
    if (!draft || bsg_scene_ref_is_null(draft->shape_ref))
	return GED_DRAW_SHAPE_REF_NULL;

    _ged_draw_shape_draft_sync_aux_geometry(draft);

    ged_draw_append_scene_ref_to_last_group(draft->gedp, draft->shape_ref);
    ged_draw_shape_ref ref =
	ged_draw_shape_ref_from_scene_ref(draft->gedp, draft->shape_ref);
    if (ged_draw_shape_ref_is_null(ref)) {
	ged_draw_shape_draft_destroy(draft);
	return GED_DRAW_SHAPE_REF_NULL;
    }
    ref.scene_revision = 0;

    draft->committed = 1;
    ged_draw_shape_draft_destroy(draft);
    return ref;
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
