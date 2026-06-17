/*                B S G _ G E D _ D R A W _ T R A N S A C T I O N S . C
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
/** @file libged/b_s_g___g_e_d___d_r_a_w___t_r_a_n_s_a_c_t_i_o_n_s_._c.c
 *
 * Draw transactions, database invalidation, and redraw compatibility bridge.
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
#include "ged/selection_state.h"
#include "./ged_private.h"
#include "./draw/ged_draw.h"

struct ged_draw_db_update_ctx {
    struct ged *gedp;
    const char *path;
    ged_draw_stale_reason reason;
    int changed;
};


struct ged_draw_transparency_ctx {
    struct ged *gedp;
    struct directory **dpp;
    fastf_t transparency;
    int changed;
};


struct ged_draw_visibility_ctx {
    struct ged *gedp;
    const char *path;
    const char *basename;
    int visible;
    int changed;
};


struct ged_draw_highlight_ctx {
    struct ged *gedp;
    const char *basename;
    int highlighted;
    int matched;
};


struct ged_draw_redraw_ctx {
    struct db_i *dbip;
    struct db_tree_state *tsp;
    struct bsg_view *gvp;
    int skip_subtractions;
    int ret;
};


struct ged_draw_redraw_path_ctx {
    struct ged *gedp;
    struct db_full_path *obj_path;
    struct bsg_view *view;
    int found;
    int ret;
};


struct ged_draw_redraw_shape_entry {
    ged_draw_shape_ref ref;
    struct bsg_view *view;
};


struct ged_draw_redraw_source_ctx {
    struct ged *gedp;
    const char *path;
    struct bsg_view *view;
    struct bu_ptbl shape_refs;
};


struct ged_draw_reexpand_group_entry {
    ged_draw_group_ref ref;
    char *path;
    struct bsg_view *view;
    struct bsg_appearance_settings appearance;
};


struct ged_draw_reexpand_source_ctx {
    struct ged *gedp;
    const char *path;
    struct bsg_view *view;
    struct bu_ptbl groups;
};


struct ged_draw_lod_finalize_ctx {
    struct ged *gedp;
    struct bsg_view *first_view;
    struct bsg_view **views;
    size_t view_count;
    int ensured;
};


struct ged_draw_rename_ctx {
    struct ged *gedp;
    const char *old_path;
    const char *new_path;
    int changed;
};


struct ged_draw_observer_entry {
    ged_draw_observer_token token;
    ged_draw_transaction_observer_func_t func;
    void *client_data;
    int active;
};


static struct ged_drawable *
_ged_draw_gdp(struct ged *gedp)
{
    return (gedp && gedp->i) ? gedp->i->ged_gdp : NULL;
}


static void
_ged_draw_observers_init_if_needed(struct ged_drawable *gdp)
{
    if (!gdp || gdp->gd_draw_observers_init)
	return;
    BU_PTBL_INIT(&gdp->gd_draw_observers);
    gdp->gd_draw_observers_init = 1;
    gdp->gd_draw_next_observer_token = 1;
    gdp->gd_draw_observer_dispatch_depth = 0;
}


static void
_ged_draw_observer_entry_free(struct ged_draw_observer_entry *entry)
{
    if (!entry)
	return;
    bu_free(entry, "ged draw observer entry");
}


static struct ged_draw_observer_entry *
_ged_draw_observer_find(struct ged_drawable *gdp,
			ged_draw_observer_token token)
{
    if (!gdp || !gdp->gd_draw_observers_init || !token)
	return NULL;
    for (size_t i = 0; i < BU_PTBL_LEN(&gdp->gd_draw_observers); i++) {
	struct ged_draw_observer_entry *entry =
	    (struct ged_draw_observer_entry *)BU_PTBL_GET(
		    &gdp->gd_draw_observers, i);
	if (entry && entry->token == token)
	    return entry;
    }
    return NULL;
}


static int
_ged_draw_observers_have_active(struct ged *gedp)
{
    struct ged_drawable *gdp = _ged_draw_gdp(gedp);
    if (!gdp || !gdp->gd_draw_observers_init)
	return 0;
    for (size_t i = 0; i < BU_PTBL_LEN(&gdp->gd_draw_observers); i++) {
	struct ged_draw_observer_entry *entry =
	    (struct ged_draw_observer_entry *)BU_PTBL_GET(
		    &gdp->gd_draw_observers, i);
	if (entry && entry->active && entry->func)
	    return 1;
    }
    return 0;
}


static void
_ged_draw_observers_prune(struct ged_drawable *gdp)
{
    if (!gdp || !gdp->gd_draw_observers_init ||
	    gdp->gd_draw_observer_dispatch_depth > 0)
	return;

    size_t i = 0;
    while (i < BU_PTBL_LEN(&gdp->gd_draw_observers)) {
	struct ged_draw_observer_entry *entry =
	    (struct ged_draw_observer_entry *)BU_PTBL_GET(
		    &gdp->gd_draw_observers, i);
	if (entry && (!entry->active || !entry->func)) {
	    bu_ptbl_rm(&gdp->gd_draw_observers, (long *)entry);
	    _ged_draw_observer_entry_free(entry);
	    continue;
	}
	i++;
    }
}


static void
_ged_draw_observers_dispatch(struct ged *gedp,
			     const struct ged_draw_transaction *txn,
			     const struct ged_draw_transaction_result *result)
{
    struct ged_drawable *gdp = _ged_draw_gdp(gedp);
    if (!gdp || !gdp->gd_draw_observers_init || !txn || !result ||
	    result->status <= 0)
	return;

    size_t len = BU_PTBL_LEN(&gdp->gd_draw_observers);
    gdp->gd_draw_observer_dispatch_depth++;
    for (size_t i = 0; i < len; i++) {
	struct ged_draw_observer_entry *entry =
	    (struct ged_draw_observer_entry *)BU_PTBL_GET(
		    &gdp->gd_draw_observers, i);
	if (!entry || !entry->active || !entry->func)
	    continue;
	entry->func(gedp, txn, result, entry->client_data);
    }
    gdp->gd_draw_observer_dispatch_depth--;
    _ged_draw_observers_prune(gdp);
}


ged_draw_observer_token
ged_draw_observer_add(struct ged *gedp,
		      ged_draw_transaction_observer_func_t func,
		      void *client_data)
{
    struct ged_drawable *gdp = _ged_draw_gdp(gedp);
    if (!gdp || !func)
	return 0;

    _ged_draw_observers_init_if_needed(gdp);

    struct ged_draw_observer_entry *entry =
	(struct ged_draw_observer_entry *)bu_calloc(1, sizeof(*entry),
		"ged draw observer entry");
    entry->token = gdp->gd_draw_next_observer_token++;
    if (!entry->token)
	entry->token = gdp->gd_draw_next_observer_token++;
    entry->func = func;
    entry->client_data = client_data;
    entry->active = 1;
    bu_ptbl_ins(&gdp->gd_draw_observers, (long *)entry);
    return entry->token;
}


int
ged_draw_observer_remove(struct ged *gedp, ged_draw_observer_token token)
{
    struct ged_drawable *gdp = _ged_draw_gdp(gedp);
    struct ged_draw_observer_entry *entry =
	_ged_draw_observer_find(gdp, token);
    if (!entry || !entry->active)
	return 0;

    entry->active = 0;
    entry->func = NULL;
    if (gdp->gd_draw_observer_dispatch_depth > 0)
	return 1;

    bu_ptbl_rm(&gdp->gd_draw_observers, (long *)entry);
    _ged_draw_observer_entry_free(entry);
    return 1;
}


void
ged_draw_observers_free(struct ged *gedp)
{
    struct ged_drawable *gdp = _ged_draw_gdp(gedp);
    if (!gdp || !gdp->gd_draw_observers_init)
	return;

    for (size_t i = 0; i < BU_PTBL_LEN(&gdp->gd_draw_observers); i++) {
	struct ged_draw_observer_entry *entry =
	    (struct ged_draw_observer_entry *)BU_PTBL_GET(
		    &gdp->gd_draw_observers, i);
	_ged_draw_observer_entry_free(entry);
    }
    bu_ptbl_free(&gdp->gd_draw_observers);
    gdp->gd_draw_observers_init = 0;
    gdp->gd_draw_next_observer_token = 1;
    gdp->gd_draw_observer_dispatch_depth = 0;
}


static struct bsg_lod_source_policy_settings
ged_draw_lod_policy(const struct bsg_view *v)
{
    struct bsg_lod_source_policy_settings policy;
    memset(&policy, 0, sizeof(policy));
    policy.policy = BSG_LOD_AUTO;
    policy.scale = 1.0;
    policy.curve_scale = 1.0;
    policy.point_scale = 1.0;
    if (v)
	(void)bsg_view_lod_source_policy_get(v, &policy);
    return policy;
}


static const char *
_ged_draw_component_name(const char *name)
{
    if (!name)
	return NULL;

    name = ged_draw_dbpath_skip_lead_slash(name);
    const char *basename = strrchr(name, '/');
    return basename ? basename + 1 : name;
}


static int
_dbfullpath_has_component(const struct db_full_path *fp, const char *name)
{
    if (!fp || !name)
	return 0;
    const char *basename = _ged_draw_component_name(name);
    if (!basename || !*basename)
	return 0;
    for (size_t i = 0; i < fp->fp_len; i++) {
	struct directory *dp = fp->fp_names[i];
	if (dp && BU_STR_EQUAL(dp->d_namep, basename))
	    return 1;
    }
    return 0;
}


static int
_ged_draw_path_has_component(const char *path, const char *name)
{
    if (!path || !name)
	return 0;

    path = ged_draw_dbpath_skip_lead_slash(path);
    const char *basename = _ged_draw_component_name(name);
    if (!*path || !basename || !*basename)
	return 0;

    size_t namelen = strlen(basename);
    const char *p = path;
    while (*p) {
	while (*p == '/')
	    p++;
	if (!*p)
	    break;
	const char *slash = strchr(p, '/');
	size_t len = slash ? (size_t)(slash - p) : strlen(p);
	if (len == namelen && bu_strncmp(p, basename, len) == 0)
	    return 1;
	if (!slash)
	    break;
	p = slash + 1;
    }

    return 0;
}


static int
_ged_draw_shape_record_has_component(const struct ged_draw_shape_record *rec,
				     const char *name)
{
    if (!rec || !name)
	return 0;

    if (rec->display_name &&
	    _ged_draw_path_has_component(rec->display_name, name))
	return 1;
    if (rec->fullpath && _dbfullpath_has_component(rec->fullpath, name))
	return 1;

    const char *basename = _ged_draw_component_name(name);
    if (rec->leaf_name && basename && BU_STR_EQUAL(rec->leaf_name, basename))
	return 1;

    return 0;
}


static int
_ged_draw_path_replace_component(struct bu_vls *out,
				 const char *path,
				 const char *old_name,
				 const char *new_name)
{
    if (!out || !path || !old_name || !new_name)
	return 0;

    path = ged_draw_dbpath_skip_lead_slash(path);
    old_name = ged_draw_dbpath_skip_lead_slash(old_name);
    new_name = ged_draw_dbpath_skip_lead_slash(new_name);
    size_t old_len = strlen(old_name);
    if (!*path || !old_len || !*new_name)
	return 0;

    int changed = 0;
    int first = 1;
    bu_vls_trunc(out, 0);
    const char *p = path;
    while (*p) {
	const char *slash = strchr(p, '/');
	size_t len = slash ? (size_t)(slash - p) : strlen(p);
	if (!first)
	    bu_vls_putc(out, '/');
	if (len == old_len && bu_strncmp(p, old_name, old_len) == 0) {
	    bu_vls_strcat(out, new_name);
	    changed = 1;
	} else {
	    bu_vls_strncat(out, p, len);
	}
	first = 0;
	if (!slash)
	    break;
	p = slash + 1;
    }
    return changed;
}


static int
_ged_draw_rename_group_cb(const struct ged_draw_group_record *rec, void *userdata)
{
    struct ged_draw_rename_ctx *ctx =
	(struct ged_draw_rename_ctx *)userdata;
    if (!ctx || !ctx->gedp || !ctx->old_path || !ctx->new_path ||
	    !rec || !rec->path)
	return 1;

    const char *path = ged_draw_dbpath_skip_lead_slash(rec->path);
    const char *old_path = ged_draw_dbpath_skip_lead_slash(ctx->old_path);
    const char *new_path = ged_draw_dbpath_skip_lead_slash(ctx->new_path);
    struct bu_vls updated = BU_VLS_INIT_ZERO;
    if (!_ged_draw_path_replace_component(&updated, path, old_path, new_path)) {
	bu_vls_free(&updated);
	return 1;
    }

    struct db_full_path dfp;
    db_full_path_init(&dfp);
    if (db_string_to_path(&dfp, ctx->gedp->dbip, bu_vls_cstr(&updated)) == 0) {
	if (ged_draw_group_ref_set_dbpath(ctx->gedp, rec->ref, &dfp))
	    ctx->changed++;
    }
    db_free_full_path(&dfp);
    bu_vls_free(&updated);
    return 1;
}


static int
_ged_draw_apply_database_rename(struct ged *gedp,
				const char *old_path,
				const char *new_path)
{
    if (!gedp || !old_path || !new_path)
	return 0;

    struct ged_draw_rename_ctx ctx;
    ctx.gedp = gedp;
    ctx.old_path = old_path;
    ctx.new_path = new_path;
    ctx.changed = 0;
    ged_draw_foreach_group_record(gedp, _ged_draw_rename_group_cb, &ctx);
    return ctx.changed;
}


static int
_ged_draw_mark_db_change_scene_ref(struct ged_draw_db_update_ctx *ctx,
				   bsg_scene_ref shape_ref)
{
    if (!ctx || bsg_scene_ref_is_null(shape_ref))
	return 1;

    ged_draw_shape_state *shape_data = ged_draw_scene_ref_shape_state(shape_ref);
    if (!shape_data)
	return 1;
    shape_data->source_revision++;
    shape_data->stale_reason = ctx->reason ? ctx->reason : GED_DRAW_STALE_SOURCE_CHANGED;
    struct ged_draw_source_state *d =
	ged_draw_scene_ref_source_data(shape_ref);
    if (d) {
	d->source_revision = shape_data->source_revision;
	d->stale_reason = shape_data->stale_reason;
    }
    ged_draw_scene_ref_mark_realization_stale(shape_ref,
	    shape_data->source_revision,
	    shape_data->inputs_revision,
	    shape_data->stale_reason);
    ged_draw_scene_ref_database_source_sync(shape_ref, d, shape_data);
    ctx->changed++;
    return 1;
}


static int
_ged_draw_mark_db_change_cb(const struct ged_draw_shape_record *rec,
			    void *userdata)
{
    struct ged_draw_db_update_ctx *ctx =
	(struct ged_draw_db_update_ctx *)userdata;
    if (!ctx || !rec)
	return 1;
    if (ctx->path && !_ged_draw_shape_record_has_component(rec, ctx->path))
	return 1;
    bsg_scene_ref shape_ref =
	ged_draw_registry_shape_scene_ref(ctx->gedp, rec->ref);
    if (bsg_scene_ref_is_null(shape_ref))
	return 1;
    return _ged_draw_mark_db_change_scene_ref(ctx, shape_ref);
}


static int
_ged_draw_mark_db_change_index_cb(bsg_scene_ref shape_ref, void *userdata)
{
    return _ged_draw_mark_db_change_scene_ref(
	    (struct ged_draw_db_update_ctx *)userdata, shape_ref);
}


int
ged_draw_mark_database_change(struct ged *gedp,
			      const char *path,
			      ged_draw_stale_reason reason)
{
    if (!gedp)
	return 0;
    struct ged_draw_db_update_ctx ctx;
    ctx.gedp = gedp;
    ctx.path = path ? ged_draw_dbpath_skip_lead_slash(path) : NULL;
    ctx.reason = reason ? reason : GED_DRAW_STALE_SOURCE_CHANGED;
    ctx.changed = 0;
    int indexed = ctx.path ?
	ged_draw_shape_index_for_component(gedp, ctx.path,
		_ged_draw_mark_db_change_index_cb, &ctx) : -1;
    if (indexed < 0) {
	struct ged_drawable *gdp = _ged_draw_gdp(gedp);
	if (gdp)
	    gdp->gd_draw_index_fallback_shape_scans++;
	ged_draw_foreach_shape_record(gedp, _ged_draw_mark_db_change_cb,
		&ctx);
    }
    return ctx.changed;
}


static int
_ged_draw_set_transparency_cb(const struct ged_draw_shape_record *rec,
			      void *userdata)
{
    struct ged_draw_transparency_ctx *ctx =
	(struct ged_draw_transparency_ctx *)userdata;
    if (!ctx || !ctx->dpp)
	return 1;

    if (!rec || !rec->fullpath)
	return 1;

    struct directory **tmp_dpp = ctx->dpp;
    size_t i = 0;
    for (i = 0;
	 i < rec->fullpath->fp_len && *tmp_dpp != RT_DIR_NULL;
	 ++i, ++tmp_dpp) {
	if (rec->fullpath->fp_names[i] != *tmp_dpp)
	    break;
    }

    if (*tmp_dpp != RT_DIR_NULL)
	return 1;

    bsg_scene_ref shape_ref =
	ged_draw_registry_shape_scene_ref(ctx->gedp, rec->ref);
    if (!bsg_scene_ref_is_null(shape_ref) &&
	    !ZERO(rec->transparency - ctx->transparency)) {
	bsg_scene_set_transparency(shape_ref, ctx->transparency);
	ctx->changed++;
    }

    return 1;
}


static int
_ged_draw_set_transparency(struct ged *gedp, const char *path,
			   fastf_t transparency)
{
    if (!gedp || !path)
	return 0;

    struct directory **dpp = _ged_build_dpp(gedp, path);
    if (!dpp)
	return 0;

    struct ged_draw_transparency_ctx ctx;
    ctx.gedp = gedp;
    ctx.dpp = dpp;
    ctx.transparency = transparency;
    ctx.changed = 0;

    ged_draw_foreach_shape_record(gedp, _ged_draw_set_transparency_cb, &ctx);
    bu_free((void *)dpp, "_ged_draw_set_transparency: directory pointers");
    return ctx.changed;
}


static int
_ged_draw_set_default_mode(struct ged *gedp, bsg_draw_mode mode)
{
    if (!gedp)
	return 0;
    if (gedp->i->ged_gdp->gd_shaded_mode == (int)mode)
	return 0;
    gedp->i->ged_gdp->gd_shaded_mode = (int)mode;
    return 1;
}


static int
_ged_draw_visibility_group_cb(const struct ged_draw_group_record *rec,
			      void *userdata)
{
    struct ged_draw_visibility_ctx *ctx =
	(struct ged_draw_visibility_ctx *)userdata;
    if (!ctx || !rec)
	return 1;

    const char *tail = rec->path;
    if (tail) {
	const char *p = tail;
	while (*p) {
	    if (*p == '/')
		tail = p + 1;
	    p++;
	}
    }
    if (!((rec->path && BU_STR_EQUAL(rec->path, ctx->path)) ||
	  (tail && BU_STR_EQUAL(tail, ctx->basename))))
	return 1;

    bsg_scene_ref group_ref =
	ged_draw_registry_group_scene_ref(ctx->gedp, rec->ref);
    if (!bsg_scene_ref_is_null(group_ref) && rec->visible != ctx->visible) {
	bsg_scene_set_visible(group_ref, ctx->visible);
	ctx->changed++;
    }

    return 1;
}


static int
_ged_draw_visibility_shape_cb(const struct ged_draw_shape_record *rec,
			      void *userdata)
{
    struct ged_draw_visibility_ctx *ctx =
	(struct ged_draw_visibility_ctx *)userdata;
    if (!ctx || !rec || !rec->fullpath)
	return 1;

    int match = 0;
    for (size_t i = 0; i < rec->fullpath->fp_len; i++) {
	struct directory *dp = rec->fullpath->fp_names[i];
	if (dp && BU_STR_EQUAL(dp->d_namep, ctx->basename)) {
	    match = 1;
	    break;
	}
    }
    if (!match)
	return 1;

    bsg_scene_ref shape_ref =
	ged_draw_registry_shape_scene_ref(ctx->gedp, rec->ref);
    if (!bsg_scene_ref_is_null(shape_ref) && rec->visible != ctx->visible) {
	bsg_scene_set_visible(shape_ref, ctx->visible);
	ctx->changed++;
    }

    return 1;
}


static int
_ged_draw_set_visibility(struct ged *gedp, const char *path, int visible)
{
    if (!gedp || !path)
	return 0;

    const char *basename = strrchr(path, '/');
    basename = basename ? basename + 1 : path;

    struct ged_draw_visibility_ctx ctx;
    ctx.gedp = gedp;
    ctx.path = ged_draw_dbpath_skip_lead_slash(path);
    ctx.basename = basename;
    ctx.visible = visible ? 1 : 0;
    ctx.changed = 0;

    ged_draw_foreach_group_record(gedp, _ged_draw_visibility_group_cb, &ctx);
    ged_draw_foreach_shape_record(gedp, _ged_draw_visibility_shape_cb, &ctx);
    return ctx.changed;
}


static int
_ged_draw_highlight_shape_cb(const struct ged_draw_shape_record *rec,
			     void *userdata)
{
    struct ged_draw_highlight_ctx *ctx =
	(struct ged_draw_highlight_ctx *)userdata;
    if (!ctx || !rec || !rec->fullpath)
	return 1;

    for (size_t i = 0; i < rec->fullpath->fp_len; i++) {
	struct directory *dp = rec->fullpath->fp_names[i];
	if (!dp || !BU_STR_EQUAL(dp->d_namep, ctx->basename))
	    continue;
	ctx->matched++;
	(void)ged_draw_shape_set_highlighted(ctx->gedp, rec->ref,
		ctx->highlighted);
	break;
    }

    return 1;
}


static int
_ged_draw_set_highlight(struct ged *gedp, const char *path, int highlighted)
{
    if (!gedp || !path)
	return 0;

    const char *basename = strrchr(path, '/');
    basename = basename ? basename + 1 : path;

    struct ged_draw_highlight_ctx ctx;
    ctx.gedp = gedp;
    ctx.basename = basename;
    ctx.highlighted = highlighted ? 1 : 0;
    ctx.matched = 0;

    ged_draw_foreach_shape_record(gedp, _ged_draw_highlight_shape_cb, &ctx);
    ged_draw_highlighted_shape_ref_invalidate(gedp);
    return ctx.matched;
}


static fastf_t
_ged_draw_wireframe_shape(bsg_scene_ref shape_ref, struct bsg_view *gvp,
			  struct db_i *dbip, const struct bn_tol *tol,
			  const struct bg_tess_tol *ttol)
{
    struct rt_db_internal dbintern;
    struct rt_db_internal *ip = &dbintern;
    int ret = -1;

    ged_draw_shape_state *shape_data = ged_draw_scene_ref_shape_state(shape_ref);
    if (!shape_data)
	return 0;
    if (shape_data->s_fullpath.fp_len <= 0)
	return 0;

    mat_t draw_mat;
    ged_draw_scene_ref_draw_mat(shape_ref, draw_mat);
    int get_ret = rt_db_get_internal(ip, DB_FULL_PATH_CUR_DIR(&shape_data->s_fullpath),
			     dbip, draw_mat);
    if (get_ret < 0)
	return -1;

    struct bsg_lod_source_policy_settings lod_policy = ged_draw_lod_policy(gvp);
    if (gvp && lod_policy.csg_enabled)
	ret = ged_draw_scene_ref_publish_primitive_wireframe(shape_ref, ip,
		ttol, tol, gvp, 1);
    if (ret < 0)
	ret = ged_draw_scene_ref_publish_primitive_wireframe(shape_ref, ip,
		ttol, tol, gvp, 0);

    rt_db_free_internal(ip);

    if (ret < 0) {
	if (DB_FULL_PATH_CUR_DIR(&shape_data->s_fullpath))
	    bu_log("%s: plot failure\n",
		   DB_FULL_PATH_CUR_DIR(&shape_data->s_fullpath)->d_namep);
	return -1;
    }

    return 0;
}


static int
_ged_draw_redraw_shape(bsg_scene_ref shape_ref, struct db_i *dbip,
		       struct db_tree_state *tsp, struct bsg_view *gvp)
{
    if (bsg_scene_dmode(shape_ref) != _GED_WIREFRAME)
	return 0;

    ged_draw_shape_state *shape_data = ged_draw_scene_ref_shape_state(shape_ref);
    if (!shape_data || shape_data->s_fullpath.fp_len <= 0)
	return 0;

    ged_draw_scene_ref_geometry_clear(shape_ref);
    int ret = _ged_draw_wireframe_shape(shape_ref, gvp, dbip, tsp->ts_tol, tsp->ts_ttol);
    struct ged_draw_source_state *d =
	ged_draw_scene_ref_source_data(shape_ref);
    if (d) {
	if (ret < 0) {
	    d->stale_reason = GED_DRAW_STALE_UPDATE_FAILED;
	} else {
	    d->realized_source_revision = d->source_revision;
	    d->realized_inputs_revision = d->inputs_revision;
	    d->stale_reason = GED_DRAW_STALE_NONE;
	}
    }
    if (shape_data) {
	if (ret < 0) {
	    shape_data->stale_reason = GED_DRAW_STALE_UPDATE_FAILED;
	} else {
	    shape_data->realized_source_revision = shape_data->source_revision;
	    shape_data->realized_inputs_revision = shape_data->inputs_revision;
	    shape_data->stale_reason = GED_DRAW_STALE_NONE;
	    if (d) {
		d->realized_source_revision = shape_data->realized_source_revision;
		d->realized_inputs_revision = shape_data->realized_inputs_revision;
	    }
	}
	ged_draw_scene_ref_mark_realization_result(shape_ref,
		shape_data->source_revision,
		shape_data->inputs_revision,
		ret < 0);
    } else if (d) {
	ged_draw_scene_ref_mark_realization_result(shape_ref,
		d->source_revision,
		d->inputs_revision,
		ret < 0);
    }
    ged_draw_scene_ref_database_source_sync(shape_ref, d, shape_data);
    return ret;
}


static void
_ged_draw_redraw_scene_ref(bsg_scene_ref ref, struct ged_draw_redraw_ctx *ctx)
{
    if (!ctx || bsg_scene_ref_is_null(ref))
	return;

    if (bsg_scene_ref_type(ref) == BSG_SCENE_ELEMENT_SHAPE) {
	if (!ctx->skip_subtractions || !bsg_scene_line_style(ref))
	    ctx->ret += _ged_draw_redraw_shape(ref, ctx->dbip, ctx->tsp, ctx->gvp);
    }

    size_t child_count = bsg_scene_child_count(ref);
    for (size_t i = 0; i < child_count; i++)
	_ged_draw_redraw_scene_ref(bsg_scene_child_at(ref, i), ctx);
}


static int
_ged_draw_redraw_group_scene_ref(struct ged *gedp, bsg_scene_ref group_ref,
				 int skip_subtractions,
				 struct bsg_view *view)
{
    if (!gedp || bsg_scene_ref_is_null(group_ref))
	return -1;

    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    struct ged_draw_redraw_ctx ctx;
    ctx.dbip = gedp->dbip;
    ctx.tsp = &wdbp->wdb_initial_tree_state;
    ctx.gvp = view ? view : gedp->ged_gvp;
    ctx.skip_subtractions = skip_subtractions;
    ctx.ret = 0;

    _ged_draw_redraw_scene_ref(group_ref, &ctx);

    return ctx.ret;
}


int
ged_draw_redraw_group_ref(struct ged *gedp, ged_draw_group_ref ref,
			  int skip_subtractions)
{
    return _ged_draw_redraw_group_scene_ref(gedp,
	    ged_draw_registry_group_scene_ref(gedp, ref), skip_subtractions,
	    NULL);
}


void
ged_draw_revalidate_db_event(struct ged *gedp, const struct bsg_db_event *ev)
{
    if (!gedp || !ev || !ev->dbe_path || !strlen(ev->dbe_path))
	return;

    bsg_scene_ref root_ref = ged_scene_root_ref(gedp);
    if (!bsg_scene_ref_is_null(root_ref))
	bsg_scene_draw_intent_revalidate(root_ref, ev);

    struct bu_ptbl *views = bsg_set_views(&gedp->ged_views);
    if (views) {
	for (size_t vi = 0; vi < BU_PTBL_LEN(views); vi++) {
	    struct bsg_view *v = (struct bsg_view *)BU_PTBL_GET(views, vi);
	    bsg_scene_ref view_root = v ? ged_draw_view_scene_root_ref(v) :
		bsg_scene_ref_null();
	    if (!bsg_scene_ref_is_null(view_root) &&
		    !bsg_scene_ref_equal(view_root, root_ref))
		bsg_scene_draw_intent_revalidate(view_root, ev);
	}
    }
}


static int
_ged_draw_redraw_all_cb(const struct ged_draw_group_record *rec, void *ud)
{
    struct ged_draw_redraw_path_ctx *ctx =
	(struct ged_draw_redraw_path_ctx *)ud;
    if (!rec)
	return 1;
    if (ctx->view && !ged_draw_group_record_in_view(rec, ctx->view))
	return 1;
    struct bsg_view *rv = ctx->view ? ctx->view :
	((rec->in_view_scope && rec->view) ? rec->view : ctx->gedp->ged_gvp);
    int ret = _ged_draw_redraw_group_scene_ref(ctx->gedp,
	    ged_draw_registry_group_scene_ref(ctx->gedp, rec->ref), 0, rv);
    if (ret < 0) {
	ctx->ret = -1;
	return 0;
    }
    ctx->found++;
    return 1;
}


static int
_ged_draw_redraw_path_cb(const struct ged_draw_group_record *rec, void *ud)
{
    struct ged_draw_redraw_path_ctx *ctx =
	(struct ged_draw_redraw_path_ctx *)ud;
    if (!rec)
	return 1;
    if (ctx->view && !ged_draw_group_record_in_view(rec, ctx->view))
	return 1;
    struct db_full_path draw_path;
    db_full_path_init(&draw_path);

    bsg_scene_ref group_ref =
	ged_draw_registry_group_scene_ref(ctx->gedp, rec->ref);
    int ret = ged_draw_group_scene_ref_dbpath(ctx->gedp, group_ref, &draw_path);
    if (ret < 0) {
	ctx->ret = -1;
	return 0;
    }

    if (db_full_path_match_top(&draw_path, ctx->obj_path)) {
	ctx->found = 1;
	db_free_full_path(&draw_path);
	struct bsg_view *rv = ctx->view ? ctx->view :
	    ((rec->in_view_scope && rec->view) ? rec->view : ctx->gedp->ged_gvp);
	ctx->ret = _ged_draw_redraw_group_scene_ref(ctx->gedp, group_ref, 0, rv);
	return 0;
    }

    db_free_full_path(&draw_path);
    return 1;
}


static int
_ged_draw_redraw(struct ged *gedp, const char *path, struct bsg_view *view)
{
    if (!gedp)
	return -1;

    if (!path) {
	struct ged_draw_redraw_path_ctx ctx;
	ctx.gedp = gedp;
	ctx.obj_path = NULL;
	ctx.view = view;
	ctx.found = 0;
	ctx.ret = 0;
	ged_draw_foreach_group_record(gedp, _ged_draw_redraw_all_cb, &ctx);
	if (ctx.ret < 0)
	    return -1;
	return ctx.found;
    }

    struct db_full_path obj_path;
    db_full_path_init(&obj_path);
    int ret = db_string_to_path(&obj_path, gedp->dbip,
				ged_draw_dbpath_skip_lead_slash(path));
    if (ret < 0)
	return -1;

    struct ged_draw_redraw_path_ctx ctx;
    ctx.gedp = gedp;
    ctx.obj_path = &obj_path;
    ctx.view = view;
    ctx.found = 0;
    ctx.ret = 0;
    ged_draw_foreach_group_record(gedp, _ged_draw_redraw_path_cb, &ctx);
    db_free_full_path(&obj_path);

    if (ctx.ret < 0)
	return -1;
    return ctx.found ? 1 : 0;
}


static int
_ged_draw_redraw_source_add_shape(struct ged_draw_redraw_source_ctx *ctx,
				  ged_draw_shape_ref ref)
{
    if (!ctx || ged_draw_shape_ref_is_null(ref))
	return 1;

    struct ged_draw_group_record grec;
    if (!ged_draw_group_record_get(ctx->gedp,
	    ged_draw_group_ref_of_shape(ctx->gedp, ref), &grec))
	return 1;
    if (grec.is_overlay)
	return 1;
    if (ctx->view && !ged_draw_group_record_in_view(&grec, ctx->view))
	return 1;

    struct ged_draw_redraw_shape_entry *entry =
	(struct ged_draw_redraw_shape_entry *)bu_calloc(1, sizeof(*entry),
		"redraw source shape entry");
    entry->ref = ref;
    entry->view = ctx->view ? ctx->view :
	((grec.in_view_scope && grec.view) ? grec.view : ctx->gedp->ged_gvp);
    bu_ptbl_ins(&ctx->shape_refs, (long *)entry);
    return 1;
}


static int
_ged_draw_redraw_source_shape_cb(const struct ged_draw_shape_record *rec,
				 void *userdata)
{
    struct ged_draw_redraw_source_ctx *ctx =
	(struct ged_draw_redraw_source_ctx *)userdata;
    if (!ctx || !rec)
	return 1;
    if (ctx->path && !_ged_draw_shape_record_has_component(rec, ctx->path))
	return 1;

    return _ged_draw_redraw_source_add_shape(ctx, rec->ref);
}


static int
_ged_draw_redraw_source_index_cb(bsg_scene_ref shape_ref, void *userdata)
{
    struct ged_draw_redraw_source_ctx *ctx =
	(struct ged_draw_redraw_source_ctx *)userdata;
    if (!ctx || bsg_scene_ref_is_null(shape_ref))
	return 1;

    return _ged_draw_redraw_source_add_shape(ctx,
	    ged_draw_shape_ref_from_scene_ref(ctx->gedp, shape_ref));
}


static int
_ged_draw_redraw_source(struct ged *gedp, const char *path,
			struct bsg_view *view)
{
    if (!gedp || !gedp->dbip)
	return -1;

    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    if (!wdbp)
	return -1;

    struct ged_draw_redraw_source_ctx ctx;
    ctx.gedp = gedp;
    ctx.path = path ? ged_draw_dbpath_skip_lead_slash(path) : NULL;
    ctx.view = view;
    bu_ptbl_init(&ctx.shape_refs, 8, "redraw source shape refs");

    int indexed = ctx.path ?
	ged_draw_shape_index_for_component(gedp, ctx.path,
		_ged_draw_redraw_source_index_cb, &ctx) : -1;
    if (indexed < 0) {
	struct ged_drawable *gdp = _ged_draw_gdp(gedp);
	if (gdp)
	    gdp->gd_draw_index_fallback_shape_scans++;
	ged_draw_foreach_shape_record(gedp, _ged_draw_redraw_source_shape_cb,
		&ctx);
    }

    int redrawn = 0;
    int failed = 0;
    for (size_t i = 0; i < BU_PTBL_LEN(&ctx.shape_refs); i++) {
	struct ged_draw_redraw_shape_entry *entry =
	    (struct ged_draw_redraw_shape_entry *)BU_PTBL_GET(&ctx.shape_refs, i);
	if (!entry)
	    continue;
	entry->ref.scene_revision = 0;
	bsg_scene_ref shape_ref =
	    ged_draw_registry_shape_scene_ref(gedp, entry->ref);
	if (!bsg_scene_ref_is_null(shape_ref)) {
	    int ret = _ged_draw_redraw_shape(shape_ref, gedp->dbip,
		    &wdbp->wdb_initial_tree_state,
		    entry->view ? entry->view : gedp->ged_gvp);
	    if (ret < 0)
		failed = 1;
	    else
		redrawn++;
	}
	bu_free(entry, "redraw source shape entry");
    }
    bu_ptbl_free(&ctx.shape_refs);

    return failed ? -1 : redrawn;
}


static int
ged_draw_txn_view_array(struct ged *gedp,
			struct bsg_view *v,
			struct bsg_view ***views_out)
{
    if (views_out)
	*views_out = NULL;
    if (!gedp || !v || !views_out)
	return 0;

    size_t count = 0;
    if (bsg_view_is_independent(v)) {
	count = 1;
    } else {
	struct bu_ptbl *views = bsg_set_views(&gedp->ged_views);
	for (size_t i = 0; i < BU_PTBL_LEN(views); i++) {
	    struct bsg_view *lv = (struct bsg_view *)BU_PTBL_GET(views, i);
	    if (lv && !bsg_view_is_independent(lv))
		count++;
	}
	if (!count)
	    count = 1;
    }

    struct bsg_view **out = (struct bsg_view **)bu_calloc(count,
	    sizeof(struct bsg_view *), "draw transaction view array");
    if (bsg_view_is_independent(v)) {
	out[0] = v;
    } else {
	size_t idx = 0;
	struct bu_ptbl *views = bsg_set_views(&gedp->ged_views);
	for (size_t i = 0; i < BU_PTBL_LEN(views); i++) {
	    struct bsg_view *lv = (struct bsg_view *)BU_PTBL_GET(views, i);
	    if (lv && !bsg_view_is_independent(lv))
		out[idx++] = lv;
	}
	if (!idx)
	    out[idx++] = v;
	count = idx;
    }

    *views_out = out;
    return (int)count;
}


static int
ged_draw_prepare_views_for_transaction(struct ged *gedp,
				       struct bsg_view *v)
{
    struct bsg_view **views = NULL;
    size_t view_count = ged_draw_txn_view_array(gedp, v, &views);
    if (!view_count || !views)
	return 0;

    for (size_t i = 0; i < view_count; i++) {
	if (views[i])
	    views[i]->gv_bounds_update = &bsg_view_bounds;
    }

    bu_free(views, "draw transaction view array");
    return (int)view_count;
}


static int
ged_draw_autoview_for_transaction(struct ged *gedp,
				  struct bsg_view *v)
{
    struct bsg_view **views = NULL;
    size_t view_count = ged_draw_txn_view_array(gedp, v, &views);
    if (!view_count || !views)
	return 0;

    int adjusted = 0;
    for (size_t i = 0; i < view_count; i++) {
	if (!views[i])
	    continue;
	bsg_autoview(views[i], BSG_AUTOVIEW_SCALE_DEFAULT, 0);
	adjusted++;
    }

    bu_free(views, "draw transaction view array");
    return adjusted;
}


static int
ged_draw_finalize_lod_shape_cb(const struct ged_draw_shape_record *rec,
			       void *userdata)
{
    struct ged_draw_lod_finalize_ctx *ctx =
	(struct ged_draw_lod_finalize_ctx *)userdata;
    if (!ctx || !rec)
	return 1;
    if (ged_draw_shape_ref_lod_ensure(ctx->gedp, rec->ref, ctx->first_view,
	    ctx->views, ctx->view_count))
	ctx->ensured++;
    return 1;
}


static int
ged_draw_finalize_lod_for_transaction(struct ged *gedp,
				      struct bsg_view *v)
{
    if (!gedp || !v)
	return 0;

    struct bsg_view **views = NULL;
    size_t view_count = ged_draw_txn_view_array(gedp, v, &views);
    if (!view_count || !views)
	return 0;

    struct ged_draw_lod_finalize_ctx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.gedp = gedp;
    ctx.first_view = v;
    ctx.views = views;
    ctx.view_count = view_count;

    ged_draw_foreach_shape_record(gedp, ged_draw_finalize_lod_shape_cb, &ctx);

    bu_free(views, "draw transaction view array");
    return ctx.ensured;
}


static struct ged_draw_source_state *
ged_draw_txn_source_state_create(struct ged *gedp,
				 const struct db_full_path *path)
{
    if (!gedp || !gedp->dbip || !path || path->fp_len <= 0)
	return NULL;

    struct ged_draw_source_state *ud;
    BU_GET(ud, struct ged_draw_source_state);
    memset(ud, 0, sizeof(*ud));

    BU_GET(ud->fp, struct db_full_path);
    db_full_path_init(ud->fp);
    db_dup_full_path(ud->fp, path);

    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    ud->dbip = gedp->dbip;
    ud->tol = wdbp ? &wdbp->wdb_tol : NULL;
    ud->ttol = wdbp ? &wdbp->wdb_ttol : NULL;
    ud->mesh_c = gedp->ged_lod;
    ud->stale_reason = GED_DRAW_STALE_NONE;
    return ud;
}


static int
ged_draw_path_is_subtraction(struct ged *gedp,
			     const struct db_full_path *path)
{
    if (!gedp || !gedp->dbip || !path)
	return 0;

    int op = db_fp_op(path, gedp->dbip, 0);
    return (op == DB_OP_SUBTRACT || op == OP_SUBTRACT) ? 1 : 0;
}


static ged_draw_shape_ref
ged_draw_create_evaluated_shape_ref(struct ged *gedp,
				    struct bsg_view *v,
				    const char *path,
				    const struct bsg_appearance_settings *settings)
{
    if (!gedp || !gedp->dbip || !v || !path || !path[0] || !settings)
	return GED_DRAW_SHAPE_REF_NULL;

    struct db_full_path dfp;
    db_full_path_init(&dfp);
    if (db_string_to_path(&dfp, gedp->dbip, path) != 0)
	return GED_DRAW_SHAPE_REF_NULL;

    ged_draw_group_ref group_ref =
	ged_draw_group_ref_lookup_or_create(gedp, &dfp);
    if (ged_draw_group_ref_is_null(group_ref)) {
	db_free_full_path(&dfp);
	return GED_DRAW_SHAPE_REF_NULL;
    }
    ged_draw_group_ref_set_appearance(gedp, group_ref, settings);

    ged_draw_shape_draft *draft = ged_draw_shape_draft_create(gedp, v, 0);
    if (!draft) {
	db_free_full_path(&dfp);
	return GED_DRAW_SHAPE_REF_NULL;
    }

    char *name = db_path_to_string(&dfp);
    if (name) {
	ged_draw_shape_draft_set_name(draft,
		ged_draw_dbpath_skip_lead_slash(name));
    }
    ged_draw_shape_draft_set_fullpath(draft, &dfp);
    ged_draw_shape_draft_set_source_state(draft,
	    ged_draw_txn_source_state_create(gedp, &dfp));
    ged_draw_shape_draft_mark_db_object(draft);

    struct bu_color c;
    unsigned char color[3] = {255, 0, 0};
    db_full_path_color(&c, &dfp, gedp->dbip);
    bu_color_to_rgb_chars(&c, color);
    if (settings->color_override) {
	color[0] = settings->color[0];
	color[1] = settings->color[1];
	color[2] = settings->color[2];
    }
    ged_draw_shape_draft_set_material_rgb(draft, color[0], color[1], color[2]);
    ged_draw_shape_draft_set_draw_mode(draft, settings->draw_mode);

    mat_t node_mat;
    MAT_IDN(node_mat);
    if (db_path_to_mat(gedp->dbip, &dfp, node_mat, 0))
	ged_draw_shape_draft_set_transform(draft, node_mat);

    point_t bmin, bmax;
    VSETALL(bmin, INFINITY);
    VSETALL(bmax, -INFINITY);
    const char *bounds_path = name ? ged_draw_dbpath_skip_lead_slash(name) : path;
    const char *bounds_argv[1] = {bounds_path};
    if (rt_obj_bounds(gedp->ged_result_str, gedp->dbip, 1, bounds_argv, 0,
	    bmin, bmax) != BRLCAD_ERROR) {
	ged_draw_shape_draft_set_bounds(draft, bmin, bmax);
	vect_t center;
	VSET(center,
	    (bmin[X] + bmax[X]) * 0.5,
	    (bmin[Y] + bmax[Y]) * 0.5,
	    (bmin[Z] + bmax[Z]) * 0.5);
	fastf_t size = bmax[X] - bmin[X];
	V_MAX(size, bmax[Y] - bmin[Y]);
	V_MAX(size, bmax[Z] - bmin[Z]);
	ged_draw_shape_draft_set_center_size(draft, center, size);
    }

    int is_subtraction = ged_draw_path_is_subtraction(gedp, &dfp);
    if (!settings->draw_solid_lines_only)
	ged_draw_shape_draft_set_line_style(draft, is_subtraction ? 1 : 0);
    if (settings->draw_non_subtract_only && is_subtraction)
	ged_draw_shape_draft_set_visible(draft, 0);
    if (settings->s_line_width)
	ged_draw_shape_draft_set_line_width(draft, settings->s_line_width);
    ged_draw_shape_draft_set_transparency(draft, settings->transparency);
    ged_draw_shape_draft_apply_settings(draft, settings);

    ged_draw_shape_ref ref =
	ged_draw_shape_draft_commit_to_group(draft, group_ref);
    if (name)
	bu_free(name, "draw evaluated path name");
    db_free_full_path(&dfp);
    return ref;
}


static int
ged_draw_realize_shape_refs_for_transaction(struct ged *gedp,
					    struct bsg_view *v,
					    ged_draw_shape_ref *refs,
					    int ref_count)
{
    if (!gedp || !v || !refs || ref_count <= 0)
	return 0;

    struct bsg_view **views = NULL;
    size_t view_count = ged_draw_txn_view_array(gedp, v, &views);
    if (!view_count || !views)
	return 0;

    int realized = 0;
    for (size_t vi = 0; vi < view_count; vi++) {
	if (!views[vi])
	    continue;
	for (int ri = 0; ri < ref_count; ri++) {
	    if (ged_draw_shape_ref_is_null(refs[ri]))
		continue;
	    if (ged_draw_shape_ref_realize(gedp, refs[ri], views[vi]))
		realized++;
	}
    }

    bu_free(views, "draw transaction view array");
    return realized;
}


static int
ged_draw_apply_evaluated_paths(struct ged *gedp,
			       struct bsg_view *v,
			       const char **paths,
			       int path_count,
			       const struct bsg_appearance_settings *settings)
{
    if (!gedp || !v || !paths || path_count <= 0 || !settings)
	return -1;

    ged_draw_shape_ref *refs = (ged_draw_shape_ref *)bu_calloc(
	    (size_t)path_count, sizeof(ged_draw_shape_ref),
	    "evaluated draw shape refs");

    int created = 0;
    for (int i = 0; i < path_count; i++) {
	refs[i] = ged_draw_create_evaluated_shape_ref(gedp, v, paths[i],
		settings);
	if (!ged_draw_shape_ref_is_null(refs[i]))
	    created++;
    }

    if (created > 0)
	(void)ged_draw_realize_shape_refs_for_transaction(gedp, v, refs,
		path_count);

    bu_free(refs, "evaluated draw shape refs");
    return created;
}


static int
_ged_draw_paths_realized(struct ged *gedp,
			 struct bsg_view *v,
			 const char **paths,
			 int path_count)
{
    if (!gedp || !v || !paths || path_count <= 0)
	return 0;

    int realized = 0;
    for (int i = 0; i < path_count; i++) {
	if (paths[i] && paths[i][0] &&
		ged_draw_path_state(gedp, v, paths[i], -1) > 0)
	    realized++;
    }
    return realized;
}


static int
_ged_draw_apply_draw(struct ged *gedp,
		     const struct ged_draw_transaction *txn,
		     const char *path,
		     struct ged_draw_transaction_result *result)
{
    if (!gedp || !txn)
	return -1;

    struct bsg_view *v = txn->view ? txn->view : gedp->ged_gvp;
    if (!v)
	return -1;

    const char *single_path[1] = {NULL};
    const char **paths = txn->paths;
    int path_count = txn->path_count;
    if ((!paths || path_count <= 0) && path) {
	single_path[0] = path;
	paths = single_path;
	path_count = 1;
    }
    if (!paths || path_count <= 0)
	return 0;

    const char **draw_paths = (const char **)bu_calloc(path_count + 1,
	    sizeof(const char *), "draw transaction paths");
    int draw_count = 0;
    for (int i = 0; i < path_count; i++) {
	if (paths[i] && paths[i][0])
	    draw_paths[draw_count++] = paths[i];
    }
    if (draw_count <= 0) {
	bu_free((void *)draw_paths, "draw transaction paths");
	return 0;
    }
    struct bsg_appearance_settings settings = BSG_APPEARANCE_SETTINGS_INIT;
    if (txn->appearance)
	settings = *txn->appearance;

    for (int i = 0; i < draw_count; i++) {
	struct ged_draw_transaction erase_txn =
	    ged_draw_transaction_make(GED_DRAW_TXN_ERASE, draw_paths[i]);
	erase_txn.view = v;
	erase_txn.mode = (txn->mode >= 0) ? txn->mode :
	    (settings.mixed_modes ? (int)settings.draw_mode : -1);
	ged_draw_apply_transaction(gedp, &erase_txn, NULL);
    }

    struct bsg_view *saved_view = gedp->ged_gvp;
    gedp->ged_gvp = v;
    (void)ged_draw_prepare_views_for_transaction(gedp, v);

    struct _ged_client_data dgcdp;
    memset(&dgcdp, 0, sizeof(dgcdp));
    dgcdp.gedp = gedp;
    dgcdp.v = v;
    dgcdp.autoview = txn->autoview ? 1 : 0;
    dgcdp.nmg_triangulate = 1;
    dgcdp.vs = settings;

    int ret = 0;
    if (settings.draw_mode == BSG_DRAW_MODE_EVAL_WIRE ||
	    settings.draw_mode == BSG_DRAW_MODE_EVAL_POINTS) {
	ret = ged_draw_apply_evaluated_paths(gedp, v, draw_paths, draw_count,
		&settings);
	if (ret < 0)
	    ret = -1;
	else
	    ret = 0;
    } else {
	ret = _ged_drawtrees(gedp, draw_count, draw_paths, _GED_DRAW_WIREFRAME,
		&dgcdp);
    }
    gedp->ged_gvp = saved_view;

    if (ret != 0) {
	/* Legacy draw walking may report failure for multi-path instance
	 * draws even after realizing all requested child-path groups.  Trust
	 * the retained draw state in that case so transaction observers and
	 * callers see the scene change that actually happened. */
	int realized = _ged_draw_paths_realized(gedp, v, draw_paths,
		draw_count);
	if (realized == draw_count)
	    ret = 0;
    }

    bu_free((void *)draw_paths, "draw transaction paths");

    if (ret != 0)
	return -1;

    if (txn->autoview)
	(void)ged_draw_autoview_for_transaction(gedp, v);
    (void)ged_draw_finalize_lod_for_transaction(gedp, v);
    if (txn->autoview)
	(void)ged_draw_autoview_for_transaction(gedp, v);
    (void)ged_selection_draw_sync(gedp, NULL);

    if (result) {
	result->affected_groups = draw_count;
	result->affected_shapes = ged_draw_shape_count(gedp);
    }
    return draw_count;
}


static int
_ged_draw_source_is_comb(struct ged *gedp, const char *path)
{
    if (!gedp || !gedp->dbip || !path)
	return 0;

    const char *name = _ged_draw_component_name(path);
    if (!name || !*name)
	return 0;

    struct directory *dp = db_lookup(gedp->dbip, name, LOOKUP_QUIET);
    return (dp && (dp->d_flags & RT_DIR_COMB)) ? 1 : 0;
}


static int
_ged_draw_reexpand_group_seen(const struct ged_draw_reexpand_source_ctx *ctx,
			      ged_draw_group_ref ref)
{
    if (!ctx || ged_draw_group_ref_is_null(ref))
	return 1;

    for (size_t i = 0; i < BU_PTBL_LEN(&ctx->groups); i++) {
	const struct ged_draw_reexpand_group_entry *entry =
	    (const struct ged_draw_reexpand_group_entry *)BU_PTBL_GET(
		    &ctx->groups, i);
	if (entry && entry->ref.token == ref.token)
	    return 1;
    }

    return 0;
}


static int
_ged_draw_reexpand_group_add(struct ged_draw_reexpand_source_ctx *ctx,
			     const struct ged_draw_group_record *rec)
{
    if (!ctx || !rec || !rec->path || rec->is_overlay ||
	    ged_draw_group_ref_is_null(rec->ref))
	return 0;
    if (ctx->view && !ged_draw_group_record_in_view(rec, ctx->view))
	return 0;
    if (_ged_draw_reexpand_group_seen(ctx, rec->ref))
	return 0;

    struct ged_draw_reexpand_group_entry *entry =
	(struct ged_draw_reexpand_group_entry *)bu_calloc(1, sizeof(*entry),
		"reexpand group entry");
    entry->ref = rec->ref;
    entry->path = bu_strdup(rec->path);
    entry->view = ctx->view ? ctx->view :
	((rec->in_view_scope && rec->view) ? rec->view : ctx->gedp->ged_gvp);
    struct bsg_appearance_settings appearance = BSG_APPEARANCE_SETTINGS_INIT;
    bsg_scene_ref group_ref =
	ged_draw_registry_group_scene_ref(ctx->gedp, rec->ref);
    const struct bsg_draw_intent *di =
	bsg_scene_ref_is_null(group_ref) ? NULL :
	bsg_scene_draw_intent(group_ref);
    if (!bsg_draw_intent_appearance(di, &appearance)) {
	appearance.draw_mode = rec->draw_mode;
	appearance.transparency = rec->transparency;
    }
    entry->appearance = appearance;
    bu_ptbl_ins(&ctx->groups, (long *)entry);
    return 1;
}


static int
_ged_draw_reexpand_group_add_ref(struct ged_draw_reexpand_source_ctx *ctx,
				 ged_draw_group_ref ref)
{
    if (!ctx || ged_draw_group_ref_is_null(ref))
	return 0;

    struct ged_draw_group_record rec;
    if (!ged_draw_group_record_get(ctx->gedp, ref, &rec))
	return 0;
    return _ged_draw_reexpand_group_add(ctx, &rec);
}


static int
_ged_draw_reexpand_source_group_cb(const struct ged_draw_group_record *rec,
				   void *userdata)
{
    struct ged_draw_reexpand_source_ctx *ctx =
	(struct ged_draw_reexpand_source_ctx *)userdata;
    if (!ctx || !rec || !rec->path)
	return 1;
    if (ctx->path && !_ged_draw_path_has_component(rec->path, ctx->path))
	return 1;
    (void)_ged_draw_reexpand_group_add(ctx, rec);
    return 1;
}


static int
_ged_draw_reexpand_source_shape_cb(const struct ged_draw_shape_record *rec,
				   void *userdata)
{
    struct ged_draw_reexpand_source_ctx *ctx =
	(struct ged_draw_reexpand_source_ctx *)userdata;
    if (!ctx || !rec)
	return 1;
    if (ctx->path && !_ged_draw_shape_record_has_component(rec, ctx->path))
	return 1;

    struct ged_draw_group_record grec;
    if (!ged_draw_group_record_get(ctx->gedp, rec->group, &grec))
	return 1;
    (void)_ged_draw_reexpand_group_add(ctx, &grec);
    return 1;
}


static int
_ged_draw_reexpand_source_group_index_cb(bsg_scene_ref group_ref,
					 void *userdata)
{
    struct ged_draw_reexpand_source_ctx *ctx =
	(struct ged_draw_reexpand_source_ctx *)userdata;
    if (!ctx || bsg_scene_ref_is_null(group_ref))
	return 1;

    (void)_ged_draw_reexpand_group_add_ref(ctx,
	    ged_draw_group_ref_from_scene_ref(ctx->gedp, group_ref));
    return 1;
}


static int
_ged_draw_reexpand_source_shape_index_cb(bsg_scene_ref shape_ref,
					 void *userdata)
{
    struct ged_draw_reexpand_source_ctx *ctx =
	(struct ged_draw_reexpand_source_ctx *)userdata;
    if (!ctx || bsg_scene_ref_is_null(shape_ref))
	return 1;

    ged_draw_shape_ref ref =
	ged_draw_shape_ref_from_scene_ref(ctx->gedp, shape_ref);
    (void)_ged_draw_reexpand_group_add_ref(ctx,
	    ged_draw_group_ref_of_shape(ctx->gedp, ref));
    return 1;
}


static int
_ged_draw_reexpand_source_groups(struct ged *gedp, const char *path,
				 struct bsg_view *view)
{
    if (!gedp)
	return -1;

    struct ged_draw_reexpand_source_ctx ctx;
    ctx.gedp = gedp;
    ctx.path = path ? ged_draw_dbpath_skip_lead_slash(path) : NULL;
    ctx.view = view;
    bu_ptbl_init(&ctx.groups, 8, "reexpand source groups");

    int groups_indexed = ctx.path ?
	ged_draw_group_index_for_component(gedp, ctx.path,
		_ged_draw_reexpand_source_group_index_cb, &ctx) : -1;
    int shapes_indexed = ctx.path ?
	ged_draw_shape_index_for_component(gedp, ctx.path,
		_ged_draw_reexpand_source_shape_index_cb, &ctx) : -1;

    if (groups_indexed < 0 || shapes_indexed < 0) {
	struct ged_drawable *gdp = _ged_draw_gdp(gedp);
	if (gdp) {
	    gdp->gd_draw_index_fallback_group_scans++;
	    gdp->gd_draw_index_fallback_shape_scans++;
	}
	ged_draw_foreach_group_record(gedp,
		_ged_draw_reexpand_source_group_cb, &ctx);
	ged_draw_foreach_shape_record(gedp,
		_ged_draw_reexpand_source_shape_cb, &ctx);
    }

    int reexpanded = 0;
    int failed = 0;
    for (size_t i = 0; i < BU_PTBL_LEN(&ctx.groups); i++) {
	struct ged_draw_reexpand_group_entry *entry =
	    (struct ged_draw_reexpand_group_entry *)BU_PTBL_GET(&ctx.groups, i);
	if (!entry)
	    continue;

	struct bsg_appearance_settings settings = entry->appearance;

	struct ged_draw_transaction txn =
	    ged_draw_transaction_make(GED_DRAW_TXN_DRAW, entry->path);
	txn.view = entry->view;
	txn.appearance = &settings;
	txn.mode = settings.draw_mode;
	txn.autoview = 0;
	if (_ged_draw_apply_draw(gedp, &txn, entry->path, NULL) < 0)
	    failed = 1;
	else
	    reexpanded++;

	bu_free(entry->path, "reexpand group path");
	bu_free(entry, "reexpand group entry");
    }
    bu_ptbl_free(&ctx.groups);

    return failed ? -1 : reexpanded;
}


struct ged_draw_transaction
ged_draw_transaction_make(ged_draw_transaction_kind kind, const char *path)
{
    struct ged_draw_transaction txn;
    memset(&txn, 0, sizeof(txn));
    txn.kind = kind;
    txn.path = path;
    txn.mode = -1;
    txn.stale_reason = GED_DRAW_STALE_SOURCE_CHANGED;
    return txn;
}


struct ged_draw_transaction
ged_draw_transaction_make_value(ged_draw_transaction_kind kind,
				const char *path,
				double value)
{
    struct ged_draw_transaction txn = ged_draw_transaction_make(kind, path);
    txn.value = value;
    return txn;
}


void
ged_draw_transaction_result_init(struct ged_draw_transaction_result *result)
{
    if (!result)
	return;
    memset(result, 0, sizeof(*result));
    BU_VLS_INIT(&result->names);
    BU_VLS_INIT(&result->errors);
}


void
ged_draw_transaction_result_free(struct ged_draw_transaction_result *result)
{
    if (!result)
	return;
    if (BU_VLS_IS_INITIALIZED(&result->names))
	bu_vls_free(&result->names);
    if (BU_VLS_IS_INITIALIZED(&result->errors))
	bu_vls_free(&result->errors);
    memset(result, 0, sizeof(*result));
}


static void
_ged_draw_txn_result_prepare(struct ged_draw_transaction_result *result,
			     struct ged *gedp)
{
    if (!result)
	return;
    if (!BU_VLS_IS_INITIALIZED(&result->names)) {
	BU_VLS_INIT(&result->names);
    } else {
	bu_vls_trunc(&result->names, 0);
    }
    if (!BU_VLS_IS_INITIALIZED(&result->errors)) {
	BU_VLS_INIT(&result->errors);
    } else {
	bu_vls_trunc(&result->errors, 0);
    }
    result->status = 0;
    result->affected_groups = 0;
    result->affected_shapes = 0;
    result->stale_count = 0;
    result->redrawn_count = 0;
    result->error_count = 0;
    result->scene_revision_before = ged_draw_scene_revision(gedp);
    result->scene_revision_after = result->scene_revision_before;
}


static const char *
_ged_draw_txn_path(struct ged *gedp,
		   const struct ged_draw_transaction *txn,
		   struct bu_vls *tmp)
{
    if (!txn)
	return NULL;
    if (txn->path)
	return txn->path;
    if (!txn->dfp)
	return NULL;
    char *s = db_path_to_string(txn->dfp);
    if (!s)
	return NULL;
    bu_vls_sprintf(tmp, "%s", ged_draw_dbpath_skip_lead_slash(s));
    bu_free(s, "ged_draw transaction path");
    (void)gedp;
    return bu_vls_cstr(tmp);
}


static void
_ged_draw_txn_note_name(struct ged_draw_transaction_result *result,
			const char *name)
{
    if (!result || !name || !*name)
	return;
    if (bu_vls_strlen(&result->names))
	bu_vls_putc(&result->names, ' ');
    bu_vls_printf(&result->names, "%s", name);
}


static int
_ged_draw_apply_transaction_inner(struct ged *gedp,
				  const struct ged_draw_transaction *txn,
				  struct ged_draw_transaction_result *result,
				  const char *path)
{
    if (!gedp || !txn)
	return 0;

    bsg_scene_ref root = ged_scene_root_ref(gedp);
    uint64_t rev0 = gedp->i->ged_gdp->gd_draw_rev;
    size_t child_count0 = bsg_scene_ref_is_null(root) ? 0 : bsg_scene_child_count(root);
    int ret = 0;

    switch (txn->kind) {
	case GED_DRAW_TXN_ERASE:
	    if (!path)
		return 0;
	    if (txn->view || txn->mode >= 0)
		ret = ged_draw_erase_path_string_scoped(gedp, path,
			txn->view, txn->mode);
	    else
		ret = ged_draw_erase_path_string(gedp, path);
	    if (result) {
		result->affected_groups = ret;
		result->affected_shapes = ret;
	    }
	    break;
	case GED_DRAW_TXN_ERASE_PREFIX:
	    if (!path)
		return 0;
	    if (txn->view || txn->mode >= 0)
		ret = ged_draw_erase_path_prefix_string_scoped(gedp, path,
			txn->view, txn->mode);
	    else
		ret = ged_draw_erase_path_prefix_string(gedp, path);
	    if (result) {
		result->affected_groups = ret;
		result->affected_shapes = ret;
	    }
	    break;
	case GED_DRAW_TXN_TEARDOWN:
	case GED_DRAW_TXN_CLEAR:
	{
	    ged_draw_clear(gedp);
	    ret = (child_count0 > 0 || rev0 != 0) ? 1 : 0;
	    if (result)
		result->affected_groups = (int)child_count0;
	    break;
	}
	case GED_DRAW_TXN_CLEAR_SCOPE:
	    ret = ged_draw_clear_view(gedp, txn->view);
	    if (result)
		result->affected_groups = ret;
	    break;
	case GED_DRAW_TXN_TRANSPARENCY:
	    if (!path)
		return 0;
	    ret = _ged_draw_set_transparency(gedp, path, (fastf_t)txn->value);
	    if (result)
		result->affected_shapes = ret;
	    break;
	case GED_DRAW_TXN_DEFAULT_DRAW_MODE:
	    ret = _ged_draw_set_default_mode(gedp, (bsg_draw_mode)((int)txn->value));
	    break;
	case GED_DRAW_TXN_VISIBILITY:
	    if (!path)
		return 0;
	    ret = _ged_draw_set_visibility(gedp, path, !ZERO(txn->value));
	    if (result) {
		result->affected_groups = ret;
		result->affected_shapes = ret;
	    }
	    break;
	case GED_DRAW_TXN_HIGHLIGHT:
	    if (!path)
		return 0;
	    ret = _ged_draw_set_highlight(gedp, path, !ZERO(txn->value));
	    if (result)
		result->affected_shapes = ret;
	    break;
	case GED_DRAW_TXN_MATERIAL_CHANGED:
	    ged_draw_bump_material_revision(gedp);
	    ret = 1;
	    if (result)
		result->affected_shapes = ged_draw_shape_count(gedp);
	    break;
	case GED_DRAW_TXN_REFRESH_MATERIAL_COLORS:
	    ged_draw_refresh_material_colors(gedp);
	    ret = 1;
	    if (result)
		result->affected_shapes = ged_draw_shape_count(gedp);
	    break;
	case GED_DRAW_TXN_REDRAW:
	    ret = _ged_draw_redraw(gedp, path, txn->view);
	    if (ret >= 0)
		(void)ged_draw_finalize_lod_for_transaction(gedp,
			txn->view ? txn->view : gedp->ged_gvp);
	    if (result)
		result->redrawn_count = (ret > 0) ? ret : 0;
	    break;
	case GED_DRAW_TXN_STALE_SOURCE:
	    ret = ged_draw_mark_database_change(gedp, path, txn->stale_reason);
	    if (result)
		result->stale_count = ret;
	    break;
	case GED_DRAW_TXN_SOURCE_UPDATED:
	    ret = ged_draw_apply_database_update(gedp, path, txn->removed,
		    txn->redraw);
	    if (result) {
		if (txn->removed)
		    result->affected_groups = (ret > 0) ? ret : 0;
		else if (txn->redraw)
		    result->redrawn_count = (ret > 0) ? ret : 0;
		else
		    result->stale_count = (ret > 0) ? ret : 0;
	    }
	    break;
	case GED_DRAW_TXN_SOURCE_RENAMED:
	    if (!path || !txn->new_path)
		return 0;
	    ret = _ged_draw_apply_database_rename(gedp, path, txn->new_path);
	    if (result)
		result->affected_groups = (ret > 0) ? ret : 0;
	    break;
	case GED_DRAW_TXN_SOURCE_REFERENCES_REMOVED:
	    if (!path)
		return 0;
	    ret = ged_draw_erase_nonroot_component_string_scoped(gedp, path,
		    txn->view, txn->mode);
	    if (result) {
		result->affected_groups = ret;
		result->affected_shapes = ret;
	    }
	    break;
	case GED_DRAW_TXN_DRAW:
	    ret = _ged_draw_apply_draw(gedp, txn, path, result);
	    break;
	case GED_DRAW_TXN_NONE:
	default:
	    ret = 0;
	    break;
    }

    if (result && ret) {
	if (path) {
	    _ged_draw_txn_note_name(result, path);
	} else if (txn->paths && txn->path_count > 0) {
	    for (int i = 0; i < txn->path_count; i++)
		_ged_draw_txn_note_name(result, txn->paths[i]);
	}
    }
    return ret;
}


int
ged_draw_apply_transaction(struct ged *gedp,
			   const struct ged_draw_transaction *txn,
			   struct ged_draw_transaction_result *result)
{
    if (!gedp || !txn) {
	if (result) {
	    _ged_draw_txn_result_prepare(result, gedp);
	    result->status = 0;
	}
	return 0;
    }

    struct ged_draw_transaction_result local_result;
    int use_local_result = 0;
    if (!result && _ged_draw_observers_have_active(gedp)) {
	ged_draw_transaction_result_init(&local_result);
	result = &local_result;
	use_local_result = 1;
    }

    struct bu_vls path_tmp = BU_VLS_INIT_ZERO;
    const char *path = _ged_draw_txn_path(gedp, txn, &path_tmp);
    if (result)
	_ged_draw_txn_result_prepare(result, gedp);

    int ret = _ged_draw_apply_transaction_inner(gedp, txn, result, path);
    if (result) {
	result->status = ret;
	result->scene_revision_after = ged_draw_scene_revision(gedp);
	if (ret < 0) {
	    result->error_count = 1;
	    bu_vls_printf(&result->errors, "draw transaction failed");
	    if (path)
		bu_vls_printf(&result->errors, ": %s", path);
	}
	_ged_draw_observers_dispatch(gedp, txn, result);
    }

    bu_vls_free(&path_tmp);
    if (use_local_result)
	ged_draw_transaction_result_free(&local_result);
    return ret;
}


int
ged_draw_apply_database_update(struct ged *gedp,
			       const char *path,
			       int removed,
			       int redraw)
{
    if (!gedp)
	return 0;
    if (removed) {
	if (!path)
	    return 0;
	return ged_draw_erase_component_string_scoped(gedp,
		ged_draw_dbpath_skip_lead_slash(path), NULL, -1);
    }

    int marked = ged_draw_mark_database_change(gedp, path,
	    GED_DRAW_STALE_SOURCE_CHANGED);
    if (!redraw)
	return marked;

    if (path && _ged_draw_source_is_comb(gedp, path)) {
	int reexpanded = _ged_draw_reexpand_source_groups(gedp, path, NULL);
	if (reexpanded < 0)
	    return reexpanded;
	if (reexpanded > 0)
	    return marked ? marked : reexpanded;
    }

    int redrawn = _ged_draw_redraw_source(gedp, path, NULL);
    if (redrawn < 0)
	return redrawn;
    return marked ? marked : redrawn;
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
