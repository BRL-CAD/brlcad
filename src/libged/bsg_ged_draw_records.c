/*            B S G _ G E D _ D R A W _ R E C O R D S . C
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
/** @file bsg_ged_draw_records.c
 *
 * Semantic GED draw-record construction and iteration.
 *
 * This file is intentionally record/ref-facing.  It may resolve the temporary
 * draw registry to retained nodes internally, but it must not grow new raw
 * node-storage dependencies.
 */

#include "common.h"

#include <string.h>

#include "bu/hash.h"
#include "bu/malloc.h"
#include "bu/sort.h"
#include "bu/str.h"
#include "bsg/appearance.h"
#include "bsg/database_source.h"
#include "bsg/draw_intent.h"
#include "bsg/draw_source.h"
#include "bsg/interaction.h"
#include "bsg/scene_builder.h"
#include "bsg/selection.h"
#include "ged/bsg_ged_draw.h"
#include "bsg/util.h"
#include "./ged_private.h"
#include "./bsg_ged_draw_private.h"


static bsg_scene_ref
_draw_scene_for_shape_ref(struct ged *gedp, ged_draw_shape_ref ref)
{
    return ged_draw_registry_shape_scene_ref(gedp, ref);
}


static bsg_scene_ref
_draw_scene_for_group_ref(struct ged *gedp, ged_draw_group_ref ref)
{
    return ged_draw_registry_group_scene_ref(gedp, ref);
}


static int
_draw_foreach_shape_scene(bsg_scene_ref ref,
			  int (*cb)(bsg_scene_ref, void *),
			  void *userdata);


static const char *
_dbpath_skip_lead_slash(const char *s)
{
    if (s && *s == '/')
	return s + 1;
    return s;
}


static void
_draw_path_normalize(struct bu_vls *out, const char *path)
{
    if (!out)
	return;
    bu_vls_trunc(out, 0);
    if (!path)
	return;

    path = ged_draw_dbpath_skip_lead_slash(path);
    bu_vls_strcpy(out, path);
    while (bu_vls_strlen(out) > 0 &&
	    bu_vls_addr(out)[bu_vls_strlen(out) - 1] == '/')
	bu_vls_trunc(out, bu_vls_strlen(out) - 1);
}


static int
_draw_path_equal(const char *a, const char *b)
{
    if (!a || !b)
	return 0;
    a = ged_draw_dbpath_skip_lead_slash(a);
    b = ged_draw_dbpath_skip_lead_slash(b);
    return BU_STR_EQUAL(a, b);
}


static int
_draw_path_is_prefix(const char *prefix, const char *path)
{
    if (!prefix || !path)
	return 0;
    prefix = ged_draw_dbpath_skip_lead_slash(prefix);
    path = ged_draw_dbpath_skip_lead_slash(path);
    size_t plen = strlen(prefix);
    if (plen == 0)
	return 1;
    if (bu_strncmp(prefix, path, plen) != 0)
	return 0;
    return path[plen] == '\0' || path[plen] == '/';
}


struct _draw_path_leaf_ctx {
    bu_hash_tbl *paths;
    size_t count;
};


static union tree *
_draw_path_leaf_cb(struct db_tree_state *UNUSED(tsp),
		   const struct db_full_path *pathp,
		   struct rt_db_internal *UNUSED(ip),
		   void *client_data)
{
    struct _draw_path_leaf_ctx *ctx =
	(struct _draw_path_leaf_ctx *)client_data;
    if (!ctx || !ctx->paths || !pathp)
	return TREE_NULL;

    char *path = db_path_to_string(pathp);
    if (!path)
	return TREE_NULL;
    const char *key = ged_draw_dbpath_skip_lead_slash(path);
    if (key && key[0] && bu_hash_set(ctx->paths, (const uint8_t *)key,
	    strlen(key), (void *)1) == 1)
	ctx->count++;
    bu_free(path, "draw path leaf string");
    return TREE_NULL;
}


static size_t
_draw_path_expected_leaf_paths(struct ged *gedp, const char *path,
			       bu_hash_tbl *expected)
{
    if (!gedp || !gedp->dbip || !path || !expected)
	return 0;

    struct db_tree_state state;
    db_init_db_tree_state(&state, gedp->dbip);
    struct _draw_path_leaf_ctx ctx;
    ctx.paths = expected;
    ctx.count = 0;

    const char *av[1] = {path};
    int ret = db_walk_tree(gedp->dbip, 1, av, 1, &state,
	    NULL, NULL, _draw_path_leaf_cb, (void *)&ctx);
    db_free_db_tree_state(&state);

    if (ret < 0)
	return 0;
    return ctx.count;
}


static bsg_scene_ref
_draw_group_scene_ref_of_shape(bsg_scene_ref shape_ref)
{
    if (bsg_scene_ref_is_null(shape_ref))
	return bsg_scene_ref_null();

    bsg_scene_ref group_ref = bsg_scene_parent(shape_ref);
    while (!bsg_scene_ref_is_null(group_ref)) {
	bsg_scene_ref parent_ref = bsg_scene_parent(group_ref);
	if (bsg_scene_ref_is_null(parent_ref))
	    return group_ref;
	bsg_scene_ref grandparent_ref = bsg_scene_parent(parent_ref);
	if (bsg_scene_ref_is_null(grandparent_ref))
	    return group_ref;
	group_ref = parent_ref;
    }
    return bsg_scene_ref_null();
}


static int
_draw_scene_ref_in_view_scope(bsg_scene_ref ref)
{
    bsg_scene_ref cur = ref;

    while (!bsg_scene_ref_is_null(cur)) {
	if (bsg_scene_is_view_scope(cur))
	    return 1;
	cur = bsg_scene_parent(cur);
    }

    return 0;
}


static int
_draw_group_scene_ref_in_view(bsg_scene_ref group_ref, struct bsg_view *v)
{
    if (bsg_scene_ref_is_null(group_ref))
	return 0;

    int in_view_scope = _draw_scene_ref_in_view_scope(group_ref);
    if (!v)
	return !in_view_scope;
    if (bsg_view_is_independent(v))
	return in_view_scope && bsg_scene_view(group_ref) == v;
    if (!in_view_scope)
	return 1;
    return bsg_scene_view(group_ref) == v;
}


int
ged_draw_group_record_in_view(const struct ged_draw_group_record *rec,
			      struct bsg_view *v)
{
    if (!rec)
	return 0;
    if (!v)
	return !rec->in_view_scope;
    if (bsg_view_is_independent(v))
	return rec->in_view_scope && rec->view == v;
    if (!rec->in_view_scope)
	return 1;
    return rec->view == v;
}


struct _draw_path_state_ctx {
    struct ged *gedp;
    struct bsg_view *view;
    const char *path;
    int mode;
    bu_hash_tbl *drawn_leaf_paths;
    int exact_shape;
    int descendant_shape;
};


static int
_draw_path_state_shape_scene_cb(bsg_scene_ref shape_ref, void *ud)
{
    struct _draw_path_state_ctx *ctx =
	(struct _draw_path_state_ctx *)ud;
    if (!ctx || bsg_scene_ref_is_null(shape_ref) || !bsg_scene_visible(shape_ref))
	return 1;
    if (ctx->mode >= 0 && bsg_scene_dmode(shape_ref) != ctx->mode)
	return 1;

    bsg_scene_ref group_ref = _draw_group_scene_ref_of_shape(shape_ref);
    if (ged_draw_group_scene_ref_is_overlay(group_ref) ||
	    !bsg_scene_visible(group_ref) ||
	    !_draw_group_scene_ref_in_view(group_ref, ctx->view))
	return 1;

    const struct db_full_path *fullpath = ged_draw_scene_ref_fullpath(shape_ref);
    if (!fullpath)
	return 1;

    char *path = db_path_to_string(fullpath);
    if (!path)
	return 1;
    const char *key = ged_draw_dbpath_skip_lead_slash(path);

    if (_draw_path_equal(ctx->path, key))
	ctx->exact_shape = 1;
    if (_draw_path_is_prefix(ctx->path, key))
	ctx->descendant_shape = 1;
    if (key && key[0])
	(void)bu_hash_set(ctx->drawn_leaf_paths,
		(const uint8_t *)key, strlen(key), (void *)1);

    bu_free(path, "draw shape fullpath string");
    return 1;
}


int
ged_draw_path_state(struct ged *gedp,
		    struct bsg_view *v,
		    const char *path,
		    int mode)
{
    if (!gedp || !path)
	return 0;

    struct bu_vls norm = BU_VLS_INIT_ZERO;
    _draw_path_normalize(&norm, path);
    if (bu_vls_strlen(&norm) == 0) {
	bu_vls_free(&norm);
	return 0;
    }

    bu_hash_tbl *drawn = bu_hash_create(0);
    if (!drawn) {
	bu_vls_free(&norm);
	return 0;
    }

    struct _draw_path_state_ctx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.gedp = gedp;
    ctx.view = v;
    ctx.path = bu_vls_cstr(&norm);
    ctx.mode = mode;
    ctx.drawn_leaf_paths = drawn;

    _draw_foreach_shape_scene(ged_scene_root_ref(gedp),
	    _draw_path_state_shape_scene_cb, &ctx);

    if (ctx.exact_shape) {
	bu_hash_destroy(drawn);
	bu_vls_free(&norm);
	return 1;
    }

    if (!ctx.descendant_shape) {
	bu_hash_destroy(drawn);
	bu_vls_free(&norm);
	return 0;
    }

    bu_hash_tbl *expected = bu_hash_create(0);
    if (!expected) {
	bu_hash_destroy(drawn);
	bu_vls_free(&norm);
	return 0;
    }

    size_t expected_count =
	_draw_path_expected_leaf_paths(gedp, bu_vls_cstr(&norm), expected);

    int state = 0;
    if (expected_count > 0) {
	size_t matched = 0;
	bu_hash_entry *e = NULL;
	while ((e = bu_hash_next(expected, e)) != NULL) {
	    uint8_t *key = NULL;
	    size_t key_len = 0;
	    if (bu_hash_key(e, &key, &key_len) == 0 &&
		    bu_hash_get(drawn, key, key_len))
		matched++;
	}

	if (ctx.exact_shape || matched == expected_count)
	    state = 1;
	else if (matched > 0 || ctx.descendant_shape)
	    state = 2;
    } else {
	if (ctx.exact_shape)
	    state = 1;
	else if (ctx.descendant_shape)
	    state = 2;
    }

    bu_hash_destroy(expected);
    bu_hash_destroy(drawn);
    bu_vls_free(&norm);
    return state;
}


struct _draw_path_list_ctx {
    struct ged *gedp;
    struct bsg_view *view;
    int mode;
    bu_hash_tbl *seen;
    bu_hash_tbl *state_cache;
    char **paths;
    size_t count;
    size_t cap;
};


static void
_draw_path_list_free(struct _draw_path_list_ctx *ctx)
{
    if (!ctx)
	return;
    for (size_t i = 0; i < ctx->count; i++)
	bu_free(ctx->paths[i], "draw path list string");
    if (ctx->paths)
	bu_free(ctx->paths, "draw path list");
    ctx->paths = NULL;
    ctx->count = 0;
    ctx->cap = 0;
    if (ctx->seen)
	bu_hash_destroy(ctx->seen);
    ctx->seen = NULL;
    if (ctx->state_cache)
	bu_hash_destroy(ctx->state_cache);
    ctx->state_cache = NULL;
}


static int
_draw_path_list_add(struct _draw_path_list_ctx *ctx, const char *path)
{
    if (!ctx || !ctx->seen || !path || !*path)
	return 1;

    path = ged_draw_dbpath_skip_lead_slash(path);
    if (!path || !*path)
	return 1;

    size_t len = strlen(path);
    if (bu_hash_set(ctx->seen, (const uint8_t *)path, len, (void *)1) != 1)
	return 1;

    if (ctx->count == ctx->cap) {
	size_t ncap = ctx->cap ? ctx->cap * 2 : 32;
	char **npaths = (char **)bu_realloc(ctx->paths,
		ncap * sizeof(char *), "draw path list");
	ctx->paths = npaths;
	ctx->cap = ncap;
    }
    ctx->paths[ctx->count++] = bu_strdup(path);
    return 1;
}


static int
_draw_path_strcmp(const void *a, const void *b, void *UNUSED(data))
{
    const char * const *sa = (const char * const *)a;
    const char * const *sb = (const char * const *)b;
    return bu_strcmp(*sa, *sb);
}


static int
_draw_path_list_add_collapsed(struct _draw_path_list_ctx *ctx, const char *path)
{
    if (!ctx || !path || !*path)
	return 1;
    path = ged_draw_dbpath_skip_lead_slash(path);

    for (size_t i = 0; i < ctx->count; i++) {
	if (_draw_path_equal(ctx->paths[i], path) ||
		_draw_path_is_prefix(ctx->paths[i], path))
	    return 1;
    }

    size_t write = 0;
    for (size_t i = 0; i < ctx->count; i++) {
	if (_draw_path_is_prefix(path, ctx->paths[i])) {
	    bu_hash_rm(ctx->seen, (const uint8_t *)ctx->paths[i],
		    strlen(ctx->paths[i]));
	    bu_free(ctx->paths[i], "draw path list collapsed descendant");
	    continue;
	}
	ctx->paths[write++] = ctx->paths[i];
    }
    ctx->count = write;

    return _draw_path_list_add(ctx, path);
}


static int
_draw_path_list_cached_state(struct _draw_path_list_ctx *ctx, const char *path)
{
    if (!ctx || !ctx->state_cache || !path)
	return 0;

    path = ged_draw_dbpath_skip_lead_slash(path);
    if (!path || !*path)
	return 0;

    size_t len = strlen(path);
    void *cached = bu_hash_get(ctx->state_cache, (const uint8_t *)path, len);
    if (cached)
	return (int)((uintptr_t)cached) - 1;

    int state = ged_draw_path_state(ctx->gedp, ctx->view, path, ctx->mode);
    (void)bu_hash_set(ctx->state_cache, (const uint8_t *)path, len,
	    (void *)(uintptr_t)(state + 1));
    return state;
}


static int
_draw_list_shape_scene_path_collapsed_cb(bsg_scene_ref shape_ref, void *ud)
{
    struct _draw_path_list_ctx *ctx = (struct _draw_path_list_ctx *)ud;
    if (!ctx || bsg_scene_ref_is_null(shape_ref) || !bsg_scene_visible(shape_ref))
	return 1;
    if (ctx->mode >= 0 && bsg_scene_dmode(shape_ref) != ctx->mode)
	return 1;

    bsg_scene_ref group_ref = _draw_group_scene_ref_of_shape(shape_ref);
    if (ged_draw_group_scene_ref_is_overlay(group_ref) ||
	    !bsg_scene_visible(group_ref) ||
	    !_draw_group_scene_ref_in_view(group_ref, ctx->view))
	return 1;

    const struct db_full_path *fullpath = ged_draw_scene_ref_fullpath(shape_ref);
    if (!fullpath)
	return 1;

    char *path = db_path_to_string(fullpath);
    if (!path)
	return 1;
    const char *spath = ged_draw_dbpath_skip_lead_slash(path);
    if (!spath || !*spath) {
	bu_free(path, "draw list collapsed shape path");
	return 1;
    }

    struct bu_vls candidate = BU_VLS_INIT_ZERO;
    const char *best = spath;
    size_t len = strlen(spath);
    for (size_t i = 0; i <= len; i++) {
	if (spath[i] != '/' && spath[i] != '\0')
	    continue;
	bu_vls_trunc(&candidate, 0);
	bu_vls_strncpy(&candidate, spath, i);
	if (_draw_path_list_cached_state(ctx, bu_vls_cstr(&candidate)) == 1) {
	    best = bu_vls_cstr(&candidate);
	    break;
	}
    }

    (void)_draw_path_list_add_collapsed(ctx, best);
    bu_vls_free(&candidate);
    bu_free(path, "draw list collapsed shape path");
    return 1;
}


static int
_draw_list_shape_scene_path_cb(bsg_scene_ref shape_ref, void *ud)
{
    struct _draw_path_list_ctx *ctx = (struct _draw_path_list_ctx *)ud;
    if (!ctx || bsg_scene_ref_is_null(shape_ref) || !bsg_scene_visible(shape_ref))
	return 1;
    if (ctx->mode >= 0 && bsg_scene_dmode(shape_ref) != ctx->mode)
	return 1;

    bsg_scene_ref group_ref = _draw_group_scene_ref_of_shape(shape_ref);
    if (ged_draw_group_scene_ref_is_overlay(group_ref) ||
	    !bsg_scene_visible(group_ref) ||
	    !_draw_group_scene_ref_in_view(group_ref, ctx->view))
	return 1;

    const struct db_full_path *fullpath = ged_draw_scene_ref_fullpath(shape_ref);
    if (!fullpath)
	return 1;

    char *path = db_path_to_string(fullpath);
    if (!path)
	return 1;
    (void)_draw_path_list_add(ctx, path);
    bu_free(path, "draw list shape path");
    return 1;
}


struct _draw_has_paths_ctx {
    struct bsg_view *view;
    int mode;
    int found;
};


static int
_draw_has_paths_shape_scene_cb(bsg_scene_ref shape_ref, void *ud)
{
    struct _draw_has_paths_ctx *ctx = (struct _draw_has_paths_ctx *)ud;
    if (!ctx || bsg_scene_ref_is_null(shape_ref) || !bsg_scene_visible(shape_ref))
	return 1;
    if (ctx->mode >= 0 && bsg_scene_dmode(shape_ref) != ctx->mode)
	return 1;

    bsg_scene_ref group_ref = _draw_group_scene_ref_of_shape(shape_ref);
    if (ged_draw_group_scene_ref_is_overlay(group_ref) ||
	    !bsg_scene_visible(group_ref) ||
	    !_draw_group_scene_ref_in_view(group_ref, ctx->view))
	return 1;

    if (!ged_draw_scene_ref_fullpath(shape_ref))
	return 1;

    ctx->found = 1;
    return 0;
}


size_t
ged_draw_list_paths(struct ged *gedp,
		    struct bsg_view *v,
		    int mode,
		    int expanded,
		    struct bu_vls *result)
{
    if (!gedp || !result)
	return 0;

    struct _draw_path_list_ctx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.gedp = gedp;
    ctx.view = v;
    ctx.mode = mode;
    ctx.seen = bu_hash_create(0);
    ctx.state_cache = bu_hash_create(0);
    if (!ctx.seen || !ctx.state_cache) {
	_draw_path_list_free(&ctx);
	return 0;
    }

    if (expanded)
	_draw_foreach_shape_scene(ged_scene_root_ref(gedp),
		_draw_list_shape_scene_path_cb, &ctx);
    else
	_draw_foreach_shape_scene(ged_scene_root_ref(gedp),
		_draw_list_shape_scene_path_collapsed_cb, &ctx);

    if (ctx.count > 1)
	bu_sort(ctx.paths, ctx.count, sizeof(char *), _draw_path_strcmp, NULL);

    for (size_t i = 0; i < ctx.count; i++)
	bu_vls_printf(result, "%s\n", ctx.paths[i]);

    size_t count = ctx.count;
    _draw_path_list_free(&ctx);
    return count;
}


int
ged_draw_has_paths(struct ged *gedp,
		   struct bsg_view *v,
		   int mode)
{
    if (!gedp)
	return 0;

    struct _draw_has_paths_ctx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.view = v;
    ctx.mode = mode;

    _draw_foreach_shape_scene(ged_scene_root_ref(gedp),
	    _draw_has_paths_shape_scene_cb, &ctx);
    return ctx.found;
}


static int
_draw_foreach_shape_scene(bsg_scene_ref ref,
			  int (*cb)(bsg_scene_ref, void *),
			  void *userdata)
{
    if (bsg_scene_ref_is_null(ref))
	return 1;

    if (bsg_scene_ref_type(ref) == BSG_SCENE_ELEMENT_SHAPE) {
	if (ged_draw_shape_state_get_scene_ref(ref))
	    return (*cb)(ref, userdata);
	return 1;
    }

    for (size_t i = 0; i < bsg_scene_child_count(ref); i++) {
	bsg_scene_ref child_ref = bsg_scene_child_at(ref, i);
	if (!_draw_foreach_shape_scene(child_ref, cb, userdata))
	    return 0;
    }
    return 1;
}


static void
_ged_draw_fill_shape_record(struct ged *gedp,
			    bsg_scene_ref shape_ref,
			    struct ged_draw_shape_record *out)
{
    memset(out, 0, sizeof(*out));
    out->ref = ged_draw_shape_ref_from_scene_ref(gedp, shape_ref);
    out->group = ged_draw_group_ref_of_shape(gedp, out->ref);
    ged_draw_shape_state *shape_data = ged_draw_scene_ref_shape_state(shape_ref);
    if (shape_data) {
	out->fullpath = &shape_data->s_fullpath;
	out->display_name = shape_data->display_name;
	out->path_hash = shape_data->path_hash;
	out->source_revision = shape_data->source_revision;
	out->inputs_revision = shape_data->inputs_revision;
	out->realized_source_revision = shape_data->realized_source_revision;
	out->realized_inputs_revision = shape_data->realized_inputs_revision;
	out->stale = (shape_data->stale_reason != GED_DRAW_STALE_NONE ||
		shape_data->source_revision != shape_data->realized_source_revision ||
		shape_data->inputs_revision != shape_data->realized_inputs_revision);
	if (shape_data->stale_reason != GED_DRAW_STALE_NONE)
	    out->stale_reason =
		ged_draw_stale_reason_name(shape_data->stale_reason);
	else if (shape_data->source_revision !=
		shape_data->realized_source_revision)
	    out->stale_reason =
		ged_draw_stale_reason_name(GED_DRAW_STALE_SOURCE_CHANGED);
	else if (shape_data->inputs_revision !=
		shape_data->realized_inputs_revision)
	    out->stale_reason =
		ged_draw_stale_reason_name(GED_DRAW_STALE_VIEW_INPUT_CHANGED);
	else
	    out->stale_reason =
		ged_draw_stale_reason_name(GED_DRAW_STALE_NONE);
	if (shape_data->leaf_dp)
	    out->leaf_name = shape_data->leaf_dp->d_namep;
	else if (shape_data->s_fullpath.fp_len > 0 &&
		shape_data->s_fullpath.fp_names[shape_data->s_fullpath.fp_len - 1])
	    out->leaf_name =
		shape_data->s_fullpath.fp_names[shape_data->s_fullpath.fp_len - 1]->d_namep;
    }
    out->visible = bsg_scene_visible(shape_ref);
    out->highlighted = bsg_scene_highlighted(shape_ref);
    out->selected = 0;
    out->evaluated_region = bsg_scene_legacy_eval_flag(shape_ref);
    struct bsg_selection *selection = (gedp && gedp->ged_gvp) ?
	bsg_view_selection(gedp->ged_gvp) : NULL;
    if (selection) {
	struct bsg_interaction_record *selected =
	    ged_draw_shape_interaction_record(gedp, out->ref,
		    BSG_INTERACTION_SELECTED_PATH);
	if (selected) {
	    out->selected = bsg_selection_contains_record(selection, selected);
	    bsg_interaction_record_free(selected);
	}
    }
    struct ged_draw_source_state *d = ged_draw_scene_ref_source_data(shape_ref);
    if (!shape_data && d) {
	out->source_revision = d->source_revision;
	out->inputs_revision = d->inputs_revision;
	out->realized_source_revision = d->realized_source_revision;
	out->realized_inputs_revision = d->realized_inputs_revision;
	out->stale = (d->stale_reason != GED_DRAW_STALE_NONE ||
		d->source_revision != d->realized_source_revision ||
		d->inputs_revision != d->realized_inputs_revision);
	if (d->stale_reason != GED_DRAW_STALE_NONE)
	    out->stale_reason = ged_draw_stale_reason_name(d->stale_reason);
	else if (d->source_revision != d->realized_source_revision)
	    out->stale_reason =
		ged_draw_stale_reason_name(GED_DRAW_STALE_SOURCE_CHANGED);
	else if (d->inputs_revision != d->realized_inputs_revision)
	    out->stale_reason =
		ged_draw_stale_reason_name(GED_DRAW_STALE_VIEW_INPUT_CHANGED);
	else
	    out->stale_reason = ged_draw_stale_reason_name(GED_DRAW_STALE_NONE);
    } else if (!shape_data) {
	out->stale_reason = ged_draw_stale_reason_name(GED_DRAW_STALE_NONE);
    }
    struct bsg_database_source_record source_record;
    bsg_database_source_ref source_ref =
	bsg_database_source_ref_from_scene(ged_draw_shape_source_ref(shape_ref));
    if (bsg_database_source_record_get(source_ref,
	    &source_record) &&
	    ged_draw_database_source_record_has_state(&source_record)) {
	out->source_revision = source_record.source_revision;
	out->inputs_revision = source_record.inputs_revision;
	out->realized_source_revision = source_record.realized_source_revision;
	out->realized_inputs_revision = source_record.realized_inputs_revision;
	out->stale =
	    bsg_database_source_ref_is_stale(source_ref);
	if (source_record.stale_reason != BSG_DATABASE_SOURCE_STALE_NONE)
	    out->stale_reason =
		ged_draw_database_source_stale_reason_name(source_record.stale_reason);
	else if (source_record.source_revision !=
		source_record.realized_source_revision)
	    out->stale_reason =
		ged_draw_stale_reason_name(GED_DRAW_STALE_SOURCE_CHANGED);
	else if (source_record.inputs_revision !=
		source_record.realized_inputs_revision)
	    out->stale_reason =
		ged_draw_stale_reason_name(GED_DRAW_STALE_VIEW_INPUT_CHANGED);
	else
	    out->stale_reason =
		ged_draw_stale_reason_name(GED_DRAW_STALE_NONE);
	if (!out->draw_mode)
	    out->draw_mode = source_record.draw_mode;
    }
    out->drawn_revision = bsg_scene_drawn_rev(shape_ref);
    out->transparency = bsg_scene_transparency(shape_ref);
    int draw_mode = bsg_scene_dmode(shape_ref);
    out->draw_mode = (bsg_draw_mode)draw_mode;
    out->line_width = bsg_scene_line_width(shape_ref);
    bsg_scene_draw_center(shape_ref, out->center);
}


int
ged_draw_shape_record_get(struct ged *gedp,
			  ged_draw_shape_ref ref,
			  struct ged_draw_shape_record *out)
{
    if (!out)
	return 0;
    bsg_scene_ref shape_ref = _draw_scene_for_shape_ref(gedp, ref);
    if (bsg_scene_ref_is_null(shape_ref))
	return 0;
    _ged_draw_fill_shape_record(gedp, shape_ref, out);
    return 1;
}


struct bsg_interaction_record *
ged_draw_shape_interaction_record(struct ged *gedp,
				  ged_draw_shape_ref ref,
				  bsg_interaction_kind kind)
{
    if (!gedp || ged_draw_shape_ref_is_null(ref))
	return NULL;

    bsg_scene_ref shape_ref = _draw_scene_for_shape_ref(gedp, ref);
    if (bsg_scene_ref_is_null(shape_ref))
	return NULL;

    char *path = NULL;
    const char *source_path = NULL;
    ged_draw_shape_state *shape_data = ged_draw_scene_ref_shape_state(shape_ref);
    if (shape_data && shape_data->s_fullpath.fp_len > 0) {
	path = db_path_to_string(&shape_data->s_fullpath);
	source_path = path;
    } else {
	const struct db_full_path *fp = ged_draw_scene_ref_fullpath(shape_ref);
	if (fp) {
	    path = db_path_to_string(fp);
	    source_path = path;
	}
    }
    if ((!source_path || !source_path[0]) && shape_data &&
	    shape_data->display_name && shape_data->display_name[0]) {
	source_path = shape_data->display_name;
    }
    if ((!source_path || !source_path[0]) &&
	    bsg_scene_name(shape_ref)) {
	source_path = bsg_scene_name(shape_ref);
    }

    struct bsg_interaction_record *record =
	bsg_interaction_record_create_ref(gedp->ged_gvp, kind,
		(bsg_feature_ref)BSG_FEATURE_REF_NULL_INIT,
		_dbpath_skip_lead_slash(source_path), NULL);

    if (path)
	bu_free(path, "db_path_to_string");
    return record;
}


static int
_ged_draw_count_shape_cb(bsg_scene_ref UNUSED(ref), void *ud)
{
    int *count = (int *)ud;
    (*count)++;
    return 1;
}


struct _first_shape_scene_ctx {
    bsg_scene_ref result;
};


static int
_ged_draw_first_shape_scene_cb(bsg_scene_ref ref, void *ud)
{
    struct _first_shape_scene_ctx *ctx = (struct _first_shape_scene_ctx *)ud;
    ctx->result = ref;
    return 0;
}


int
ged_draw_group_record_get(struct ged *gedp,
			  ged_draw_group_ref ref,
			  struct ged_draw_group_record *out)
{
    if (!out)
	return 0;
    bsg_scene_ref group_ref = _draw_scene_for_group_ref(gedp, ref);
    if (bsg_scene_ref_is_null(group_ref))
	return 0;
    memset(out, 0, sizeof(*out));
    out->ref = ref;
    out->path = ged_draw_group_scene_ref_path(group_ref);
    out->view = bsg_scene_view(group_ref);
    out->draw_mode = ged_draw_group_scene_ref_mode(group_ref);
    struct _first_shape_scene_ctx first_shape;
    first_shape.result = bsg_scene_ref_null();
    _draw_foreach_shape_scene(group_ref, _ged_draw_first_shape_scene_cb,
	    &first_shape);
    out->transparency = bsg_scene_ref_is_null(first_shape.result) ?
	0.0 : bsg_scene_transparency(first_shape.result);
    out->visible = bsg_scene_visible(group_ref);
    out->is_overlay = ged_draw_group_scene_ref_is_overlay(group_ref);
    out->is_view_scope = bsg_scene_is_view_scope(group_ref);
    out->in_view_scope = _draw_scene_ref_in_view_scope(group_ref);
    out->is_view_source = bsg_scene_is_view_source(group_ref);
    out->is_local_source = bsg_scene_is_local_source(group_ref);
    _draw_foreach_shape_scene(group_ref, _ged_draw_count_shape_cb,
	    &out->shape_count);
    return 1;
}


struct _foreach_group_record_ctx {
    struct ged *gedp;
    int (*cb)(const struct ged_draw_group_record *, void *);
    void *userdata;
};


static int
_foreach_group_record_cb(bsg_scene_ref group_ref, void *ud)
{
    struct _foreach_group_record_ctx *ctx =
	(struct _foreach_group_record_ctx *)ud;
    struct ged_draw_group_record rec;
    if (!ged_draw_group_record_get(ctx->gedp,
	    ged_draw_group_ref_from_scene_ref(ctx->gedp, group_ref), &rec))
	return 1;
    return ctx->cb(&rec, ctx->userdata);
}


static int
_draw_foreach_group_scene(bsg_scene_ref ref,
			  int (*cb)(bsg_scene_ref, void *),
			  void *userdata)
{
    if (bsg_scene_ref_is_null(ref))
	return 1;
    if (bsg_scene_ref_type(ref) == BSG_SCENE_ELEMENT_GROUP &&
	    !cb(ref, userdata))
	return 0;
    for (size_t i = 0; i < bsg_scene_child_count(ref); i++) {
	if (!_draw_foreach_group_scene(bsg_scene_child_at(ref, i), cb, userdata))
	    return 0;
    }
    return 1;
}


void
ged_draw_foreach_group_record(struct ged *gedp,
			      int (*cb)(const struct ged_draw_group_record *rec,
				  void *userdata),
			      void *userdata)
{
    if (!gedp || !cb)
	return;
    struct _foreach_group_record_ctx ctx;
    ctx.gedp = gedp;
    ctx.cb = cb;
    ctx.userdata = userdata;
    bsg_scene_ref root_ref = ged_scene_root_ref(gedp);
    for (size_t gi = 0; gi < bsg_scene_child_count(root_ref); gi++)
	if (!_draw_foreach_group_scene(bsg_scene_child_at(root_ref, gi),
		_foreach_group_record_cb, &ctx))
	    return;
}


struct _foreach_shape_record_ctx {
    struct ged *gedp;
    int (*cb)(const struct ged_draw_shape_record *, void *);
    void *userdata;
};


static int
_foreach_shape_record_cb(bsg_scene_ref ref, void *ud)
{
    struct _foreach_shape_record_ctx *ctx =
	(struct _foreach_shape_record_ctx *)ud;
    struct ged_draw_shape_record rec;
    _ged_draw_fill_shape_record(ctx->gedp, ref, &rec);
    return ctx->cb(&rec, ctx->userdata);
}


void
ged_draw_foreach_shape_record(struct ged *gedp,
			      int (*cb)(const struct ged_draw_shape_record *rec,
				  void *userdata),
			      void *userdata)
{
    if (!gedp || !cb)
	return;
    struct _foreach_shape_record_ctx ctx;
    ctx.gedp = gedp;
    ctx.cb = cb;
    ctx.userdata = userdata;
    bsg_scene_ref root_ref = ged_scene_root_ref(gedp);
    _draw_foreach_shape_scene(root_ref, _foreach_shape_record_cb, &ctx);
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
