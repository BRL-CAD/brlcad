/*                B S G _ G E D _ D R A W _ T R E E . C
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
/** @file libged/b_s_g___g_e_d___d_r_a_w___t_r_e_e_._c.c
 *
 * Scene-ref retained draw-tree mutation and group path management.
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#include "bu/color.h"
#include "bu/hash.h"
#include "bu/malloc.h"
#include "bu/ptbl.h"
#include "bu/str.h"
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
#include "bsg/scene_builder.h"
#include "bsg/scene_object.h"
#include "bsg/selection.h"
#include "bsg/view_set.h"
#include "bsg/view_state.h"
#include "bsg/util.h"
#include "bg/clip.h"

#include "ged.h"
#include "ged/bsg_ged_draw.h"
#include "ged/db_index.h"
#include "./bsg_ged_draw_private.h"
#include "./ged_private.h"


struct _sg_ref_snapshot {
    bsg_scene_ref *refs;
    size_t len;
    size_t cap;
};


static void
_sg_ref_snapshot_append(struct _sg_ref_snapshot *snap, bsg_scene_ref ref)
{
    if (!snap || bsg_scene_ref_is_null(ref))
	return;

    if (snap->len == snap->cap) {
	size_t new_cap = snap->cap ? snap->cap * 2 : 16;
	snap->refs = (bsg_scene_ref *)bu_realloc(snap->refs,
		new_cap * sizeof(bsg_scene_ref), "draw tree scene-ref snapshot");
	snap->cap = new_cap;
    }

    snap->refs[snap->len++] = ref;
}


static void
_sg_ref_snapshot_free(struct _sg_ref_snapshot *snap)
{
    if (!snap)
	return;
    if (snap->refs)
	bu_free(snap->refs, "draw tree scene-ref snapshot");
    snap->refs = NULL;
    snap->len = 0;
    snap->cap = 0;
}


static void
_sg_snapshot_children(struct _sg_ref_snapshot *snap, bsg_scene_ref parent,
		      int groups_only, int skip_overlays)
{
    if (!snap || bsg_scene_ref_is_null(parent))
	return;

    size_t child_count = bsg_scene_child_count(parent);
    for (size_t i = 0; i < child_count; i++) {
	bsg_scene_ref child = bsg_scene_child_at(parent, i);
	if (bsg_scene_ref_is_null(child))
	    continue;
	if (groups_only &&
		bsg_scene_ref_type(child) != BSG_SCENE_ELEMENT_GROUP)
	    continue;
	if (skip_overlays && ged_draw_group_scene_ref_is_overlay(child))
	    continue;
	_sg_ref_snapshot_append(snap, child);
    }
}


static bsg_scene_ref _sg_add_path(struct ged *gedp, const char *name);
static void _sg_erase_path(struct ged *gedp, const char *path);
static void _sg_erase_all_paths(struct ged *gedp, const char *path);
static void _sg_erase_path_scoped(struct ged *gedp, const char *path,
				  struct bsg_view *v, int mode);
static void _sg_erase_all_paths_scoped(struct ged *gedp, const char *path,
				       struct bsg_view *v, int mode);
static int _sg_erase_nonroot_component_scoped(struct ged *gedp,
					      const char *name,
					      struct bsg_view *v,
					      int mode);
static int _sg_erase_component_scoped(struct ged *gedp,
				      const char *name,
				      struct bsg_view *v,
				      int mode);
static int _sg_prune_empty_groups_from_parent(struct ged *gedp,
					      bsg_scene_ref parent_ref,
					      struct bsg_view *v,
					      int mode);
static int _sg_erase_shapes_by_path(struct ged *gedp,
				    const struct db_full_path *subpath,
				    bsg_scene_ref prune_base_ref,
				    struct bsg_view *v,
				    int mode);
static int _sg_erase_by_path_prefix_string(struct ged *gedp,
					   const char *path,
					   bsg_scene_ref base_ref,
					   struct bsg_view *v,
					   int mode);
static void _draw_append_scene_ref_to_group(struct ged *gedp, bsg_scene_ref group_ref, bsg_scene_ref shape_ref);


static bsg_scene_ref
_sg_find_or_create_child_group(struct ged *gedp, bsg_scene_ref parent_ref,
			       const char *comp_name)
{
    if (!gedp || bsg_scene_ref_is_null(parent_ref) || !comp_name)
	return bsg_scene_ref_null();

    bsg_scene_ref existing = bsg_scene_group_find_child(parent_ref, comp_name);
    if (!bsg_scene_ref_is_null(existing))
	return existing;

    struct bsg_view *v = gedp->ged_gvp;
    if (!v)
	return bsg_scene_ref_null();

    struct directory *dp = RT_DIR_NULL;
    if (gedp->dbip)
	dp = db_lookup(gedp->dbip, comp_name, LOOKUP_QUIET);

    bsg_scene_ref child_ref =
	bsg_scene_group_ensure_child(parent_ref, v, comp_name, (void *)dp);
    if (bsg_scene_ref_is_null(child_ref))
	return bsg_scene_ref_null();

    bsg_scene_bump_rev(parent_ref);
    return child_ref;
}


static bsg_scene_ref
_sg_add_path(struct ged *gedp, const char *name)
{
    if (!gedp || !name || !gedp->dbip)
	return bsg_scene_ref_null();

    bsg_scene_ref root_ref = ged_draw_ensure_root(gedp);
    if (bsg_scene_ref_is_null(root_ref))
	return bsg_scene_ref_null();

    bsg_scene_ref base_ref = root_ref;
    if (gedp->ged_gvp && bsg_view_is_independent(gedp->ged_gvp)) {
	bsg_scene_ref scope_ref = bsg_view_independent_scope_ref(gedp->ged_gvp, 1);
	if (!bsg_scene_ref_is_null(scope_ref))
	    base_ref = scope_ref;
    }

    struct db_full_path pathcomp;
    if (db_string_to_path(&pathcomp, gedp->dbip, name) != 0) {
	const char *cp = strrchr(name, '/');
	cp = cp ? cp + 1 : name;
	struct directory *dp = db_lookup(gedp->dbip, cp, LOOKUP_NOISY);
	if (dp == RT_DIR_NULL)
	    return bsg_scene_ref_null();
	return _sg_find_or_create_child_group(gedp, base_ref, cp);
    }

    if (pathcomp.fp_len == 0) {
	db_free_full_path(&pathcomp);
	return bsg_scene_ref_null();
    }

    bsg_scene_ref cur_ref = base_ref;
    for (size_t i = 0; i < pathcomp.fp_len; i++) {
	const char *comp = pathcomp.fp_names[i]->d_namep;
	bsg_scene_ref child_ref =
	    _sg_find_or_create_child_group(gedp, cur_ref, comp);
	if (bsg_scene_ref_is_null(child_ref)) {
	    db_free_full_path(&pathcomp);
	    return bsg_scene_ref_null();
	}
	cur_ref = child_ref;
    }

    db_free_full_path(&pathcomp);
    return cur_ref;
}


static int
_sg_path_match_cb(bsg_scene_ref shape_ref, void *match_ctx)
{
    if (bsg_scene_ref_is_null(shape_ref) || !match_ctx)
	return 0;

    ged_draw_shape_state *bd = ged_draw_shape_state_get_scene_ref(shape_ref);
    if (!bd)
	return 0;

    struct db_full_path *subpath = (struct db_full_path *)match_ctx;
    return db_full_path_match_top(subpath, &bd->s_fullpath);
}


static void
_sg_erase_nested_subpath(bsg_scene_ref parent_ref,
			 struct db_full_path *subpath, size_t depth_start)
{
    if (bsg_scene_ref_is_null(parent_ref) || !subpath ||
	    subpath->fp_len == 0 || depth_start >= subpath->fp_len)
	return;

    const char **names = (const char **)bu_malloc(
	sizeof(const char *) * subpath->fp_len, "subpath names");
    for (size_t i = 0; i < subpath->fp_len; i++)
	names[i] = subpath->fp_names[i]->d_namep;

    bsg_scene_erase_nested_subpath(parent_ref, names, subpath->fp_len,
	    depth_start, _sg_path_match_cb, (void *)subpath);

    bu_free(names, "subpath names");
}


static void
_sg_erase_path(struct ged *gedp, const char *path)
{
    bsg_scene_ref root_ref = ged_scene_root_ref(gedp);
    if (bsg_scene_ref_is_null(root_ref))
	return;

    struct db_i *dbip = gedp->dbip;
    struct db_full_path subpath;
    int found_subpath = (db_string_to_path(&subpath, dbip, path) == 0);
    uint64_t erase_rev0 = gedp->i->ged_gdp->gd_draw_rev;

    struct _sg_ref_snapshot snap = {NULL, 0, 0};
    _sg_snapshot_children(&snap, root_ref, 1, 1);

    for (size_t gi = 0; gi < snap.len; gi++) {
	bsg_scene_ref group_ref = snap.refs[gi];
	const char *gpath = ged_draw_group_scene_ref_path(group_ref);
	if (!gpath)
	    continue;

	if (BU_STR_EQUAL(path, gpath)) {
	    bsg_scene_free_group(group_ref);
	    break;
	}

	if (!found_subpath)
	    continue;

	struct db_full_path gdlpath;
	if (db_string_to_path(&gdlpath, dbip, gpath) != 0)
	    continue;

	int is_ancestor = db_full_path_match_top(&gdlpath, &subpath);
	size_t ancestor_depth = gdlpath.fp_len;
	db_free_full_path(&gdlpath);

	if (is_ancestor && ancestor_depth < subpath.fp_len) {
	    _sg_erase_nested_subpath(group_ref, &subpath, ancestor_depth);
	    if (bsg_scene_child_count(group_ref) == 0)
		bsg_scene_free_group(group_ref);
	    break;
	}
    }

    _sg_ref_snapshot_free(&snap);
    if (found_subpath) {
	if (gedp->i->ged_gdp->gd_draw_rev == erase_rev0)
	    (void)_sg_erase_shapes_by_path(gedp, &subpath, root_ref, NULL, -1);
	db_free_full_path(&subpath);
    }
}


static bsg_scene_ref
_sg_scoped_erase_base_ref(struct ged *gedp, struct bsg_view *v)
{
    if (!gedp)
	return bsg_scene_ref_null();

    if (v && bsg_view_is_independent(v))
	return bsg_view_independent_scope_ref(v, 0);

    return ged_scene_root_ref(gedp);
}


static int
_sg_group_matches_erase_scope(struct ged *gedp,
			      bsg_scene_ref group_ref,
			      struct bsg_view *v,
			      int mode)
{
    if (!gedp || bsg_scene_ref_is_null(group_ref))
	return 0;

    struct ged_draw_group_record rec;
    if (!ged_draw_group_record_get(gedp,
	    ged_draw_group_ref_from_scene_ref(gedp, group_ref), &rec))
	return 0;
    if (rec.is_overlay)
	return 0;
    if (!ged_draw_group_record_in_view(&rec, v))
	return 0;
    if (mode >= 0 && (int)rec.draw_mode != mode)
	return 0;
    return 1;
}


static void
_sg_erase_path_scoped(struct ged *gedp, const char *path,
		      struct bsg_view *v, int mode)
{
    bsg_scene_ref base_ref = _sg_scoped_erase_base_ref(gedp, v);
    if (bsg_scene_ref_is_null(base_ref))
	return;

    struct db_i *dbip = gedp->dbip;
    struct db_full_path subpath;
    int found_subpath = (db_string_to_path(&subpath, dbip, path) == 0);
    uint64_t erase_rev0 = gedp->i->ged_gdp->gd_draw_rev;

    struct _sg_ref_snapshot snap = {NULL, 0, 0};
    _sg_snapshot_children(&snap, base_ref, 1, 1);

    for (size_t gi = 0; gi < snap.len; gi++) {
	bsg_scene_ref group_ref = snap.refs[gi];
	if (!_sg_group_matches_erase_scope(gedp, group_ref, v, mode))
	    continue;

	const char *gpath = ged_draw_group_scene_ref_path(group_ref);
	if (!gpath)
	    continue;

	if (BU_STR_EQUAL(path, gpath)) {
	    bsg_scene_free_group(group_ref);
	    break;
	}

	if (!found_subpath)
	    continue;

	struct db_full_path gdlpath;
	if (db_string_to_path(&gdlpath, dbip, gpath) != 0)
	    continue;

	int is_ancestor = db_full_path_match_top(&gdlpath, &subpath);
	size_t ancestor_depth = gdlpath.fp_len;
	db_free_full_path(&gdlpath);

	if (is_ancestor && ancestor_depth < subpath.fp_len) {
	    _sg_erase_nested_subpath(group_ref, &subpath, ancestor_depth);
	    if (bsg_scene_child_count(group_ref) == 0)
		bsg_scene_free_group(group_ref);
	    break;
	}
    }

    _sg_ref_snapshot_free(&snap);
    if (found_subpath) {
	if (gedp->i->ged_gdp->gd_draw_rev == erase_rev0)
	    (void)_sg_erase_shapes_by_path(gedp, &subpath, base_ref, v, mode);
	db_free_full_path(&subpath);
    }
}


static void
_sg_erase_subgroups_by_name(bsg_scene_ref parent_ref, const char *name)
{
    if (bsg_scene_ref_is_null(parent_ref) || !name)
	return;

    struct _sg_ref_snapshot snap = {NULL, 0, 0};
    _sg_snapshot_children(&snap, parent_ref, 1, 0);

    for (size_t i = 0; i < snap.len; i++) {
	bsg_scene_ref child_ref = snap.refs[i];
	if (BU_STR_EQUAL(bsg_scene_name(child_ref), name)) {
	    bsg_scene_free_group(child_ref);
	} else {
	    _sg_erase_subgroups_by_name(child_ref, name);
	}
    }

    _sg_ref_snapshot_free(&snap);
}


static void
_sg_erase_all_names(struct ged *gedp, const char *name)
{
    bsg_scene_ref root_ref = ged_scene_root_ref(gedp);
    if (bsg_scene_ref_is_null(root_ref))
	return;

    ged_draw_overlay_erase_name(gedp, name);

    struct _sg_ref_snapshot snap = {NULL, 0, 0};
    _sg_snapshot_children(&snap, root_ref, 1, 1);

    for (size_t gi = 0; gi < snap.len; gi++) {
	bsg_scene_ref group_ref = snap.refs[gi];
	const char *gpath = ged_draw_group_scene_ref_path(group_ref);
	if (!gpath)
	    continue;

	char *dup_path = bu_strdup(gpath);
	char *tok = strtok(dup_path, "/");
	int found = 0;
	while (tok) {
	    if (BU_STR_EQUAL(tok, name)) {
		bsg_scene_free_group(group_ref);
		found = 1;
		break;
	    }
	    tok = strtok(NULL, "/");
	}
	bu_free(dup_path, "_sg_erase_all_names dup");

	if (!found)
	    _sg_erase_subgroups_by_name(group_ref, name);
    }

    _sg_ref_snapshot_free(&snap);
}


static void
_sg_erase_all_paths(struct ged *gedp, const char *path)
{
    bsg_scene_ref root_ref = ged_scene_root_ref(gedp);
    if (bsg_scene_ref_is_null(root_ref))
	return;

    if (_sg_erase_by_path_prefix_string(gedp, path, root_ref, NULL, -1))
	return;

    if (ged_db_index_available(gedp) &&
	    !ged_db_index_path_resolve(gedp, path, NULL, 0))
	return;

    struct db_i *dbip = gedp->dbip;
    struct db_full_path subpath;
    if (db_string_to_path(&subpath, dbip, path) != 0)
	return;
    uint64_t erase_rev0 = gedp->i->ged_gdp->gd_draw_rev;

    int restart;
    do {
	restart = 0;
	struct _sg_ref_snapshot snap = {NULL, 0, 0};
	_sg_snapshot_children(&snap, root_ref, 1, 0);

	for (size_t i = 0; i < snap.len; i++) {
	    bsg_scene_ref group_ref = snap.refs[i];
	    const char *gpath = ged_draw_group_scene_ref_path(group_ref);
	    if (!gpath)
		continue;

	    struct db_full_path fullpath;
	    if (db_string_to_path(&fullpath, dbip, gpath) != 0)
		continue;

	    if (db_full_path_subset(&fullpath, &subpath, 0)) {
		db_free_full_path(&fullpath);
		bsg_scene_free_group(group_ref);
		restart = 1;
		break;
	    }

	    if (db_full_path_match_top(&fullpath, &subpath) &&
		    fullpath.fp_len < subpath.fp_len) {
		size_t depth = fullpath.fp_len;
		uint64_t rev0 = gedp->i->ged_gdp->gd_draw_rev;
		db_free_full_path(&fullpath);
		_sg_erase_nested_subpath(group_ref, &subpath, depth);
		if (bsg_scene_child_count(group_ref) == 0) {
		    bsg_scene_free_group(group_ref);
		    restart = 1;
		    break;
		}
		if (gedp->i->ged_gdp->gd_draw_rev != rev0) {
		    restart = 1;
		    break;
		}
		continue;
	    }

	    db_free_full_path(&fullpath);
	}

	_sg_ref_snapshot_free(&snap);
    } while (restart);

    if (gedp->i->ged_gdp->gd_draw_rev == erase_rev0)
	(void)_sg_erase_shapes_by_path(gedp, &subpath, root_ref, NULL, -1);

    db_free_full_path(&subpath);
}


static void
_sg_erase_all_paths_scoped(struct ged *gedp, const char *path,
			   struct bsg_view *v, int mode)
{
    bsg_scene_ref base_ref = _sg_scoped_erase_base_ref(gedp, v);
    if (bsg_scene_ref_is_null(base_ref))
	return;

    if (_sg_erase_by_path_prefix_string(gedp, path, base_ref, v, mode))
	return;

    if (ged_db_index_available(gedp) &&
	    !ged_db_index_path_resolve(gedp, path, NULL, 0))
	return;

    struct db_i *dbip = gedp->dbip;
    struct db_full_path subpath;
    if (db_string_to_path(&subpath, dbip, path) != 0)
	return;
    uint64_t erase_rev0 = gedp->i->ged_gdp->gd_draw_rev;

    int restart;
    do {
	restart = 0;
	struct _sg_ref_snapshot snap = {NULL, 0, 0};
	_sg_snapshot_children(&snap, base_ref, 1, 0);

	for (size_t i = 0; i < snap.len; i++) {
	    bsg_scene_ref group_ref = snap.refs[i];
	    if (!_sg_group_matches_erase_scope(gedp, group_ref, v, mode))
		continue;

	    const char *gpath = ged_draw_group_scene_ref_path(group_ref);
	    if (!gpath)
		continue;

	    struct db_full_path fullpath;
	    if (db_string_to_path(&fullpath, dbip, gpath) != 0)
		continue;

	    if (db_full_path_subset(&fullpath, &subpath, 0)) {
		db_free_full_path(&fullpath);
		bsg_scene_free_group(group_ref);
		restart = 1;
		break;
	    }

	    if (db_full_path_match_top(&fullpath, &subpath) &&
		    fullpath.fp_len < subpath.fp_len) {
		size_t depth = fullpath.fp_len;
		uint64_t rev0 = gedp->i->ged_gdp->gd_draw_rev;
		db_free_full_path(&fullpath);
		_sg_erase_nested_subpath(group_ref, &subpath, depth);
		if (bsg_scene_child_count(group_ref) == 0) {
		    bsg_scene_free_group(group_ref);
		    restart = 1;
		    break;
		}
		if (gedp->i->ged_gdp->gd_draw_rev != rev0) {
		    restart = 1;
		    break;
		}
		continue;
	    }

	    db_free_full_path(&fullpath);
	}

	_sg_ref_snapshot_free(&snap);
    } while (restart);

    if (gedp->i->ged_gdp->gd_draw_rev == erase_rev0)
	(void)_sg_erase_shapes_by_path(gedp, &subpath, base_ref, v, mode);

    db_free_full_path(&subpath);
}


static int
_sg_group_matches_optional_scope(struct ged *gedp,
				 bsg_scene_ref group_ref,
				 struct bsg_view *v,
				 int mode)
{
    if (!gedp || bsg_scene_ref_is_null(group_ref))
	return 0;

    struct ged_draw_group_record rec;
    if (!ged_draw_group_record_get(gedp,
	    ged_draw_group_ref_from_scene_ref(gedp, group_ref), &rec))
	return 0;
    if (rec.is_overlay)
	return 0;
    if (v && !ged_draw_group_record_in_view(&rec, v))
	return 0;
    if (mode >= 0 && (int)rec.draw_mode != mode)
	return 0;
    return 1;
}


static int
_sg_path_has_component(const char *path, const char *name, size_t first_idx)
{
    if (!path || !name)
	return 0;

    path = ged_draw_dbpath_skip_lead_slash(path);
    name = ged_draw_dbpath_skip_lead_slash(name);
    size_t namelen = strlen(name);
    if (!*path || !namelen)
	return 0;

    size_t idx = 0;
    const char *p = path;
    while (*p) {
	const char *slash = strchr(p, '/');
	size_t len = slash ? (size_t)(slash - p) : strlen(p);
	if (idx >= first_idx && len == namelen &&
		bu_strncmp(p, name, len) == 0)
	    return 1;
	if (!slash)
	    break;
	p = slash + 1;
	idx++;
    }

    return 0;
}


static int
_sg_fullpath_has_component(const struct db_full_path *fp,
			   const char *name,
			   size_t first_idx)
{
    if (!fp || !name)
	return 0;

    name = ged_draw_dbpath_skip_lead_slash(name);
    size_t namelen = strlen(name);
    if (!namelen)
	return 0;

    for (size_t i = first_idx; i < fp->fp_len; i++) {
	struct directory *dp = fp->fp_names[i];
	if (dp && strlen(dp->d_namep) == namelen &&
		bu_strncmp(dp->d_namep, name, namelen) == 0)
	    return 1;
    }

    return 0;
}


static size_t
_sg_path_norm_len(const char *path)
{
    if (!path)
	return 0;
    size_t len = strlen(path);
    while (len && path[len - 1] == '/')
	len--;
    return len;
}


static int
_sg_path_string_is_prefix(const char *prefix, const char *path)
{
    if (!prefix || !path)
	return 0;

    prefix = ged_draw_dbpath_skip_lead_slash(prefix);
    path = ged_draw_dbpath_skip_lead_slash(path);

    size_t plen = _sg_path_norm_len(prefix);
    size_t path_len = _sg_path_norm_len(path);
    if (!plen || path_len < plen)
	return 0;
    if (bu_strncmp(prefix, path, plen) != 0)
	return 0;
    return path_len == plen || path[plen] == '/';
}


struct _sg_erase_nonroot_component_ctx {
    struct ged *gedp;
    const char *name;
    struct bsg_view *view;
    int mode;
    int nonroot_only;
    struct bu_ptbl shape_refs;
};


struct _sg_erase_path_record_ctx {
    struct ged *gedp;
    const struct db_full_path *subpath;
    struct bsg_view *view;
    int mode;
    struct bu_ptbl shape_refs;
};


struct _sg_erase_path_string_record_ctx {
    struct ged *gedp;
    const char *path;
    struct bsg_view *view;
    int mode;
    struct bu_ptbl shape_refs;
};


static int
_sg_erase_path_shape_cb(const struct ged_draw_shape_record *rec,
			void *userdata)
{
    struct _sg_erase_path_record_ctx *ctx =
	(struct _sg_erase_path_record_ctx *)userdata;
    if (!ctx || !rec || !rec->fullpath || !ctx->subpath)
	return 1;
    if (ctx->mode >= 0 && (int)rec->draw_mode != ctx->mode)
	return 1;
    if (!db_full_path_match_top(ctx->subpath, rec->fullpath))
	return 1;

    struct ged_draw_group_record grec;
    if (!ged_draw_group_record_get(ctx->gedp, rec->group, &grec))
	return 1;
    if (grec.is_overlay)
	return 1;
    if (ctx->view && !ged_draw_group_record_in_view(&grec, ctx->view))
	return 1;

    ged_draw_shape_ref *ref = (ged_draw_shape_ref *)bu_malloc(
	    sizeof(ged_draw_shape_ref), "path erase shape ref");
    *ref = rec->ref;
    bu_ptbl_ins(&ctx->shape_refs, (long *)ref);
    return 1;
}


static int
_sg_erase_path_string_shape_cb(const struct ged_draw_shape_record *rec,
			       void *userdata)
{
    struct _sg_erase_path_string_record_ctx *ctx =
	(struct _sg_erase_path_string_record_ctx *)userdata;
    if (!ctx || !rec || !rec->fullpath || !ctx->path)
	return 1;
    if (ctx->mode >= 0 && (int)rec->draw_mode != ctx->mode)
	return 1;

    char *rec_path = db_path_to_string(rec->fullpath);
    if (!rec_path)
	return 1;
    int matches = _sg_path_string_is_prefix(ctx->path, rec_path);
    bu_free(rec_path, "draw erase record path string");
    if (!matches)
	return 1;

    struct ged_draw_group_record grec;
    if (!ged_draw_group_record_get(ctx->gedp, rec->group, &grec))
	return 1;
    if (grec.is_overlay)
	return 1;
    if (ctx->view && !ged_draw_group_record_in_view(&grec, ctx->view))
	return 1;

    ged_draw_shape_ref *ref = (ged_draw_shape_ref *)bu_malloc(
	    sizeof(ged_draw_shape_ref), "path string erase shape ref");
    *ref = rec->ref;
    bu_ptbl_ins(&ctx->shape_refs, (long *)ref);
    return 1;
}


static int
_sg_erase_shapes_by_path(struct ged *gedp,
			 const struct db_full_path *subpath,
			 bsg_scene_ref prune_base_ref,
			 struct bsg_view *v,
			 int mode)
{
    if (!gedp || !subpath)
	return 0;

    struct _sg_erase_path_record_ctx ctx;
    ctx.gedp = gedp;
    ctx.subpath = subpath;
    ctx.view = v;
    ctx.mode = mode;
    bu_ptbl_init(&ctx.shape_refs, 8, "path erase shape refs");

    ged_draw_foreach_shape_record(gedp, _sg_erase_path_shape_cb, &ctx);

    int released = 0;
    for (size_t i = 0; i < BU_PTBL_LEN(&ctx.shape_refs); i++) {
	ged_draw_shape_ref *ref =
	    (ged_draw_shape_ref *)BU_PTBL_GET(&ctx.shape_refs, i);
	if (ref) {
	    ref->scene_revision = 0;
	    if (ged_draw_shape_ref_release(gedp, *ref))
		released++;
	    bu_free(ref, "path erase shape ref");
	}
    }
    bu_ptbl_free(&ctx.shape_refs);

    if (released)
	(void)_sg_prune_empty_groups_from_parent(gedp, prune_base_ref, v, mode);

    return released;
}


static int
_sg_erase_shapes_by_path_prefix_string(struct ged *gedp,
				       const char *path,
				       bsg_scene_ref prune_base_ref,
				       struct bsg_view *v,
				       int mode)
{
    if (!gedp || !path)
	return 0;

    struct _sg_erase_path_string_record_ctx ctx;
    ctx.gedp = gedp;
    ctx.path = ged_draw_dbpath_skip_lead_slash(path);
    ctx.view = v;
    ctx.mode = mode;
    bu_ptbl_init(&ctx.shape_refs, 8, "path string erase shape refs");

    ged_draw_foreach_shape_record(gedp, _sg_erase_path_string_shape_cb,
	    &ctx);

    int released = 0;
    for (size_t i = 0; i < BU_PTBL_LEN(&ctx.shape_refs); i++) {
	ged_draw_shape_ref *ref =
	    (ged_draw_shape_ref *)BU_PTBL_GET(&ctx.shape_refs, i);
	if (ref) {
	    ref->scene_revision = 0;
	    if (ged_draw_shape_ref_release(gedp, *ref))
		released++;
	    bu_free(ref, "path string erase shape ref");
	}
    }
    bu_ptbl_free(&ctx.shape_refs);

    if (released)
	(void)_sg_prune_empty_groups_from_parent(gedp, prune_base_ref, v, mode);

    return released;
}


static int
_sg_erase_nonroot_component_shape_cb(const struct ged_draw_shape_record *rec,
				     void *userdata)
{
    struct _sg_erase_nonroot_component_ctx *ctx =
	(struct _sg_erase_nonroot_component_ctx *)userdata;
    if (!ctx || !rec || !rec->fullpath)
	return 1;
    if (ctx->mode >= 0 && (int)rec->draw_mode != ctx->mode)
	return 1;

    struct ged_draw_group_record grec;
    if (!ged_draw_group_record_get(ctx->gedp, rec->group, &grec))
	return 1;
    if (grec.is_overlay)
	return 1;
    if (ctx->view && !ged_draw_group_record_in_view(&grec, ctx->view))
	return 1;
    size_t first_idx = ctx->nonroot_only ? 1 : 0;
    if (rec->display_name &&
	    !_sg_path_has_component(rec->display_name, ctx->name, first_idx))
	return 1;
    if (!rec->display_name &&
	    !_sg_fullpath_has_component(rec->fullpath, ctx->name, first_idx))
	return 1;

    ged_draw_shape_ref *ref = (ged_draw_shape_ref *)bu_malloc(
	    sizeof(ged_draw_shape_ref), "nonroot component shape ref");
    *ref = rec->ref;
    bu_ptbl_ins(&ctx->shape_refs, (long *)ref);
    return 1;
}


static int
_sg_prune_empty_groups_from_parent(struct ged *gedp,
				   bsg_scene_ref parent_ref,
				   struct bsg_view *v,
				   int mode)
{
    if (!gedp || bsg_scene_ref_is_null(parent_ref))
	return 0;

    int pruned = 0;
    struct _sg_ref_snapshot snap = {NULL, 0, 0};
    _sg_snapshot_children(&snap, parent_ref, 1, 1);

    for (size_t i = 0; i < snap.len; i++) {
	bsg_scene_ref group_ref = snap.refs[i];
	pruned += _sg_prune_empty_groups_from_parent(gedp, group_ref, v, mode);
	if (bsg_scene_child_count(group_ref) == 0 &&
		_sg_group_matches_optional_scope(gedp, group_ref, v, mode)) {
	    bsg_scene_free_group(group_ref);
	    pruned++;
	}
    }

    _sg_ref_snapshot_free(&snap);
    return pruned;
}


static int
_sg_erase_groups_by_path_prefix_string(struct ged *gedp,
				       const char *path,
				       bsg_scene_ref base_ref,
				       struct bsg_view *v,
				       int mode)
{
    if (!gedp || !path || bsg_scene_ref_is_null(base_ref))
	return 0;

    int erased = 0;
    int restart;
    do {
	restart = 0;
	struct _sg_ref_snapshot snap = {NULL, 0, 0};
	_sg_snapshot_children(&snap, base_ref, 1, 1);

	for (size_t i = 0; i < snap.len; i++) {
	    bsg_scene_ref group_ref = snap.refs[i];
	    if (!_sg_group_matches_optional_scope(gedp, group_ref, v, mode))
		continue;

	    const char *gpath = ged_draw_group_scene_ref_path(group_ref);
	    if (!gpath)
		continue;

	    if (_sg_path_string_is_prefix(path, gpath)) {
		bsg_scene_free_group(group_ref);
		erased++;
		restart = 1;
		break;
	    }
	}

	_sg_ref_snapshot_free(&snap);
    } while (restart);

    return erased;
}


static int
_sg_erase_by_path_prefix_string(struct ged *gedp,
				const char *path,
				bsg_scene_ref base_ref,
				struct bsg_view *v,
				int mode)
{
    if (!gedp || !path || bsg_scene_ref_is_null(base_ref))
	return 0;

    int ret = _sg_erase_groups_by_path_prefix_string(gedp, path, base_ref,
	    v, mode);
    ret += _sg_erase_shapes_by_path_prefix_string(gedp, path, base_ref,
	    v, mode);
    return ret;
}


static int
_sg_erase_component_scoped_impl(struct ged *gedp,
				const char *name,
				struct bsg_view *v,
				int mode,
				int nonroot_only)
{
    bsg_scene_ref base_ref = _sg_scoped_erase_base_ref(gedp, v);
    if (bsg_scene_ref_is_null(base_ref))
	return 0;

    if (!nonroot_only)
	ged_draw_overlay_erase_name(gedp, name);

    struct _sg_erase_nonroot_component_ctx ctx;
    ctx.gedp = gedp;
    ctx.name = ged_draw_dbpath_skip_lead_slash(name);
    ctx.view = v;
    ctx.mode = mode;
    ctx.nonroot_only = nonroot_only ? 1 : 0;
    bu_ptbl_init(&ctx.shape_refs, 8, "nonroot component shape refs");

    ged_draw_foreach_shape_record(gedp, _sg_erase_nonroot_component_shape_cb,
	    &ctx);

    int released = 0;
    for (size_t i = 0; i < BU_PTBL_LEN(&ctx.shape_refs); i++) {
	ged_draw_shape_ref *ref =
	    (ged_draw_shape_ref *)BU_PTBL_GET(&ctx.shape_refs, i);
	if (ref) {
	    ref->scene_revision = 0;
	    if (ged_draw_shape_ref_release(gedp, *ref))
		released++;
	    bu_free(ref, "nonroot component shape ref");
	}
    }
    bu_ptbl_free(&ctx.shape_refs);

    if (released)
	(void)_sg_prune_empty_groups_from_parent(gedp, base_ref, v, mode);

    return released;
}


static int
_sg_erase_nonroot_component_scoped(struct ged *gedp,
				   const char *name,
				   struct bsg_view *v,
				   int mode)
{
    return _sg_erase_component_scoped_impl(gedp, name, v, mode, 1);
}


static int
_sg_erase_component_scoped(struct ged *gedp,
			   const char *name,
			   struct bsg_view *v,
			   int mode)
{
    return _sg_erase_component_scoped_impl(gedp, name, v, mode, 0);
}


static int
_sg_bounding_sph(struct ged *gedp, vect_t *min, vect_t *max, int pflag)
{
    bsg_scene_ref root_ref = ged_scene_root_ref(gedp);
    if (bsg_scene_ref_is_null(root_ref))
	return 1;

    return bsg_scene_subtree_bbox(root_ref, min, max, pflag);
}


void
ged_draw_erase_name(struct ged *gedp, const char *name)
{
    if (!gedp || !name)
	return;
    _sg_erase_all_names(gedp, name);
}


int
ged_draw_erase_path_string(struct ged *gedp, const char *path)
{
    if (!gedp || !path)
	return 0;
    uint64_t rev0 = gedp->i->ged_gdp->gd_draw_rev;
    _sg_erase_path(gedp, ged_draw_dbpath_skip_lead_slash(path));
    return (gedp->i->ged_gdp->gd_draw_rev != rev0) ? 1 : 0;
}


int
ged_draw_erase_path_prefix_string(struct ged *gedp, const char *path)
{
    if (!gedp || !path)
	return 0;
    uint64_t rev0 = gedp->i->ged_gdp->gd_draw_rev;
    _sg_erase_all_paths(gedp, ged_draw_dbpath_skip_lead_slash(path));
    return (gedp->i->ged_gdp->gd_draw_rev != rev0) ? 1 : 0;
}


int
ged_draw_erase_path_string_scoped(struct ged *gedp,
				  const char *path,
				  struct bsg_view *view,
				  int mode)
{
    if (!gedp || !path)
	return 0;
    uint64_t rev0 = gedp->i->ged_gdp->gd_draw_rev;
    _sg_erase_path_scoped(gedp, ged_draw_dbpath_skip_lead_slash(path), view,
	    mode);
    return (gedp->i->ged_gdp->gd_draw_rev != rev0) ? 1 : 0;
}


int
ged_draw_erase_path_prefix_string_scoped(struct ged *gedp,
					 const char *path,
					 struct bsg_view *view,
					 int mode)
{
    if (!gedp || !path)
	return 0;
    uint64_t rev0 = gedp->i->ged_gdp->gd_draw_rev;
    _sg_erase_all_paths_scoped(gedp, ged_draw_dbpath_skip_lead_slash(path),
	    view, mode);
    return (gedp->i->ged_gdp->gd_draw_rev != rev0) ? 1 : 0;
}


int
ged_draw_erase_nonroot_component_string_scoped(struct ged *gedp,
					       const char *name,
					       struct bsg_view *view,
					       int mode)
{
    if (!gedp || !name)
	return 0;
    return _sg_erase_nonroot_component_scoped(gedp, name, view, mode);
}


int
ged_draw_erase_component_string_scoped(struct ged *gedp,
				       const char *name,
				       struct bsg_view *view,
				       int mode)
{
    if (!gedp || !name)
	return 0;
    return _sg_erase_component_scoped(gedp, name, view, mode);
}


int
ged_draw_apply_erase_path(struct ged *gedp,
			  const struct db_full_path *dfp)
{
    if (!gedp || !dfp)
	return 0;

    char *s = db_path_to_string(dfp);
    if (!s)
	return 0;

    struct ged_draw_transaction txn =
	ged_draw_transaction_make(GED_DRAW_TXN_ERASE,
				  ged_draw_dbpath_skip_lead_slash(s));
    int ret = ged_draw_apply_transaction(gedp, &txn, NULL);
    bu_free(s, "ged_draw_apply_erase_path: path string");
    return ret;
}


int
ged_draw_apply_erase_path_prefix(struct ged *gedp,
				 const struct db_full_path *dfp)
{
    if (!gedp || !dfp)
	return 0;

    char *s = db_path_to_string(dfp);
    if (!s)
	return 0;

    struct ged_draw_transaction txn =
	ged_draw_transaction_make(GED_DRAW_TXN_ERASE_PREFIX,
				  ged_draw_dbpath_skip_lead_slash(s));
    int ret = ged_draw_apply_transaction(gedp, &txn, NULL);
    bu_free(s, "ged_draw_apply_erase_path_prefix: path string");
    return ret;
}


static bsg_scene_ref
_draw_group_lookup_or_create_ref(struct ged *gedp,
				 const struct db_full_path *dfp)
{
    if (!gedp || !dfp)
	return bsg_scene_ref_null();

    char *s = db_path_to_string(dfp);
    if (!s)
	return bsg_scene_ref_null();

    const char *path = ged_draw_dbpath_skip_lead_slash(s);
    bsg_scene_ref group_ref = _sg_add_path(gedp, path);
    if (!bsg_scene_ref_is_null(group_ref) &&
	    !bsg_scene_draw_intent(group_ref)) {
	bsg_scene_set_draw_intent(group_ref,
		bsg_draw_intent_create(path, BSG_DRAW_MODE_WIRE));
    }

    if (!bsg_scene_ref_is_null(group_ref))
	(void)ged_draw_scene_ref_set_fullpath(gedp, group_ref, dfp);

    bu_free(s, "draw group path string");
    return group_ref;
}


ged_draw_group_ref
ged_draw_group_ref_lookup_or_create(struct ged *gedp,
				    const struct db_full_path *dfp)
{
    bsg_scene_ref group_ref = _draw_group_lookup_or_create_ref(gedp, dfp);
    if (bsg_scene_ref_is_null(group_ref))
	return GED_DRAW_GROUP_REF_NULL;

    return ged_draw_group_ref_from_scene_ref(gedp, group_ref);
}


void
ged_draw_erase_path(struct ged *gedp,
		    const struct db_full_path *dfp)
{
    if (!gedp || !dfp)
	return;

    char *s = db_path_to_string(dfp);
    if (!s)
	return;

    struct ged_draw_transaction txn =
	ged_draw_transaction_make(GED_DRAW_TXN_ERASE,
				  ged_draw_dbpath_skip_lead_slash(s));
    ged_draw_apply_transaction(gedp, &txn, NULL);
    bu_free(s, "ged_draw_erase_path: path string");
}


void
ged_draw_erase_path_prefix(struct ged *gedp,
			   const struct db_full_path *dfp)
{
    if (!gedp || !dfp)
	return;

    char *s = db_path_to_string(dfp);
    if (!s)
	return;

    struct ged_draw_transaction txn =
	ged_draw_transaction_make(GED_DRAW_TXN_ERASE_PREFIX,
				  ged_draw_dbpath_skip_lead_slash(s));
    ged_draw_apply_transaction(gedp, &txn, NULL);
    bu_free(s, "ged_draw_erase_path_prefix: path string");
}


int
ged_draw_bounds(struct ged *gedp, vect_t *min, vect_t *max, int pflag)
{
    if (!gedp || !min || !max)
	return 1;
    return _sg_bounding_sph(gedp, min, max, pflag);
}


void
ged_draw_append_scene_ref_to_last_group(struct ged *gedp, bsg_scene_ref shape_ref)
{
    if (!gedp || bsg_scene_ref_is_null(shape_ref))
	return;

    bsg_scene_ref root_ref = ged_scene_root_ref(gedp);
    if (bsg_scene_ref_is_null(root_ref))
	return;

    size_t child_count = bsg_scene_child_count(root_ref);
    if (child_count == 0)
	return;

    bsg_scene_ref group_ref = bsg_scene_child_at(root_ref, child_count - 1);
    _draw_append_scene_ref_to_group(gedp, group_ref, shape_ref);
}


static int
_ged_draw_group_scene_ref_set_dbpath(struct ged *gedp,
				     bsg_scene_ref group_ref,
				     const struct db_full_path *new_dfp)
{
    if (!gedp || bsg_scene_ref_is_null(group_ref) || !new_dfp)
	return 0;

    char *s = db_path_to_string(new_dfp);
    if (!s)
	return 0;

    const char *path = ged_draw_dbpath_skip_lead_slash(s);
    bsg_scene_set_name(group_ref, path);

    struct bsg_draw_intent *di = bsg_scene_draw_intent(group_ref);
    if (di) {
	bsg_draw_intent_set_path(di, path);
    } else {
	bsg_draw_mode mode = ged_draw_group_scene_ref_mode(group_ref);
	bsg_scene_set_draw_intent(group_ref,
		bsg_draw_intent_create(path, mode));
    }

    (void)ged_draw_scene_ref_set_fullpath(gedp, group_ref, new_dfp);
    bu_free(s, "ged_draw_group_scene_ref_set_dbpath: path string");
    return 1;
}


int
ged_draw_group_ref_set_dbpath(struct ged *gedp,
			      ged_draw_group_ref ref,
			      const struct db_full_path *new_dfp)
{
    bsg_scene_ref group_ref = ged_draw_registry_group_scene_ref(gedp, ref);
    return _ged_draw_group_scene_ref_set_dbpath(gedp, group_ref, new_dfp);
}


int
ged_draw_group_ref_set_mode(struct ged *gedp,
			    ged_draw_group_ref ref,
			    bsg_draw_mode mode)
{
    bsg_scene_ref group_ref = ged_draw_registry_group_scene_ref(gedp, ref);
    if (bsg_scene_ref_is_null(group_ref))
	return 0;

    struct bsg_draw_intent *di = bsg_scene_draw_intent(group_ref);
    if (di) {
	bsg_draw_intent_set_mode(di, mode);
	return 1;
    }

    const char *path = ged_draw_group_scene_ref_path(group_ref);
    if (!path)
	path = bsg_scene_name(group_ref);
    bsg_scene_set_draw_intent(group_ref,
	    bsg_draw_intent_create(path, mode));
    return 1;
}


int
ged_draw_group_ref_set_appearance(struct ged *gedp,
				  ged_draw_group_ref ref,
				  const struct bsg_appearance_settings *settings)
{
    if (!settings)
	return 0;

    bsg_scene_ref group_ref = ged_draw_registry_group_scene_ref(gedp, ref);
    if (bsg_scene_ref_is_null(group_ref))
	return 0;

    struct bsg_draw_intent *di = bsg_scene_draw_intent(group_ref);
    if (!di) {
	const char *path = bsg_scene_name(group_ref);
	bsg_scene_set_draw_intent(group_ref,
		bsg_draw_intent_create(path,
		    (bsg_draw_mode)settings->draw_mode));
	di = bsg_scene_draw_intent(group_ref);
    }

    if (!di)
	return 0;

    bsg_draw_intent_set_appearance(di, settings);
    return 1;
}


int
ged_draw_group_scene_ref_dbpath(struct ged *gedp,
				bsg_scene_ref group_ref,
				struct db_full_path *out)
{
    if (!gedp || bsg_scene_ref_is_null(group_ref) || !out || !gedp->dbip)
	return -1;
    if (ged_draw_group_scene_ref_is_overlay(group_ref))
	return -1;
    const struct db_full_path *stored = ged_draw_scene_ref_fullpath(group_ref);
    if (!stored || stored->fp_len <= 0)
	return -1;
    db_dup_full_path(out, stored);
    return 0;
}


int
ged_draw_group_scene_ref_is_overlay(bsg_scene_ref group_ref)
{
    const struct bsg_draw_intent *di = bsg_scene_draw_intent(group_ref);
    return bsg_draw_intent_is_overlay(di);
}


static int
_sg_clear_db_groups_in_scope(struct ged *gedp, struct bsg_view *v)
{
    bsg_scene_ref base_ref = _sg_scoped_erase_base_ref(gedp, v);
    if (bsg_scene_ref_is_null(base_ref))
	return 0;

    struct _sg_ref_snapshot snap = {NULL, 0, 0};
    _sg_snapshot_children(&snap, base_ref, 1, 0);

    int cleared = 0;
    for (size_t gi = 0; gi < snap.len; gi++) {
	bsg_scene_ref group_ref = snap.refs[gi];
	if (!_sg_group_matches_erase_scope(gedp, group_ref, v, -1))
	    continue;
	bsg_scene_free_group(group_ref);
	cleared++;
    }

    _sg_ref_snapshot_free(&snap);
    return cleared;
}


static int
_sg_clear_all_groups_under(bsg_scene_ref base_ref)
{
    if (bsg_scene_ref_is_null(base_ref))
	return 0;

    struct _sg_ref_snapshot snap = {NULL, 0, 0};
    _sg_snapshot_children(&snap, base_ref, 0, 0);

    int cleared = 0;
    for (size_t gi = 0; gi < snap.len; gi++) {
	bsg_scene_ref group_ref = snap.refs[gi];
	bsg_scene_free_group_contents(group_ref);
	bsg_scene_remove_child(base_ref, group_ref);
	ged_draw_scene_ref_release(group_ref);
	cleared++;
    }

    _sg_ref_snapshot_free(&snap);
    return cleared;
}


void
ged_draw_clear(struct ged *gedp)
{
    if (!gedp)
	return;

    struct bu_ptbl *views = bsg_set_views(&gedp->ged_views);
    if (views) {
	for (size_t i = 0; i < BU_PTBL_LEN(views); i++) {
	    struct bsg_view *v = (struct bsg_view *)BU_PTBL_GET(views, i);
	    if (!v || !bsg_view_is_independent(v))
		continue;
	    bsg_scene_ref scope_ref = bsg_view_independent_scope_ref(v, 0);
	    (void)_sg_clear_all_groups_under(scope_ref);
	}
    }

    bsg_scene_ref root_ref = ged_scene_root_ref(gedp);
    (void)_sg_clear_all_groups_under(root_ref);
    ged_draw_registry_free(gedp);

    gedp->i->ged_gdp->gd_draw_rev = 0;

    struct ged_drawable *gdp = gedp->i->ged_gdp;
    gdp->gd_highlight_token = 0;
    gdp->gd_highlight_scene_rev = 0;
    gdp->gd_highlight_rev++;
}


int
ged_draw_clear_view(struct ged *gedp, struct bsg_view *v)
{
    if (!gedp)
	return 0;

    return _sg_clear_db_groups_in_scope(gedp, v);
}


int
ged_draw_has_groups(struct ged *gedp)
{
    if (!gedp || !gedp->i || !gedp->i->ged_gdp)
	return 0;
    bsg_scene_ref root_ref = ged_scene_root_ref(gedp);
    if (bsg_scene_ref_is_null(root_ref))
	return 0;
    return (bsg_scene_child_count(root_ref) > 0) ? 1 : 0;
}


static void
_draw_append_scene_ref_to_group(struct ged *gedp,
				bsg_scene_ref group_ref,
				bsg_scene_ref shape_ref)
{
    if (bsg_scene_ref_is_null(group_ref) || bsg_scene_ref_is_null(shape_ref))
	return;

    ged_draw_shape_state *bdata = ged_draw_shape_state_get_scene_ref(shape_ref);
    int fp_len = bdata ? (int)bdata->s_fullpath.fp_len : 0;

    bsg_scene_ref attach_ref = ged_draw_shape_source_ref(shape_ref);
    if (bsg_scene_ref_is_null(attach_ref))
	attach_ref = shape_ref;

    if (!gedp || fp_len == 0) {
	bsg_scene_append_child(group_ref, attach_ref);
	bsg_scene_bbox_invalidate(group_ref);
	return;
    }

    int group_depth = bsg_scene_draw_tree_depth(group_ref);
    bsg_scene_ref cur_ref = group_ref;
    for (int d = group_depth; d < fp_len - 1; d++) {
	const char *comp = bdata->s_fullpath.fp_names[d]->d_namep;
	bsg_scene_ref child_ref =
	    _sg_find_or_create_child_group(gedp, cur_ref, comp);
	if (bsg_scene_ref_is_null(child_ref))
	    break;
	cur_ref = child_ref;
    }

    bsg_scene_append_child(cur_ref, attach_ref);
    bsg_scene_bbox_invalidate(cur_ref);
}


int
ged_draw_group_ref_append_scene_ref(struct ged *gedp,
				    ged_draw_group_ref ref,
				    bsg_scene_ref shape_ref)
{
    /* Appending a single draw command's leaf shapes bumps the draw-scene
     * revision after the first insert.  Keep resolving this private mutation
     * handle by token for the rest of the batch; public record lookups still
     * use revision-stamped refs and reject stale handles. */
    ref.scene_revision = 0;
    bsg_scene_ref group_ref = ged_draw_registry_group_scene_ref(gedp, ref);
    if (bsg_scene_ref_is_null(group_ref) || bsg_scene_ref_is_null(shape_ref))
	return 0;

    _draw_append_scene_ref_to_group(gedp, group_ref, shape_ref);
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
