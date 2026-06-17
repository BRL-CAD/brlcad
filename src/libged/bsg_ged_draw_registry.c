/*             B S G _ G E D _ D R A W _ R E G I S T R Y . C
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
/** @file bsg_ged_draw_registry.c
 *
 * GED draw registry and draw-shape state compatibility bridge.
 *
 * This file owns the temporary libged registry used to map legacy GED draw
 * refs to retained BSG nodes.  The target architecture is semantic draw
 * records and typed refs, not node-backed registry lookup.
 */

#include "common.h"

#include <string.h>

#include "bu/hash.h"
#include "bu/malloc.h"
#include "bu/ptbl.h"
#include "bsg/draw_ctx.h"
#include "bsg/scene_builder.h"
#include "ged/bsg_ged_draw.h"
#include "./ged_private.h"
#include "./bsg_ged_draw_private.h"


struct ged_draw_registry_entry {
    uint32_t magic;
    uintptr_t token;
    int is_group;
    bsg_scene_ref scene_ref;
    ged_draw_shape_state data;
};

#define GED_DRAW_REGISTRY_ENTRY_MAGIC 0x67647265U /* gdre */

static struct bu_ptbl ged_draw_registry_entries = BU_PTBL_INIT_ZERO;

static struct ged_drawable *_ged_drawable_for_scene_ref(bsg_scene_ref ref);
static struct ged_drawable *_ged_draw_registry_entry_gdp(struct ged_draw_registry_entry *entry);
static void _ged_draw_registry_entry_index(struct ged_drawable *gdp,
					   struct ged_draw_registry_entry *entry);
static void _ged_draw_registry_entry_unindex(struct ged_drawable *gdp,
					     struct ged_draw_registry_entry *entry);
static void _ged_draw_registry_index_tables_free(struct ged_drawable *gdp);


static void
_ged_draw_registry_global_insert(struct ged_draw_registry_entry *entry)
{
    if (!entry)
	return;
    if (!BU_PTBL_IS_INITIALIZED(&ged_draw_registry_entries))
	BU_PTBL_INIT(&ged_draw_registry_entries);
    bu_ptbl_ins(&ged_draw_registry_entries, (long *)entry);
}


static void
_ged_draw_registry_global_remove_gdp(struct ged_drawable *gdp)
{
    if (!gdp || !BU_PTBL_IS_INITIALIZED(&ged_draw_registry_entries))
	return;

    size_t write = 0;
    for (size_t read = 0; read < BU_PTBL_LEN(&ged_draw_registry_entries); read++) {
	struct ged_draw_registry_entry *entry =
	    (struct ged_draw_registry_entry *)BU_PTBL_GET(&ged_draw_registry_entries, read);
	if (_ged_draw_registry_entry_gdp(entry) == gdp)
	    continue;
	BU_PTBL_SET(&ged_draw_registry_entries, write, entry);
	write++;
    }

    for (size_t i = write; i < BU_PTBL_LEN(&ged_draw_registry_entries); i++)
	BU_PTBL_SET(&ged_draw_registry_entries, i, NULL);
    ged_draw_registry_entries.end = write;

    if (BU_PTBL_LEN(&ged_draw_registry_entries) == 0) {
	bu_ptbl_free(&ged_draw_registry_entries);
	ged_draw_registry_entries = (struct bu_ptbl)BU_PTBL_INIT_ZERO;
    }
}


static struct ged_draw_registry_entry *
_ged_draw_registry_entry_for_scene_ref_global(bsg_scene_ref ref)
{
    if (bsg_scene_ref_is_null(ref) || !BU_PTBL_IS_INITIALIZED(&ged_draw_registry_entries))
	return NULL;
    for (size_t i = 0; i < BU_PTBL_LEN(&ged_draw_registry_entries); i++) {
	struct ged_draw_registry_entry *entry =
	    (struct ged_draw_registry_entry *)BU_PTBL_GET(&ged_draw_registry_entries, i);
	if (entry && bsg_scene_ref_equal(entry->scene_ref, ref))
	    return entry;
    }
    return NULL;
}


static void
_ged_draw_shape_state_init(ged_draw_shape_state *data, struct ged *gedp)
{
    if (!data)
	return;
    memset(data, 0, sizeof(*data));
    db_full_path_init(&data->s_fullpath);
    data->leaf_dp = RT_DIR_NULL;
    data->region_id = -1;
    data->aircode = 0;
    data->los = 0;
    data->material_id = 0;
    data->gedp = gedp;
    data->u_data = NULL;
    data->u_data_kind = GED_DRAW_SHAPE_USER_DATA_NONE;
    data->source_ref = bsg_scene_ref_null();
    data->source_data = NULL;
    data->geometry_command_count = 0;
    data->geometry_revision = 0;
}


static void
_ged_draw_shape_state_free_contents(ged_draw_shape_state *data)
{
    if (!data)
	return;
    db_free_full_path(&data->s_fullpath);
    db_full_path_init(&data->s_fullpath);
    data->leaf_dp = RT_DIR_NULL;
    if (data->display_name) {
	bu_free(data->display_name, "ged draw shape display name");
	data->display_name = NULL;
    }
    if (data->source_data) {
	ged_draw_source_data_free(data->source_data);
	data->source_data = NULL;
    }
    if (data->u_data) {
	switch (data->u_data_kind) {
	    case GED_DRAW_SHAPE_USER_DATA_RT_DB_INTERNAL: {
		struct rt_db_internal *ip = (struct rt_db_internal *)data->u_data;
		rt_db_free_internal(ip);
		BU_PUT(ip, struct rt_db_internal);
		break;
	    }
	    case GED_DRAW_SHAPE_USER_DATA_NONE:
	    default:
		break;
	}
	data->u_data = NULL;
    }
    data->u_data_kind = GED_DRAW_SHAPE_USER_DATA_NONE;
    data->source_ref = bsg_scene_ref_null();
}


static void
_ged_draw_registry_entries_free(struct ged_drawable *gdp)
{
    if (!gdp || !gdp->gd_draw_registry_init)
	return;
    _ged_draw_registry_global_remove_gdp(gdp);
    for (size_t i = 0; i < BU_PTBL_LEN(&gdp->gd_draw_registry); i++) {
	struct ged_draw_registry_entry *entry =
	    (struct ged_draw_registry_entry *)BU_PTBL_GET(&gdp->gd_draw_registry, i);
	if (!entry)
	    continue;
	_ged_draw_shape_state_free_contents(&entry->data);
	bu_free(entry, "ged draw registry entry");
    }
    bu_ptbl_reset(&gdp->gd_draw_registry);
    gdp->gd_draw_next_token = 1;
}


void
ged_draw_registry_free(struct ged *gedp)
{
    if (!gedp || !gedp->i || !gedp->i->ged_gdp)
	return;
    struct ged_drawable *gdp = gedp->i->ged_gdp;
    if (!gdp->gd_draw_registry_init)
	return;
    _ged_draw_registry_index_tables_free(gdp);
    _ged_draw_registry_entries_free(gdp);
    bu_ptbl_free(&gdp->gd_draw_registry);
    gdp->gd_draw_registry_init = 0;
}


static void
_ged_draw_registry_init_if_needed(struct ged_drawable *gdp)
{
    if (!gdp || gdp->gd_draw_registry_init)
	return;
    BU_PTBL_INIT(&gdp->gd_draw_registry);
    gdp->gd_draw_registry_init = 1;
    gdp->gd_draw_next_token = 1;
}


static struct ged_drawable *
_ged_drawable_for_scene_ref(bsg_scene_ref ref)
{
    bsg_scene_ref root_ref = ref;
    bsg_scene_ref parent_ref = bsg_scene_parent(ref);
    while (!bsg_scene_ref_is_null(parent_ref)) {
	root_ref = parent_ref;
	parent_ref = bsg_scene_parent(root_ref);
    }
    struct bsg_draw_ctx *ctx = bsg_scene_draw_ctx(root_ref);
    return ctx ? (struct ged_drawable *)ctx->owner_data : NULL;
}


static struct ged_drawable *
_ged_draw_registry_entry_gdp(struct ged_draw_registry_entry *entry)
{
    if (!entry)
	return NULL;
    if (entry->data.gedp && entry->data.gedp->i &&
	    entry->data.gedp->i->ged_gdp)
	return entry->data.gedp->i->ged_gdp;
    return _ged_drawable_for_scene_ref(entry->scene_ref);
}


static const char *
_ged_draw_index_component_name(const char *path)
{
    if (!path)
	return NULL;
    while (*path == '/')
	path++;
    const char *basename = strrchr(path, '/');
    return basename ? basename + 1 : path;
}


static unsigned long long
_ged_draw_index_name_hash(const char *name)
{
    return name ? bu_data_hash(name, strlen(name) * sizeof(char)) : 0;
}


static struct bu_hash_tbl **
_ged_draw_component_index_tablep(struct ged_drawable *gdp, int is_group)
{
    if (!gdp)
	return NULL;
    return is_group ? &gdp->gd_draw_groups_by_component_hash :
	&gdp->gd_draw_shapes_by_component_hash;
}


static struct bu_hash_tbl **
_ged_draw_path_index_tablep(struct ged_drawable *gdp, int is_group)
{
    if (!gdp)
	return NULL;
    return is_group ? &gdp->gd_draw_groups_by_path_hash :
	&gdp->gd_draw_shapes_by_path_hash;
}


static struct bu_ptbl *
_ged_draw_index_bucket(struct bu_hash_tbl **tablep,
		       unsigned long long key,
		       int create)
{
    if (!tablep)
	return NULL;
    if (!*tablep) {
	if (!create)
	    return NULL;
	*tablep = bu_hash_create(0);
	if (!*tablep)
	    return NULL;
    }

    struct bu_ptbl *bucket = (struct bu_ptbl *)bu_hash_get(*tablep,
	    (const uint8_t *)&key, sizeof(key));
    if (!bucket && create) {
	bucket = (struct bu_ptbl *)bu_calloc(1, sizeof(*bucket),
		"ged draw reverse index bucket");
	bu_ptbl_init(bucket, 8, "ged draw reverse index bucket");
	if (bu_hash_set(*tablep, (const uint8_t *)&key, sizeof(key),
		    (void *)bucket) < 0) {
	    bu_ptbl_free(bucket);
	    bu_free(bucket, "ged draw reverse index bucket");
	    return NULL;
	}
    }

    return bucket;
}


static void
_ged_draw_index_bucket_remove_entry(struct bu_hash_tbl *table,
				    unsigned long long key,
				    struct ged_draw_registry_entry *entry)
{
    if (!table || !entry)
	return;

    struct bu_ptbl *bucket = (struct bu_ptbl *)bu_hash_get(table,
	    (const uint8_t *)&key, sizeof(key));
    if (!bucket)
	return;

    (void)bu_ptbl_rm(bucket, (const long *)entry);
    if (BU_PTBL_LEN(bucket) == 0) {
	bu_hash_rm(table, (const uint8_t *)&key, sizeof(key));
	bu_ptbl_free(bucket);
	bu_free(bucket, "ged draw reverse index bucket");
    }
}


static int
_ged_draw_entry_indexable(const struct ged_draw_registry_entry *entry,
			  int is_group)
{
    return entry && entry->is_group == (is_group ? 1 : 0) &&
	!bsg_scene_ref_is_null(entry->scene_ref) &&
	entry->data.s_fullpath.fp_len > 0;
}


static int
_ged_draw_hash_seen(const unsigned long long *hashes,
		    size_t count,
		    unsigned long long hash)
{
    if (!hashes)
	return 0;
    for (size_t i = 0; i < count; i++) {
	if (hashes[i] == hash)
	    return 1;
    }
    return 0;
}


static void
_ged_draw_registry_entry_index(struct ged_drawable *gdp,
			       struct ged_draw_registry_entry *entry)
{
    if (!gdp || !entry ||
	    !_ged_draw_entry_indexable(entry, entry->is_group))
	return;

    struct db_full_path *fp = &entry->data.s_fullpath;
    struct bu_hash_tbl **component_tablep =
	_ged_draw_component_index_tablep(gdp, entry->is_group);
    struct bu_hash_tbl **path_tablep =
	_ged_draw_path_index_tablep(gdp, entry->is_group);

    unsigned long long *seen_hashes =
	(unsigned long long *)bu_calloc(fp->fp_len, sizeof(unsigned long long),
		"ged draw index component hashes");
    size_t seen_count = 0;
    for (size_t i = 0; i < fp->fp_len; i++) {
	const struct directory *dp = fp->fp_names[i];
	const char *name = (dp && dp->d_namep) ? dp->d_namep : NULL;
	if (!name || !name[0])
	    continue;
	unsigned long long hash = _ged_draw_index_name_hash(name);
	if (_ged_draw_hash_seen(seen_hashes, seen_count, hash))
	    continue;
	seen_hashes[seen_count++] = hash;
	struct bu_ptbl *bucket =
	    _ged_draw_index_bucket(component_tablep, hash, 1);
	if (bucket)
	    bu_ptbl_ins_unique(bucket, (long *)entry);
    }
    bu_free(seen_hashes, "ged draw index component hashes");

    struct bu_ptbl *path_bucket =
	_ged_draw_index_bucket(path_tablep, entry->data.path_hash, 1);
    if (path_bucket)
	bu_ptbl_ins_unique(path_bucket, (long *)entry);
}


static void
_ged_draw_registry_entry_unindex(struct ged_drawable *gdp,
				 struct ged_draw_registry_entry *entry)
{
    if (!gdp || !entry)
	return;

    int is_group = entry->is_group ? 1 : 0;
    if (entry->data.s_fullpath.fp_len > 0) {
	struct db_full_path *fp = &entry->data.s_fullpath;
	struct bu_hash_tbl **component_tablep =
	    _ged_draw_component_index_tablep(gdp, is_group);
	if (component_tablep && *component_tablep) {
	    unsigned long long *seen_hashes =
		(unsigned long long *)bu_calloc(fp->fp_len,
			sizeof(unsigned long long),
			"ged draw index component hashes");
	    size_t seen_count = 0;
	    for (size_t i = 0; i < fp->fp_len; i++) {
		const struct directory *dp = fp->fp_names[i];
		const char *name = (dp && dp->d_namep) ? dp->d_namep : NULL;
		if (!name || !name[0])
		    continue;
		unsigned long long hash = _ged_draw_index_name_hash(name);
		if (_ged_draw_hash_seen(seen_hashes, seen_count, hash))
		    continue;
		seen_hashes[seen_count++] = hash;
		_ged_draw_index_bucket_remove_entry(*component_tablep, hash,
			entry);
	    }
	    bu_free(seen_hashes, "ged draw index component hashes");
	}

	struct bu_hash_tbl **path_tablep =
	    _ged_draw_path_index_tablep(gdp, is_group);
	if (path_tablep && *path_tablep)
	    _ged_draw_index_bucket_remove_entry(*path_tablep,
		    entry->data.path_hash, entry);
    }
}


static void
_ged_draw_registry_index_table_free(struct bu_hash_tbl **tablep)
{
    if (!tablep || !*tablep)
	return;

    struct bu_hash_entry *entry = NULL;
    while ((entry = bu_hash_next(*tablep, entry)) != NULL) {
	struct bu_ptbl *bucket =
	    (struct bu_ptbl *)bu_hash_value(entry, NULL);
	if (!bucket)
	    continue;
	bu_ptbl_free(bucket);
	bu_free(bucket, "ged draw reverse index bucket");
    }
    bu_hash_destroy(*tablep);
    *tablep = NULL;
}


static void
_ged_draw_registry_index_tables_free(struct ged_drawable *gdp)
{
    if (!gdp)
	return;
    _ged_draw_registry_index_table_free(
	    &gdp->gd_draw_shapes_by_component_hash);
    _ged_draw_registry_index_table_free(
	    &gdp->gd_draw_shapes_by_path_hash);
    _ged_draw_registry_index_table_free(
	    &gdp->gd_draw_groups_by_component_hash);
    _ged_draw_registry_index_table_free(
	    &gdp->gd_draw_groups_by_path_hash);
}


static struct ged_draw_registry_entry *
_ged_draw_registry_entry_for_scene_ref_in_gdp(struct ged_drawable *gdp,
					      bsg_scene_ref ref)
{
    if (!gdp || bsg_scene_ref_is_null(ref) || !gdp->gd_draw_registry_init)
	return NULL;
    for (size_t i = 0; i < BU_PTBL_LEN(&gdp->gd_draw_registry); i++) {
	struct ged_draw_registry_entry *entry =
	    (struct ged_draw_registry_entry *)BU_PTBL_GET(&gdp->gd_draw_registry, i);
	if (entry && bsg_scene_ref_equal(entry->scene_ref, ref))
	    return entry;
    }
    return NULL;
}


static struct ged_draw_registry_entry *
_ged_draw_registry_entry_for_scene_ref(struct ged *gedp, bsg_scene_ref ref)
{
    struct ged_drawable *gdp = NULL;
    if (gedp && gedp->i)
	gdp = gedp->i->ged_gdp;
    if (!gdp)
	gdp = _ged_drawable_for_scene_ref(ref);
    struct ged_draw_registry_entry *entry =
	_ged_draw_registry_entry_for_scene_ref_in_gdp(gdp, ref);
    return entry ? entry : _ged_draw_registry_entry_for_scene_ref_global(ref);
}


void
ged_draw_scene_ref_index_remove(struct ged *gedp, bsg_scene_ref ref)
{
    struct ged_draw_registry_entry *entry =
	_ged_draw_registry_entry_for_scene_ref(gedp, ref);
    if (!entry)
	return;
    _ged_draw_registry_entry_unindex(_ged_draw_registry_entry_gdp(entry),
	    entry);
}


void
ged_draw_scene_ref_index_add(struct ged *gedp, bsg_scene_ref ref)
{
    struct ged_draw_registry_entry *entry =
	_ged_draw_registry_entry_for_scene_ref(gedp, ref);
    if (!entry)
	return;
    _ged_draw_registry_entry_index(_ged_draw_registry_entry_gdp(entry),
	    entry);
}


static int
_ged_draw_registry_entry_has_component_name(
	const struct ged_draw_registry_entry *entry,
	const char *name,
	unsigned long long hash)
{
    if (!entry || !name || !name[0] || entry->data.s_fullpath.fp_len <= 0)
	return 0;

    const struct db_full_path *fp = &entry->data.s_fullpath;
    for (size_t i = 0; i < fp->fp_len; i++) {
	const struct directory *dp = fp->fp_names[i];
	const char *component = (dp && dp->d_namep) ? dp->d_namep : NULL;
	if (!component || !component[0])
	    continue;
	if (_ged_draw_index_name_hash(component) == hash &&
		strcmp(component, name) == 0)
	    return 1;
    }

    return 0;
}


static int
_ged_draw_index_for_component(struct ged *gedp,
			      const char *path,
			      int is_group,
			      ged_draw_scene_ref_index_cb cb,
			      void *userdata)
{
    if (!gedp || !gedp->i || !gedp->i->ged_gdp || !path)
	return -1;

    const char *name = _ged_draw_index_component_name(path);
    if (!name || !name[0])
	return -1;

    struct ged_drawable *gdp = gedp->i->ged_gdp;
    struct bu_hash_tbl **tablep =
	_ged_draw_component_index_tablep(gdp, is_group);
    if (!tablep || !*tablep)
	return -1;

    unsigned long long hash = _ged_draw_index_name_hash(name);
    struct bu_ptbl *bucket = _ged_draw_index_bucket(tablep, hash, 0);
    if (is_group)
	gdp->gd_draw_index_group_component_queries++;
    else
	gdp->gd_draw_index_shape_component_queries++;
    if (!bucket)
	return 0;
    if (is_group)
	gdp->gd_draw_index_group_component_candidates += BU_PTBL_LEN(bucket);
    else
	gdp->gd_draw_index_shape_component_candidates += BU_PTBL_LEN(bucket);

    int count = 0;
    for (size_t i = 0; i < BU_PTBL_LEN(bucket); i++) {
	struct ged_draw_registry_entry *entry =
	    (struct ged_draw_registry_entry *)BU_PTBL_GET(bucket, i);
	if (!_ged_draw_entry_indexable(entry, is_group))
	    continue;
	if (!_ged_draw_registry_entry_has_component_name(entry, name, hash))
	    continue;
	count++;
	if (cb && !(*cb)(entry->scene_ref, userdata))
	    break;
    }

    return count;
}


int
ged_draw_shape_index_for_component(struct ged *gedp,
				   const char *path,
				   ged_draw_scene_ref_index_cb cb,
				   void *userdata)
{
    return _ged_draw_index_for_component(gedp, path, 0, cb, userdata);
}


int
ged_draw_group_index_for_component(struct ged *gedp,
				   const char *path,
				   ged_draw_scene_ref_index_cb cb,
				   void *userdata)
{
    return _ged_draw_index_for_component(gedp, path, 1, cb, userdata);
}


static int
_ged_draw_index_for_path_hash(struct ged *gedp,
			      unsigned long long path_hash,
			      int is_group,
			      ged_draw_scene_ref_index_cb cb,
			      void *userdata)
{
    if (!gedp || !gedp->i || !gedp->i->ged_gdp)
	return -1;

    struct ged_drawable *gdp = gedp->i->ged_gdp;
    struct bu_hash_tbl **tablep = _ged_draw_path_index_tablep(gdp, is_group);
    if (!tablep || !*tablep)
	return -1;

    struct bu_ptbl *bucket = _ged_draw_index_bucket(tablep, path_hash, 0);
    gdp->gd_draw_index_path_queries++;
    if (!bucket)
	return 0;
    gdp->gd_draw_index_path_candidates += BU_PTBL_LEN(bucket);

    int count = 0;
    for (size_t i = 0; i < BU_PTBL_LEN(bucket); i++) {
	struct ged_draw_registry_entry *entry =
	    (struct ged_draw_registry_entry *)BU_PTBL_GET(bucket, i);
	if (!_ged_draw_entry_indexable(entry, is_group))
	    continue;
	if (entry->data.path_hash != path_hash)
	    continue;
	count++;
	if (cb && !(*cb)(entry->scene_ref, userdata))
	    break;
    }

    return count;
}


int
ged_draw_shape_index_for_path_hash(struct ged *gedp,
				   unsigned long long path_hash,
				   ged_draw_scene_ref_index_cb cb,
				   void *userdata)
{
    return _ged_draw_index_for_path_hash(gedp, path_hash, 0, cb, userdata);
}


int
ged_draw_group_index_for_path_hash(struct ged *gedp,
				   unsigned long long path_hash,
				   ged_draw_scene_ref_index_cb cb,
				   void *userdata)
{
    return _ged_draw_index_for_path_hash(gedp, path_hash, 1, cb, userdata);
}


void
ged_draw_index_stats_get(struct ged *gedp, struct ged_draw_index_stats *stats)
{
    if (!stats)
	return;
    memset(stats, 0, sizeof(*stats));
    if (!gedp || !gedp->i || !gedp->i->ged_gdp)
	return;

    struct ged_drawable *gdp = gedp->i->ged_gdp;
    stats->shape_component_queries =
	gdp->gd_draw_index_shape_component_queries;
    stats->shape_component_candidates =
	gdp->gd_draw_index_shape_component_candidates;
    stats->group_component_queries =
	gdp->gd_draw_index_group_component_queries;
    stats->group_component_candidates =
	gdp->gd_draw_index_group_component_candidates;
    stats->path_queries = gdp->gd_draw_index_path_queries;
    stats->path_candidates = gdp->gd_draw_index_path_candidates;
    stats->fallback_shape_scans =
	gdp->gd_draw_index_fallback_shape_scans;
    stats->fallback_group_scans =
	gdp->gd_draw_index_fallback_group_scans;
}


void
ged_draw_index_stats_reset(struct ged *gedp)
{
    if (!gedp || !gedp->i || !gedp->i->ged_gdp)
	return;

    struct ged_drawable *gdp = gedp->i->ged_gdp;
    gdp->gd_draw_index_shape_component_queries = 0;
    gdp->gd_draw_index_shape_component_candidates = 0;
    gdp->gd_draw_index_group_component_queries = 0;
    gdp->gd_draw_index_group_component_candidates = 0;
    gdp->gd_draw_index_path_queries = 0;
    gdp->gd_draw_index_path_candidates = 0;
    gdp->gd_draw_index_fallback_shape_scans = 0;
    gdp->gd_draw_index_fallback_group_scans = 0;
}


static struct ged_draw_registry_entry *
_ged_draw_registry_entry_for_token(struct ged *gedp, uintptr_t token, int is_group)
{
    if (!gedp || !gedp->i || !gedp->i->ged_gdp || !token)
	return NULL;
    struct ged_drawable *gdp = gedp->i->ged_gdp;
    if (!gdp->gd_draw_registry_init)
	return NULL;
    for (size_t i = 0; i < BU_PTBL_LEN(&gdp->gd_draw_registry); i++) {
	struct ged_draw_registry_entry *entry =
	    (struct ged_draw_registry_entry *)BU_PTBL_GET(&gdp->gd_draw_registry, i);
	if (entry && entry->token == token && entry->is_group == is_group)
	    return entry;
    }
    return NULL;
}


static struct ged_draw_registry_entry *
_ged_draw_registry_entry_ensure(struct ged *gedp, bsg_scene_ref ref, int is_group)
{
    if (bsg_scene_ref_is_null(ref))
	return NULL;
    struct ged_draw_registry_entry *entry =
	_ged_draw_registry_entry_for_scene_ref(gedp, ref);
    if (entry) {
	if (gedp)
	    entry->data.gedp = gedp;
	if (entry->is_group != (is_group ? 1 : 0)) {
	    _ged_draw_registry_entry_unindex(
		    _ged_draw_registry_entry_gdp(entry), entry);
	    entry->is_group = is_group ? 1 : 0;
	    _ged_draw_registry_entry_index(
		    _ged_draw_registry_entry_gdp(entry), entry);
	} else {
	    entry->is_group = is_group ? 1 : 0;
	}
	return entry;
    }
    if (!gedp || !gedp->i || !gedp->i->ged_gdp)
	return NULL;
    struct ged_drawable *gdp = gedp->i->ged_gdp;
    _ged_draw_registry_init_if_needed(gdp);
    entry = (struct ged_draw_registry_entry *)bu_calloc(1, sizeof(*entry),
	    "ged draw registry entry");
    entry->magic = GED_DRAW_REGISTRY_ENTRY_MAGIC;
    entry->token = (uintptr_t)gdp->gd_draw_next_token++;
    if (!entry->token)
	entry->token = (uintptr_t)gdp->gd_draw_next_token++;
    entry->is_group = is_group ? 1 : 0;
    entry->scene_ref = ref;
    _ged_draw_shape_state_init(&entry->data, gedp);
    bu_ptbl_ins(&gdp->gd_draw_registry, (long *)entry);
    _ged_draw_registry_global_insert(entry);
    return entry;
}


void
ged_draw_scene_ref_highlight_free_cb(bsg_scene_ref ref)
{
    if (bsg_scene_ref_is_null(ref))
	return;
    struct ged_drawable *gdp = _ged_drawable_for_scene_ref(ref);
    struct ged_draw_registry_entry *entry =
	_ged_draw_registry_entry_for_scene_ref_in_gdp(gdp, ref);
    if (!entry)
	return;
    if (gdp && gdp->gd_highlight_token == entry->token) {
	gdp->gd_highlight_token = 0;
	gdp->gd_highlight_scene_rev = 0;
	gdp->gd_highlight_rev++;
    }
    ged_draw_shape_state_release_scene_ref(ref);
}


ged_draw_shape_state *
ged_draw_shape_state_get_scene_ref(bsg_scene_ref ref)
{
    if (bsg_scene_ref_is_null(ref))
	return NULL;
    struct ged_draw_registry_entry *entry =
	_ged_draw_registry_entry_for_scene_ref(NULL, ref);
    return entry ? &entry->data : NULL;
}


ged_draw_shape_state *
ged_draw_shape_state_ensure_scene_ref(struct ged *gedp, bsg_scene_ref ref)
{
    if (bsg_scene_ref_is_null(ref))
	return NULL;
    int is_group = (bsg_scene_ref_type(ref) == BSG_SCENE_ELEMENT_GROUP);
    struct ged_draw_registry_entry *entry =
	_ged_draw_registry_entry_ensure(gedp, ref, is_group);
    if (!entry)
	return NULL;
    return &entry->data;
}


void
ged_draw_shape_state_release_scene_ref(bsg_scene_ref ref)
{
    if (bsg_scene_ref_is_null(ref))
	return;
    struct ged_draw_registry_entry *entry =
	_ged_draw_registry_entry_for_scene_ref(NULL, ref);
    if (!entry)
	return;
    _ged_draw_registry_entry_unindex(_ged_draw_registry_entry_gdp(entry),
	    entry);
    _ged_draw_shape_state_free_contents(&entry->data);
    if (entry)
	entry->scene_ref = bsg_scene_ref_null();
}


ged_draw_shape_ref
ged_draw_shape_ref_from_scene_ref(struct ged *gedp, bsg_scene_ref ref)
{
    ged_draw_shape_ref shape_ref = GED_DRAW_SHAPE_REF_NULL;
    if (!gedp || bsg_scene_ref_is_null(ref))
	return shape_ref;
    struct ged_draw_registry_entry *entry =
	_ged_draw_registry_entry_ensure(gedp, ref, 0);
    if (!entry)
	return shape_ref;
    shape_ref.token = entry->token;
    shape_ref.scene_revision = ged_draw_scene_revision(gedp);
    return shape_ref;
}


ged_draw_group_ref
ged_draw_group_ref_from_scene_ref(struct ged *gedp, bsg_scene_ref ref)
{
    ged_draw_group_ref group_ref = GED_DRAW_GROUP_REF_NULL;
    if (!gedp || bsg_scene_ref_is_null(ref))
	return group_ref;
    struct ged_draw_registry_entry *entry =
	_ged_draw_registry_entry_ensure(gedp, ref, 1);
    if (!entry)
	return group_ref;
    group_ref.token = entry->token;
    group_ref.scene_revision = ged_draw_scene_revision(gedp);
    return group_ref;
}


bsg_scene_ref
ged_draw_registry_shape_scene_ref(struct ged *gedp, ged_draw_shape_ref ref)
{
    if (!gedp || ged_draw_shape_ref_is_null(ref))
	return bsg_scene_ref_null();
    if (ref.scene_revision && ref.scene_revision != ged_draw_scene_revision(gedp))
	return bsg_scene_ref_null();
    struct ged_draw_registry_entry *entry =
	_ged_draw_registry_entry_for_token(gedp, ref.token, 0);
    return entry ? entry->scene_ref : bsg_scene_ref_null();
}


bsg_scene_ref
ged_draw_shape_scene_ref_from_cache_ref(struct ged *gedp, ged_draw_shape_ref ref)
{
    if (!gedp || ged_draw_shape_ref_is_null(ref))
	return bsg_scene_ref_null();
    struct ged_draw_registry_entry *entry =
	_ged_draw_registry_entry_for_token(gedp, ref.token, 0);
    return entry ? entry->scene_ref : bsg_scene_ref_null();
}


bsg_scene_ref
ged_draw_registry_group_scene_ref(struct ged *gedp, ged_draw_group_ref ref)
{
    if (!gedp || ged_draw_group_ref_is_null(ref))
	return bsg_scene_ref_null();
    if (ref.scene_revision && ref.scene_revision != ged_draw_scene_revision(gedp))
	return bsg_scene_ref_null();
    struct ged_draw_registry_entry *entry =
	_ged_draw_registry_entry_for_token(gedp, ref.token, 1);
    return entry ? entry->scene_ref : bsg_scene_ref_null();
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
