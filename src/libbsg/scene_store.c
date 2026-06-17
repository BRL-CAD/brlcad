/*                    S C E N E _ S T O R E . C
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
/** @file libbsg/scene_store.c
 *
 * Private scene-store records for DB object lookup plus the remaining
 * recycled-node storage.
 */

#include "common.h"

#include "bu/list.h"
#include "bu/malloc.h"
#include "bu/path.h"
#include "bu/ptbl.h"
#include "bu/str.h"
#include "bu/vls.h"

#include "bsg/util.h"
#include "bsg_private.h"
#include "draw_source_private.h"
#include "draw_intent_private.h"
#include "field_private.h"
#include "node_storage_private.h"
#include "payload_typed_private.h"
#include "scene_object_private.h"
#include "scene_store_private.h"

#define BSG_VIEW_OBJ_STORE_MAGIC 0x62736f73u

struct bsg_scene_recycle_entry {
    struct bu_list l;
    bsg_node *node;
};

struct bsg_scene_recycle_pool {
    struct bu_list entries;
    struct bu_ptbl active_nodes;
};

struct bsg_scene_db_record {
    bsg_node *node;
    unsigned int source_flags;
};

struct bsg_scene_db_records {
    struct bu_ptbl records;
};

struct bsg_view_obj_store {
    uint32_t magic;
    struct bsg_scene_db_records db_records;
    struct bsg_scene_recycle_pool *recycle_pool;
};

struct bsg_view_set_scene_store {
    struct bsg_scene_db_records shared_db_records;
    struct bsg_scene_recycle_pool *recycle_pool;
};


static struct bsg_view_obj_store *
_bsg_view_obj_store(const struct bsg_view_obj_pool *pool)
{
    if (!pool || !pool->i)
	return NULL;

    struct bsg_view_obj_store *store =
	(struct bsg_view_obj_store *)pool->i;
    return (store->magic == BSG_VIEW_OBJ_STORE_MAGIC) ? store : NULL;
}


static struct bsg_scene_recycle_pool *
_bsg_view_obj_recycle_pool(const struct bsg_view_obj_pool *pool)
{
    if (!pool)
	return NULL;

    struct bsg_view_obj_store *store = _bsg_view_obj_store(pool);
    return store ? store->recycle_pool : NULL;
}


static struct bsg_scene_db_records *
_bsg_view_obj_db_records(const struct bsg_view_obj_pool *pool)
{
    if (!pool)
	return NULL;

    struct bsg_view_obj_store *store = _bsg_view_obj_store(pool);
    return store ? &store->db_records : NULL;
}


static struct bsg_view_set_scene_store *
_bsg_view_set_scene_store_get(const struct bsg_view_set_internal *store)
{
    return store ? store->scene_store : NULL;
}


static void
_bsg_scene_db_records_init(struct bsg_scene_db_records *records,
	const char *desc)
{
    if (!records)
	return;

    bu_ptbl_init(&records->records, 8, desc ? desc : "scene db records");
}


static void
_bsg_scene_db_record_free(struct bsg_scene_db_record *record)
{
    if (!record)
	return;

    record->node = NULL;
    record->source_flags = 0;
    BU_PUT(record, struct bsg_scene_db_record);
}


static void
_bsg_scene_db_records_clear(struct bsg_scene_db_records *records)
{
    if (!records || !BU_PTBL_IS_INITIALIZED(&records->records))
	return;

    while (BU_PTBL_LEN(&records->records) > 0) {
	struct bsg_scene_db_record *record =
	    (struct bsg_scene_db_record *)BU_PTBL_GET(&records->records, 0);
	bu_ptbl_rm(&records->records, (long *)record);
	_bsg_scene_db_record_free(record);
    }
}


static void
_bsg_scene_db_records_free(struct bsg_scene_db_records *records)
{
    if (!records || !BU_PTBL_IS_INITIALIZED(&records->records))
	return;

    _bsg_scene_db_records_clear(records);
    bu_ptbl_free(&records->records);
}


static struct bsg_scene_db_records *
_bsg_scene_db_records(struct bsg_view *v, int type)
{
    if (!v)
	return NULL;

    if (type & BSG_SOURCE_DB) {
	if (type & BSG_SOURCE_LOCAL || bsg_view_is_independent(v))
	    return _bsg_view_obj_db_records(&v->gv_objs);

	if (v->vset) {
	    struct bsg_view_set_scene_store *store =
		_bsg_view_set_scene_store_get(v->vset->i);
	    return store ? &store->shared_db_records : NULL;
	}
    }

    return NULL;
}


static long
_bsg_scene_db_record_locate_node(struct bsg_scene_db_records *records,
	const bsg_node *node)
{
    if (!records || !BU_PTBL_IS_INITIALIZED(&records->records) || !node)
	return -1;

    for (size_t i = 0; i < BU_PTBL_LEN(&records->records); i++) {
	struct bsg_scene_db_record *record =
	    (struct bsg_scene_db_record *)BU_PTBL_GET(&records->records, i);
	if (record && record->node == node)
	    return (long)i;
    }

    return -1;
}


static bsg_node *
_bsg_scene_db_records_find(struct bsg_scene_db_records *records,
	const char *name, int path_match)
{
    if (!records || !BU_PTBL_IS_INITIALIZED(&records->records) || !name)
	return NULL;

    for (size_t i = 0; i < BU_PTBL_LEN(&records->records); i++) {
	struct bsg_scene_db_record *record =
	    (struct bsg_scene_db_record *)BU_PTBL_GET(&records->records, i);
	bsg_node *node = record ? record->node : NULL;
	if (!node)
	    continue;
	const char *node_name = node ? bsg_node_name(node) : NULL;
	if (path_match) {
	    if (!bu_path_match(name, node_name, 0))
		return node;
	} else if (BU_STR_EQUAL(name, node_name)) {
	    return node;
	}
    }

    return NULL;
}


static struct bsg_scene_db_records *
_bsg_scene_db_records_for_node(const bsg_node *node)
{
    if (!node)
	return NULL;

    unsigned int flags = bsg_node_source_flags(node);
    if (!(flags & BSG_SOURCE_DB) || (flags & BSG_SOURCE_CHILD))
	return NULL;

    return _bsg_scene_db_records(bsg_node_get_view(node), (int)flags);
}


static void
_bsg_scene_recycle_payloads_clear(bsg_node *node)
{
    if (!node)
	return;

    bsg_node_set_payload(node, NULL);
}


struct bsg_scene_recycle_pool *
bsg_scene_recycle_pool_create(void)
{
    struct bsg_scene_recycle_pool *pool;

    BU_GET(pool, struct bsg_scene_recycle_pool);
    BU_LIST_INIT(&pool->entries);
    bu_ptbl_init(&pool->active_nodes, 8, "scene recycle active nodes");

    return pool;
}


bsg_node *
bsg_scene_recycle_acquire(struct bsg_scene_recycle_pool *pool)
{
    bsg_node *node = NULL;

    if (!pool)
	return NULL;

    if (BU_LIST_IS_EMPTY(&pool->entries)) {
	BU_ALLOC(node, struct bsg_node);
    } else {
	struct bsg_scene_recycle_entry *entry =
	    BU_LIST_NEXT(bsg_scene_recycle_entry, &pool->entries);
	BU_LIST_DEQUEUE(&entry->l);
	node = entry->node;
	BU_PUT(entry, struct bsg_scene_recycle_entry);
    }

    bsg_node_internal_ensure(node);
    bu_ptbl_ins_unique(&pool->active_nodes, (long *)node);

    return node;
}


void
bsg_scene_recycle_release(struct bsg_scene_recycle_pool *pool, bsg_node *node)
{
    if (!pool || !node)
	return;

    if (BU_PTBL_IS_INITIALIZED(&pool->active_nodes))
	bu_ptbl_rm(&pool->active_nodes, (long *)node);

    _bsg_scene_recycle_payloads_clear(node);
    struct bsg_scene_recycle_entry *entry;
    BU_GET(entry, struct bsg_scene_recycle_entry);
    BU_LIST_INIT(&entry->l);
    entry->node = node;
    BU_LIST_APPEND(&pool->entries, &entry->l);
}


static int
_bsg_scene_recycle_pool_has_node(const struct bsg_scene_recycle_pool *pool,
	const bsg_node *node)
{
    if (!pool || !node || !BU_PTBL_IS_INITIALIZED(&pool->active_nodes))
	return 0;

    return bu_ptbl_locate(&pool->active_nodes, (const long *)node) >= 0;
}


struct bsg_scene_recycle_pool *
bsg_scene_node_recycle_pool(const bsg_node *node)
{
    if (!node)
	return NULL;

    const bsg_node *parent = (const bsg_node *)node->parent;
    struct bsg_scene_recycle_pool *parent_pool =
	bsg_scene_node_recycle_pool(parent);
    if (_bsg_scene_recycle_pool_has_node(parent_pool, node))
	return parent_pool;

    struct bsg_view *v = bsg_node_get_view(node);
    if (!v && parent)
	v = bsg_node_get_view(parent);
    if (!v)
	return parent_pool;

    struct bsg_scene_recycle_pool *local =
	_bsg_view_obj_recycle_pool(&v->gv_objs);
    if (_bsg_scene_recycle_pool_has_node(local, node))
	return local;

    if (v->vset) {
	struct bsg_view_set_scene_store *store =
	    _bsg_view_set_scene_store_get(v->vset->i);
	if (store && _bsg_scene_recycle_pool_has_node(store->recycle_pool, node))
	    return store->recycle_pool;
    }

    if (parent_pool)
	return parent_pool;

    return bsg_scene_recycle_pool_for_create(v, (int)bsg_node_source_flags(node));
}


int
bsg_scene_store_node_register_db_record(bsg_node *node)
{
    struct bsg_scene_db_records *records =
	_bsg_scene_db_records_for_node(node);

    if (!records || !BU_PTBL_IS_INITIALIZED(&records->records))
	return 0;

    long loc = _bsg_scene_db_record_locate_node(records, node);
    if (loc >= 0) {
	struct bsg_scene_db_record *record =
	    (struct bsg_scene_db_record *)BU_PTBL_GET(&records->records, (size_t)loc);
	if (record)
	    record->source_flags = bsg_node_source_flags(node);
	return 1;
    }

    struct bsg_scene_db_record *record;
    BU_GET(record, struct bsg_scene_db_record);
    record->node = node;
    record->source_flags = bsg_node_source_flags(node);
    bu_ptbl_ins(&records->records, (long *)record);
    return 1;
}


int
bsg_scene_store_node_unregister_db_record(bsg_node *node)
{
    struct bsg_scene_db_records *records =
	_bsg_scene_db_records_for_node(node);

    long loc = _bsg_scene_db_record_locate_node(records, node);
    if (loc < 0)
	return 0;

    struct bsg_scene_db_record *record =
	(struct bsg_scene_db_record *)BU_PTBL_GET(&records->records, (size_t)loc);
    bu_ptbl_rm(&records->records, (long *)record);
    _bsg_scene_db_record_free(record);
    return 1;
}


void
bsg_scene_store_db_nodes_release(struct bsg_view *v, int type)
{
    struct bsg_scene_db_records *records = _bsg_scene_db_records(v, type);
    struct bu_ptbl nodes;

    if (!records || !BU_PTBL_IS_INITIALIZED(&records->records))
	return;

    bu_ptbl_init(&nodes, BU_PTBL_LEN(&records->records), "scene db release");
    for (size_t i = 0; i < BU_PTBL_LEN(&records->records); i++) {
	struct bsg_scene_db_record *record =
	    (struct bsg_scene_db_record *)BU_PTBL_GET(&records->records, i);
	bsg_node *node = record ? record->node : NULL;
	if (node)
	    bu_ptbl_ins(&nodes, (long *)node);
    }

    for (size_t i = 0; i < BU_PTBL_LEN(&nodes); i++) {
	bsg_node *node = (bsg_node *)BU_PTBL_GET(&nodes, i);
	bsg_scene_node_release(node);
    }

    bu_ptbl_free(&nodes);
    _bsg_scene_db_records_clear(records);
}


size_t
bsg_scene_store_db_nodes_count(struct bsg_view *v, int type)
{
    struct bsg_scene_db_records *records = _bsg_scene_db_records(v, type);

    return (records && BU_PTBL_IS_INITIALIZED(&records->records)) ?
	BU_PTBL_LEN(&records->records) : 0;
}


bsg_node *
bsg_scene_store_db_node_find(struct bsg_view *v, const char *name,
	int path_match)
{
    struct bsg_scene_db_records *shared =
	_bsg_scene_db_records(v, BSG_SOURCE_DB);
    struct bsg_scene_db_records *local =
	_bsg_scene_db_records(v, BSG_SOURCE_DB | BSG_SOURCE_LOCAL);
    bsg_node *found = _bsg_scene_db_records_find(shared, name, path_match);

    if (found)
	return found;

    if (local && local != shared)
	return _bsg_scene_db_records_find(local, name, path_match);

    return NULL;
}


int
bsg_scene_store_db_node_name_exists(struct bsg_view *v, const char *name)
{
    return bsg_scene_store_db_node_find(v, name, 0) ? 1 : 0;
}


int
bsg_scene_store_db_nodes_visit(struct bsg_view *v, int type,
	bsg_scene_store_node_cb cb, void *data)
{
    struct bsg_scene_db_records *records = _bsg_scene_db_records(v, type);

    if (!records || !BU_PTBL_IS_INITIALIZED(&records->records) || !cb)
	return 1;

    for (size_t i = 0; i < BU_PTBL_LEN(&records->records); i++) {
	struct bsg_scene_db_record *record =
	    (struct bsg_scene_db_record *)BU_PTBL_GET(&records->records, i);
	bsg_node *node = record ? record->node : NULL;
	if (node && !cb(node, data))
	    return 0;
    }

    return 1;
}


struct bsg_scene_recycle_pool *
bsg_scene_recycle_pool_for_create(struct bsg_view *v, int type)
{
    if (!v)
	return NULL;

    if (type & BSG_SOURCE_LOCAL || type & BSG_SOURCE_CHILD ||
	    bsg_view_is_independent(v) || !v->vset)
	return _bsg_view_obj_recycle_pool(&v->gv_objs);

    struct bsg_view_set_scene_store *store =
	_bsg_view_set_scene_store_get(v->vset->i);
    if (!store)
	return NULL;

    return store->recycle_pool;
}


void
bsg_scene_view_obj_pool_init(struct bsg_view_obj_pool *pool)
{
    if (!pool)
	return;

    struct bsg_view_obj_store *store;
    BU_GET(store, struct bsg_view_obj_store);
    store->magic = BSG_VIEW_OBJ_STORE_MAGIC;
    _bsg_scene_db_records_init(&store->db_records, "view db records init");
    store->recycle_pool = bsg_scene_recycle_pool_create();

    pool->i = store;
}


void
bsg_scene_view_obj_pool_free(struct bsg_view_obj_pool *pool)
{
    if (!pool)
	return;

    struct bsg_view_obj_store *store = _bsg_view_obj_store(pool);
    if (store) {
	_bsg_scene_db_records_free(&store->db_records);
	bsg_scene_recycle_pool_destroy(store->recycle_pool);
	store->recycle_pool = NULL;
	store->magic = 0;
	BU_PUT(store, struct bsg_view_obj_store);
    }

    pool->i = NULL;
}


void
bsg_scene_view_set_store_init(struct bsg_view_set_internal *store)
{
    if (!store)
	return;

    struct bsg_view_set_scene_store *scene_store;
    BU_GET(scene_store, struct bsg_view_set_scene_store);
    _bsg_scene_db_records_init(&scene_store->shared_db_records,
	    "shared db records init");
    scene_store->recycle_pool = bsg_scene_recycle_pool_create();
    store->scene_store = scene_store;
}


void
bsg_scene_view_set_store_free(struct bsg_view_set_internal *store)
{
    if (!store)
	return;

    struct bsg_view_set_scene_store *scene_store =
	_bsg_view_set_scene_store_get(store);
    if (!scene_store)
	return;

    _bsg_scene_db_records_free(&scene_store->shared_db_records);
    bsg_scene_recycle_pool_destroy(scene_store->recycle_pool);
    scene_store->recycle_pool = NULL;
    BU_PUT(scene_store, struct bsg_view_set_scene_store);
    store->scene_store = NULL;
}


struct bsg_scene_recycle_pool *
bsg_scene_view_set_recycle_pool(const struct bsg_view_set_internal *store)
{
    struct bsg_view_set_scene_store *scene_store =
	_bsg_view_set_scene_store_get(store);
    return scene_store ? scene_store->recycle_pool : NULL;
}


void
bsg_scene_recycle_pool_destroy(struct bsg_scene_recycle_pool *pool)
{
    struct bsg_scene_recycle_entry *entry;
    struct bsg_scene_recycle_entry *next;
    bsg_node *node;

    if (!pool)
	return;

    entry = BU_LIST_NEXT(bsg_scene_recycle_entry, &pool->entries);
    while (BU_LIST_NOT_HEAD(entry, &pool->entries)) {
	next = BU_LIST_PNEXT(bsg_scene_recycle_entry, entry);
	BU_LIST_DEQUEUE(&entry->l);
	node = entry->node;

	bsg_node_invoke_release_callback(node);

	_bsg_scene_recycle_payloads_clear(node);

	bsg_node_set_draw_intent(node, NULL);

	if (BU_PTBL_IS_INITIALIZED(&node->children))
	    bu_ptbl_free(&node->children);

	bsg_node_fields_free(node);

	if (node->i) {
	    BU_PUT(node->i, struct bsg_node_internal);
	    node->i = NULL;
	}

	BU_PUT(node, struct bsg_node);
	BU_PUT(entry, struct bsg_scene_recycle_entry);
	entry = next;
    }

    if (BU_PTBL_IS_INITIALIZED(&pool->active_nodes))
	bu_ptbl_free(&pool->active_nodes);

    BU_PUT(pool, struct bsg_scene_recycle_pool);
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
