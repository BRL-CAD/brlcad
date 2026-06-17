/*                B S G _ G E D _ D R A W _ S T A T E . C
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
/** @file libged/b_s_g___g_e_d___d_r_a_w___s_t_a_t_e_._c.c
 *
 * GED draw-shape path and region state helpers.
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

static unsigned long long
_ged_draw_path_hash(const struct db_full_path *path)
{
    if (!path || path->fp_len <= 0)
	return 0;
    unsigned long long *components = (unsigned long long *)bu_calloc(
	path->fp_len, sizeof(unsigned long long), "ged draw path hash components");
    for (size_t i = 0; i < path->fp_len; i++) {
	const struct directory *dp = path->fp_names[i];
	const char *name = (dp && dp->d_namep) ? dp->d_namep : "";
	components[i] = bu_data_hash(name, strlen(name) * sizeof(char));
    }
    unsigned long long hash = bu_data_hash(components,
	path->fp_len * sizeof(unsigned long long));
    bu_free(components, "ged draw path hash components");
    return hash;
}


void
ged_draw_shape_state_set_fullpath(ged_draw_shape_state *data,
				 const struct db_full_path *path)
{
    if (!data)
	return;
    db_free_full_path(&data->s_fullpath);
    db_full_path_init(&data->s_fullpath);
    data->leaf_dp = RT_DIR_NULL;
    data->path_hash = 0;
    if (data->display_name) {
	bu_free(data->display_name, "ged draw shape display name");
	data->display_name = NULL;
    }
    if (!path || path->fp_len <= 0)
	return;
    db_dup_full_path(&data->s_fullpath, path);
    data->leaf_dp = DB_FULL_PATH_CUR_DIR(&data->s_fullpath);
    data->path_hash = _ged_draw_path_hash(&data->s_fullpath);
    char *path_name = db_path_to_string(&data->s_fullpath);
    if (path_name) {
	data->display_name = bu_strdup(path_name);
	bu_free(path_name, "ged draw shape path string");
    } else if (data->leaf_dp && data->leaf_dp->d_namep) {
	data->display_name = bu_strdup(data->leaf_dp->d_namep);
    }
}


ged_draw_shape_state *
ged_draw_shape_ref_set_fullpath(bsg_scene_ref ref,
				struct ged *gedp,
				const struct db_full_path *path)
{
    if (!ged_draw_scene_ref_set_fullpath(gedp, ref, path))
	return NULL;
    return ged_draw_shape_state_get_scene_ref(ref);
}


int
ged_draw_scene_ref_set_fullpath(struct ged *gedp,
				bsg_scene_ref ref,
				const struct db_full_path *path)
{
    if (bsg_scene_ref_is_null(ref))
	return 0;

    ged_draw_shape_state *data =
	ged_draw_shape_state_ensure_scene_ref(gedp, ref);
    if (!data)
	return 0;
    ged_draw_scene_ref_index_remove(gedp, ref);
    ged_draw_shape_state_set_fullpath(data, path);
    ged_draw_scene_ref_index_add(gedp, ref);

    if (path && path->fp_len > 0) {
	char *path_name = db_path_to_string(path);
	if (path_name) {
	    const char *semantic_path = path_name;
	    while (*semantic_path == '/')
		semantic_path++;
	    struct bsg_draw_intent *di = bsg_scene_draw_intent(ref);
	    if (di)
		bsg_draw_intent_set_path(di, semantic_path);
	    else
		bsg_scene_set_draw_intent(ref,
			bsg_draw_intent_create(semantic_path, BSG_DRAW_MODE_WIRE));
	    bu_free(path_name, "ged draw shape intent path string");
	}
    }

    return 1;
}


int
ged_draw_scene_ref_prepare(struct ged *gedp, bsg_scene_ref ref)
{
    if (bsg_scene_ref_is_null(ref))
	return 0;
    if (!ged_draw_shape_state_ensure_scene_ref(gedp, ref))
	return 0;
    return ged_draw_scene_ref_source_ensure(ref);
}


void
ged_draw_shape_state_set_region(ged_draw_shape_state *data,
			       int region_id,
			       int aircode,
			       int los,
			       int material_id)
{
    if (!data)
	return;
    data->region_id = region_id;
    data->aircode = aircode;
    data->los = los;
    data->material_id = material_id;
}


int
ged_draw_scene_ref_set_region(struct ged *gedp,
			      bsg_scene_ref ref,
			      int region_id,
			      int aircode,
			      int los,
			      int material_id)
{
    ged_draw_shape_state *data =
	ged_draw_shape_state_ensure_scene_ref(gedp, ref);
    if (!data)
	return 0;
    ged_draw_shape_state_set_region(data, region_id, aircode, los, material_id);
    return 1;
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
