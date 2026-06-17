/*              B S G _ G E D _ D R A W _ O V E R L A Y . C
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
/** @file bsg_ged_draw_overlay.c
 *
 * Typed GED overlay insertion helpers.
 */

#include "common.h"

#include "bu/log.h"
#include "bsg/appearance.h"
#include "bsg/geometry.h"
#include "bsg/overlay.h"
#include "bsg/scene_builder.h"
#include "bsg/scene_object.h"

#include "ged.h"
#include "ged/bsg_ged_draw.h"
#include "./ged_private.h"
#include "./bsg_ged_draw_private.h"


void
ged_draw_overlay_erase_name(struct ged *gedp, const char *name)
{
    struct bsg_view *v = gedp ? gedp->ged_gvp : NULL;
    bsg_scene_ref ref = bsg_overlay_find_scene(v, name);
    if (!bsg_scene_ref_is_null(ref))
	ged_draw_scene_ref_highlight_free_cb(ref);
    bsg_overlay_erase_name(v, name);
}


static int
_ged_overlay_publish_geometry(bsg_scene_ref ref,
			      const struct ged_draw_overlay_geometry *geometry)
{
    int ok = 0;
    size_t command_count = 0;

    if (bsg_scene_ref_is_null(ref) || !geometry)
	return 0;

    switch (geometry->kind) {
	case BSG_GEOMETRY_NODE_LINE_SET:
	    if (geometry->point_count != geometry->command_count)
		return 0;
	    ok = bsg_geometry_ref_set_line_set(bsg_scene_ref_as_geometry(ref),
		    geometry->points, geometry->commands, geometry->point_count);
	    command_count = geometry->point_count;
	    break;
	case BSG_GEOMETRY_NODE_POINT_SET:
	    ok = bsg_geometry_ref_set_point_set(bsg_scene_ref_as_geometry(ref),
		    geometry->points, geometry->point_count);
	    command_count = geometry->point_count;
	    break;
	case BSG_GEOMETRY_NODE_INDEXED_FACE_SET:
	    ok = bsg_geometry_ref_set_indexed_face_set(bsg_scene_ref_as_geometry(ref),
		    geometry->points, geometry->point_count,
		    geometry->normals, geometry->normal_count,
		    geometry->indices, geometry->index_count);
	    command_count = geometry->point_count;
	    break;
	default:
	    return 0;
    }

    if (!ok)
	return 0;

    ged_draw_shape_state *shape_data = ged_draw_shape_state_get_scene_ref(ref);
    if (shape_data) {
	shape_data->geometry_command_count = command_count;
	shape_data->geometry_revision++;
    }
    bsg_scene_invalidate(ref);
    return 1;
}


static bsg_scene_ref
_ged_overlay_create_scene(struct bsg_view *v, const char *name,
			  bsg_geometry_node_kind kind)
{
    switch (kind) {
	case BSG_GEOMETRY_NODE_LINE_SET:
	    return bsg_geometry_ref_as_scene(
		    bsg_line_set_ref_as_geometry(bsg_line_set_ref_create(v, name)));
	case BSG_GEOMETRY_NODE_POINT_SET:
	    return bsg_geometry_ref_as_scene(
		    bsg_point_set_ref_as_geometry(bsg_point_set_ref_create(v, name)));
	case BSG_GEOMETRY_NODE_INDEXED_FACE_SET:
	    return bsg_geometry_ref_as_scene(
		    bsg_indexed_face_set_ref_as_geometry(
			bsg_indexed_face_set_ref_create(v, name)));
	default:
	    return (bsg_scene_ref)BSG_SCENE_REF_NULL_INIT;
    }
}


int
ged_draw_overlay_geometry_insert(struct ged *gedp, const char *name,
	const struct ged_draw_overlay_geometry *geometry,
	long int rgb, fastf_t transparency, int draw_mode, int csoltab,
	ged_draw_shape_ref *out)
{
    if (out)
	*out = GED_DRAW_SHAPE_REF_NULL;
    if (!gedp || !name || !geometry)
	return -1;
    if (!gedp->ged_gvp)
	return 0;

    struct db_i *dbip = gedp->dbip;
    if (dbip == DBI_NULL)
	return 0;

    if (db_lookup(dbip, name, LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_log("ged_draw_overlay_geometry_insert(%s) would clobber existing database entry, "
		"ignored\n", name);
	return -1;
    }

    ged_draw_overlay_erase_name(gedp, name);

    bsg_scene_ref overlay_scene = _ged_overlay_create_scene(gedp->ged_gvp,
	    name, geometry->kind);
    if (bsg_scene_ref_is_null(overlay_scene))
	return -1;
    (void)bsg_overlay_register_scene_owner(overlay_scene, gedp, BSG_OVERLAY_ROLE_MODEL,
	    BSG_OVERLAY_CLASS_COMMAND_RESULT, BSG_OVERLAY_LC_PERSISTENT,
	    BSG_OVERLAY_ORDER_MODEL, name, 0);
    bsg_scene_set_name(overlay_scene, name);

    if (!ged_draw_scene_ref_prepare(gedp, overlay_scene)) {
	bsg_scene_ref_destroy(overlay_scene);
	return -1;
    }

    if (!_ged_overlay_publish_geometry(overlay_scene, geometry)) {
	bsg_scene_ref_destroy(overlay_scene);
	return -1;
    }
    bsg_scene_update_bounds(overlay_scene, gedp->ged_gvp);

    if (!bsg_overlay_append_scene(gedp->ged_gvp, overlay_scene)) {
	bsg_scene_ref_destroy(overlay_scene);
	return -1;
    }

    bsg_scene_set_highlighted(overlay_scene, 0);
    bsg_scene_set_line_style(overlay_scene, 0);
    bsg_scene_set_legacy_eval_flag(overlay_scene, 1);
    unsigned char basecolor[3] = {
	(unsigned char)((rgb >> 16) & 0xFF),
	(unsigned char)((rgb >>  8) & 0xFF),
	(unsigned char)(rgb & 0xFF)
    };
    bsg_scene_set_legacy_color_info(overlay_scene, basecolor, 0, 0);
    bsg_scene_set_color(overlay_scene, basecolor[0], basecolor[1], basecolor[2]);
    bsg_scene_set_legacy_region_id(overlay_scene, 0);
    bsg_scene_set_work_flag(overlay_scene, 0);
    bsg_scene_set_transparency(overlay_scene, transparency);
    bsg_scene_set_dmode(overlay_scene, draw_mode);

    if (csoltab)
	color_soltab_scene_ref(gedp->dbip, overlay_scene);

    if (out)
	*out = ged_draw_shape_ref_from_scene_ref(gedp, overlay_scene);

    return 0;
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
