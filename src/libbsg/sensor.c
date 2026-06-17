/*                     S E N S O R . C
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
/** @file libbsg/sensor.c
 *
 * Typed field/node/timer/update sensors.
 */

#include "common.h"

#include <string.h>

#include "bsg/sensor.h"
#include "sensor_private.h"

#define BSG_SENSOR_MAX 512

struct bsg_sensor_entry {
    int active;
    uint64_t generation;
    uint64_t sequence;
    bsg_sensor_kind kind;
    bsg_field_ref field;
    bsg_node_ref node;
    long interval_ms;
    bsg_field_sensor_cb field_cb;
    bsg_node_sensor_cb node_cb;
    bsg_timer_sensor_cb timer_cb;
    bsg_update_sensor_cb update_cb;
    void *data;
};

static struct bsg_sensor_entry s_registry[BSG_SENSOR_MAX];
static uint64_t s_next_generation = 1;
static uint64_t s_next_sequence = 1;

static bsg_sensor_ref
_sensor_ref(struct bsg_sensor_entry *entry)
{
    bsg_sensor_ref ref = BSG_SENSOR_REF_NULL_INIT;
    if (entry && entry->active) {
	ref.opaque = entry;
	ref.generation = entry->generation;
    }
    return ref;
}

static struct bsg_sensor_entry *
_sensor_entry(bsg_sensor_ref ref)
{
    struct bsg_sensor_entry *entry = (struct bsg_sensor_entry *)ref.opaque;
    if (!entry || !entry->active || entry->generation != ref.generation)
	return NULL;
    return entry;
}

static struct bsg_sensor_entry *
_sensor_alloc(bsg_sensor_kind kind)
{
    if (kind == BSG_SENSOR_KIND_INVALID)
	return NULL;

    for (size_t i = 0; i < BSG_SENSOR_MAX; i++) {
	if (!s_registry[i].active) {
	    memset(&s_registry[i], 0, sizeof(s_registry[i]));
	    s_registry[i].active = 1;
	    s_registry[i].generation = s_next_generation++;
	    s_registry[i].sequence = s_next_sequence++;
	    s_registry[i].kind = kind;
	    if (!s_next_generation)
		s_next_generation = 1;
	    if (!s_next_sequence)
		s_next_sequence = 1;
	    return &s_registry[i];
	}
    }

    return NULL;
}

static struct bsg_sensor_entry *
_sensor_next_matching(uint64_t *last_sequence,
		      bsg_sensor_kind kind,
		      bsg_field_ref field,
		      bsg_node_ref node)
{
    struct bsg_sensor_entry *best = NULL;

    for (size_t i = 0; i < BSG_SENSOR_MAX; i++) {
	struct bsg_sensor_entry *entry = &s_registry[i];
	if (!entry->active || entry->sequence <= *last_sequence)
	    continue;
	if (kind != BSG_SENSOR_KIND_INVALID && entry->kind != kind)
	    continue;
	if (!bsg_field_ref_is_null(field) &&
		!bsg_field_ref_equal(entry->field, field))
	    continue;
	if (!bsg_node_ref_is_null(node) &&
		!bsg_node_ref_equal(entry->node, node))
	    continue;
	if (!best || entry->sequence < best->sequence)
	    best = entry;
    }

    if (best)
	*last_sequence = best->sequence;
    return best;
}

bsg_sensor_ref
bsg_sensor_ref_null(void)
{
    bsg_sensor_ref ref = BSG_SENSOR_REF_NULL_INIT;
    return ref;
}

int
bsg_sensor_ref_is_null(bsg_sensor_ref ref)
{
    return _sensor_entry(ref) ? 0 : 1;
}

int
bsg_sensor_ref_equal(bsg_sensor_ref a, bsg_sensor_ref b)
{
    return a.opaque == b.opaque && a.generation == b.generation;
}

bsg_sensor_kind
bsg_sensor_ref_kind(bsg_sensor_ref ref)
{
    struct bsg_sensor_entry *entry = _sensor_entry(ref);
    return entry ? entry->kind : BSG_SENSOR_KIND_INVALID;
}

void
bsg_sensor_ref_destroy(bsg_sensor_ref ref)
{
    struct bsg_sensor_entry *entry = _sensor_entry(ref);
    if (!entry)
	return;
    memset(entry, 0, sizeof(*entry));
}

bsg_sensor_ref
bsg_field_sensor_ref_as_sensor(bsg_field_sensor_ref ref)
{
    return ref.sensor;
}

bsg_sensor_ref
bsg_node_sensor_ref_as_sensor(bsg_node_sensor_ref ref)
{
    return ref.sensor;
}

bsg_sensor_ref
bsg_timer_sensor_ref_as_sensor(bsg_timer_sensor_ref ref)
{
    return ref.sensor;
}

bsg_sensor_ref
bsg_update_sensor_ref_as_sensor(bsg_update_sensor_ref ref)
{
    return ref.sensor;
}

bsg_field_sensor_ref
bsg_field_sensor_ref_create(bsg_field_ref field, bsg_field_sensor_cb cb, void *data)
{
    bsg_field_sensor_ref ref = BSG_FIELD_REF_SENSOR_NULL_INIT;
    if (bsg_field_ref_is_null(field) || !cb)
	return ref;

    struct bsg_sensor_entry *entry = _sensor_alloc(BSG_SENSOR_KIND_FIELD);
    if (!entry)
	return ref;

    entry->field = field;
    entry->field_cb = cb;
    entry->data = data;
    ref.sensor = _sensor_ref(entry);
    return ref;
}

bsg_node_sensor_ref
bsg_node_sensor_ref_create(bsg_node_ref node, bsg_node_sensor_cb cb, void *data)
{
    bsg_node_sensor_ref ref = BSG_NODE_REF_SENSOR_NULL_INIT;
    if (bsg_node_ref_is_null(node) || !cb)
	return ref;

    struct bsg_sensor_entry *entry = _sensor_alloc(BSG_SENSOR_KIND_NODE);
    if (!entry)
	return ref;

    entry->node = node;
    entry->node_cb = cb;
    entry->data = data;
    ref.sensor = _sensor_ref(entry);
    return ref;
}

bsg_timer_sensor_ref
bsg_timer_sensor_ref_create(long interval_ms, bsg_timer_sensor_cb cb, void *data)
{
    bsg_timer_sensor_ref ref = BSG_TIMER_REF_SENSOR_NULL_INIT;
    if (!cb)
	return ref;

    struct bsg_sensor_entry *entry = _sensor_alloc(BSG_SENSOR_KIND_TIMER);
    if (!entry)
	return ref;

    entry->interval_ms = interval_ms < 0 ? 0 : interval_ms;
    entry->timer_cb = cb;
    entry->data = data;
    ref.sensor = _sensor_ref(entry);
    return ref;
}

bsg_update_sensor_ref
bsg_update_sensor_ref_create(bsg_update_sensor_cb cb, void *data)
{
    bsg_update_sensor_ref ref = BSG_UPDATE_REF_SENSOR_NULL_INIT;
    if (!cb)
	return ref;

    struct bsg_sensor_entry *entry = _sensor_alloc(BSG_SENSOR_KIND_UPDATE);
    if (!entry)
	return ref;

    entry->update_cb = cb;
    entry->data = data;
    ref.sensor = _sensor_ref(entry);
    return ref;
}

void
bsg_field_sensor_ref_destroy(bsg_field_sensor_ref ref)
{
    bsg_sensor_ref_destroy(ref.sensor);
}

void
bsg_node_sensor_ref_destroy(bsg_node_sensor_ref ref)
{
    bsg_sensor_ref_destroy(ref.sensor);
}

void
bsg_timer_sensor_ref_destroy(bsg_timer_sensor_ref ref)
{
    bsg_sensor_ref_destroy(ref.sensor);
}

void
bsg_update_sensor_ref_destroy(bsg_update_sensor_ref ref)
{
    bsg_sensor_ref_destroy(ref.sensor);
}

bsg_field_ref
bsg_field_sensor_ref_field(bsg_field_sensor_ref ref)
{
    struct bsg_sensor_entry *entry = _sensor_entry(ref.sensor);
    return (entry && entry->kind == BSG_SENSOR_KIND_FIELD) ? entry->field : bsg_field_ref_null();
}

bsg_node_ref
bsg_node_sensor_ref_node(bsg_node_sensor_ref ref)
{
    struct bsg_sensor_entry *entry = _sensor_entry(ref.sensor);
    return (entry && entry->kind == BSG_SENSOR_KIND_NODE) ? entry->node : bsg_node_ref_null();
}

long
bsg_timer_sensor_ref_interval_ms(bsg_timer_sensor_ref ref)
{
    struct bsg_sensor_entry *entry = _sensor_entry(ref.sensor);
    return (entry && entry->kind == BSG_SENSOR_KIND_TIMER) ? entry->interval_ms : 0;
}

int
bsg_timer_sensor_ref_trigger(bsg_timer_sensor_ref ref)
{
    struct bsg_sensor_entry *entry = _sensor_entry(ref.sensor);
    if (!entry || entry->kind != BSG_SENSOR_KIND_TIMER || !entry->timer_cb)
	return 0;
    (void)entry->timer_cb(ref, entry->data);
    return 1;
}

int
bsg_update_sensor_ref_trigger(bsg_update_sensor_ref ref)
{
    struct bsg_sensor_entry *entry = _sensor_entry(ref.sensor);
    if (!entry || entry->kind != BSG_SENSOR_KIND_UPDATE || !entry->update_cb)
	return 0;
    (void)entry->update_cb(ref, entry->data);
    return 1;
}

int
bsg_update_sensors_fire(void)
{
    int fired = 0;
    uint64_t last_sequence = 0;
    struct bsg_sensor_entry *entry;

    while ((entry = _sensor_next_matching(&last_sequence,
		    BSG_SENSOR_KIND_UPDATE, bsg_field_ref_null(), bsg_node_ref_null())) != NULL) {
	bsg_update_sensor_ref ref = BSG_UPDATE_REF_SENSOR_NULL_INIT;
	ref.sensor = _sensor_ref(entry);
	if (entry->update_cb) {
	    (void)entry->update_cb(ref, entry->data);
	    fired++;
	}
    }

    return fired;
}

void
bsg_sensor_notify_field_ref(bsg_field_ref field)
{
    if (bsg_field_ref_is_null(field))
	return;

    bsg_node_ref owner = bsg_field_owner(field);
    uint64_t last_sequence = 0;
    struct bsg_sensor_entry *entry;

    while ((entry = _sensor_next_matching(&last_sequence,
		    BSG_SENSOR_KIND_INVALID, bsg_field_ref_null(), bsg_node_ref_null())) != NULL) {
	if (entry->kind == BSG_SENSOR_KIND_FIELD &&
		bsg_field_ref_equal(entry->field, field) &&
		entry->field_cb) {
	    (void)entry->field_cb(field, entry->data);
	    continue;
	}
	if (entry->kind == BSG_SENSOR_KIND_NODE &&
		!bsg_node_ref_is_null(owner) &&
		bsg_node_ref_equal(entry->node, owner) &&
		entry->node_cb) {
	    (void)entry->node_cb(owner, field, entry->data);
	}
    }
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
