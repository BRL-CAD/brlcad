/*             B S G _ G E D _ D R A W _ B I N D I N G . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
/** @file libged/bsg_ged_draw_binding.c
 *
 * GED draw-source database binding and realization-state mirrors.
 */

#include "common.h"

#include "bu/malloc.h"
#include "bsg/appearance.h"
#include "bsg/database_source.h"
#include "bsg/lod.h"
#include "bsg/scene_builder.h"
#include "ged/bsg_ged_draw.h"
#include "./ged_private.h"
#include "./bsg_ged_draw_private.h"


bsg_scene_ref
ged_draw_shape_source_ref(bsg_scene_ref ref)
{
    if (bsg_scene_ref_is_null(ref))
	return bsg_scene_ref_null();
    if (bsg_database_source_ref_is_container(
		bsg_database_source_ref_from_scene(ref)))
	return ref;

    ged_draw_shape_state *shape_data = ged_draw_shape_state_get_scene_ref(ref);
    if (shape_data && !bsg_scene_ref_is_null(shape_data->source_ref))
	return shape_data->source_ref;

    bsg_scene_ref parent_ref = bsg_scene_parent(ref);
    while (!bsg_scene_ref_is_null(parent_ref)) {
	if (bsg_database_source_ref_is_container(
		    bsg_database_source_ref_from_scene(parent_ref)))
	    return parent_ref;
	parent_ref = bsg_scene_parent(parent_ref);
    }

    return ref;
}


void
ged_draw_scene_ref_set_source_ref(bsg_scene_ref ref, bsg_scene_ref source_ref)
{
    ged_draw_shape_state *shape_data =
	ged_draw_shape_state_get_scene_ref(ref);
    if (shape_data)
	shape_data->source_ref = source_ref;
}


static bsg_database_source_stale_reason
_ged_database_source_stale_reason(ged_draw_stale_reason reason)
{
    switch (reason) {
	case GED_DRAW_STALE_SOURCE_CHANGED:
	    return BSG_DATABASE_SOURCE_STALE_SOURCE_CHANGED;
	case GED_DRAW_STALE_VIEW_INPUT_CHANGED:
	    return BSG_DATABASE_SOURCE_STALE_VIEW_INPUT_CHANGED;
	case GED_DRAW_STALE_SETTINGS_CHANGED:
	    return BSG_DATABASE_SOURCE_STALE_SETTINGS_CHANGED;
	case GED_DRAW_STALE_FORCED:
	    return BSG_DATABASE_SOURCE_STALE_FORCED;
	case GED_DRAW_STALE_UPDATE_FAILED:
	    return BSG_DATABASE_SOURCE_STALE_UPDATE_FAILED;
	case GED_DRAW_STALE_NONE:
	default:
	    return BSG_DATABASE_SOURCE_STALE_NONE;
    }
}


const char *
ged_draw_database_source_stale_reason_name(bsg_database_source_stale_reason reason)
{
    switch (reason) {
	case BSG_DATABASE_SOURCE_STALE_SOURCE_CHANGED:
	    return ged_draw_stale_reason_name(GED_DRAW_STALE_SOURCE_CHANGED);
	case BSG_DATABASE_SOURCE_STALE_VIEW_INPUT_CHANGED:
	    return ged_draw_stale_reason_name(GED_DRAW_STALE_VIEW_INPUT_CHANGED);
	case BSG_DATABASE_SOURCE_STALE_SETTINGS_CHANGED:
	    return ged_draw_stale_reason_name(GED_DRAW_STALE_SETTINGS_CHANGED);
	case BSG_DATABASE_SOURCE_STALE_FORCED:
	    return ged_draw_stale_reason_name(GED_DRAW_STALE_FORCED);
	case BSG_DATABASE_SOURCE_STALE_UPDATE_FAILED:
	    return ged_draw_stale_reason_name(GED_DRAW_STALE_UPDATE_FAILED);
	case BSG_DATABASE_SOURCE_STALE_NONE:
	default:
	    return ged_draw_stale_reason_name(GED_DRAW_STALE_NONE);
    }
}


int
ged_draw_database_source_record_has_state(const struct bsg_database_source_record *record)
{
    if (!record)
	return 0;
    return (record->database_path && record->database_path[0]) ||
	record->source_revision != 0 ||
	record->inputs_revision != 0 ||
	record->realized_source_revision != 0 ||
	record->realized_inputs_revision != 0 ||
	record->stale_reason != BSG_DATABASE_SOURCE_STALE_NONE ||
	record->realization_identity != 0;
}


void
ged_draw_scene_ref_database_source_sync(bsg_scene_ref ref,
					const struct ged_draw_source_state *source_data,
					const ged_draw_shape_state *shape_data)
{
    if (bsg_scene_ref_is_null(ref))
	return;

    const struct db_full_path *fp = NULL;
    char *path = NULL;
    bsg_scene_ref source_ref = ged_draw_shape_source_ref(ref);
    struct bsg_database_source_record record = {0};

    (void)bsg_database_source_record_get(
	    bsg_database_source_ref_from_scene(source_ref), &record);

    if (shape_data)
	fp = &shape_data->s_fullpath;
    else if (source_data)
	fp = source_data->fp;

    if (fp)
	path = db_path_to_string(fp);

    record.database_path = path ? path : "";
    int draw_mode = bsg_scene_dmode(ref);
    record.draw_mode = (bsg_draw_mode)draw_mode;
    record.material_policy = fp ? BSG_DATABASE_SOURCE_MATERIAL_DATABASE :
	BSG_DATABASE_SOURCE_MATERIAL_INHERIT;

    if (shape_data) {
	record.source_revision = shape_data->source_revision;
	record.inputs_revision = shape_data->inputs_revision;
	record.realized_source_revision = shape_data->realized_source_revision;
	record.realized_inputs_revision = shape_data->realized_inputs_revision;
	record.stale_reason =
	    _ged_database_source_stale_reason(shape_data->stale_reason);
	record.realization_identity = shape_data->path_hash;
    } else if (source_data) {
	record.source_revision = source_data->source_revision;
	record.inputs_revision = source_data->inputs_revision;
	record.realized_source_revision = source_data->realized_source_revision;
	record.realized_inputs_revision = source_data->realized_inputs_revision;
	record.stale_reason =
	    _ged_database_source_stale_reason(source_data->stale_reason);
    }

    (void)bsg_database_source_record_apply(
	    bsg_database_source_ref_from_scene(source_ref), &record);
    if (!bsg_scene_ref_equal(source_ref, ref)) {
	(void)bsg_database_source_record_apply(
		bsg_database_source_ref_from_scene(ref), &record);
    }

    if (path)
	bu_free(path, "db_path_to_string");
}


void
ged_draw_scene_ref_mark_realization_stale(bsg_scene_ref ref,
					  uint64_t source_revision,
					  uint64_t inputs_revision,
					  ged_draw_stale_reason reason)
{
    ged_draw_shape_state *shape_data = ged_draw_shape_state_get_scene_ref(ref);
    struct ged_draw_source_state *d = shape_data ? shape_data->source_data : NULL;
    ged_draw_stale_reason stale_reason = reason ? reason : GED_DRAW_STALE_SOURCE_CHANGED;

    if (shape_data) {
	shape_data->source_revision = source_revision;
	shape_data->inputs_revision = inputs_revision;
	shape_data->stale_reason = stale_reason;
    }
    if (d) {
	d->source_revision = source_revision;
	d->inputs_revision = inputs_revision;
	d->stale_reason = stale_reason;
    }
    ged_draw_scene_ref_database_source_sync(ref, d, shape_data);
}


void
ged_draw_scene_ref_mark_realization_result(bsg_scene_ref ref,
					   uint64_t source_revision,
					   uint64_t inputs_revision,
					   int failed)
{
    ged_draw_shape_state *shape_data = ged_draw_shape_state_get_scene_ref(ref);
    struct ged_draw_source_state *d = shape_data ? shape_data->source_data : NULL;
    ged_draw_stale_reason stale_reason = failed ? GED_DRAW_STALE_UPDATE_FAILED : GED_DRAW_STALE_NONE;

    if (shape_data) {
	shape_data->source_revision = source_revision;
	shape_data->inputs_revision = inputs_revision;
	shape_data->stale_reason = stale_reason;
	if (!failed) {
	    shape_data->realized_source_revision = source_revision;
	    shape_data->realized_inputs_revision = inputs_revision;
	}
    }
    if (d) {
	d->source_revision = source_revision;
	d->inputs_revision = inputs_revision;
	d->stale_reason = stale_reason;
	if (!failed) {
	    d->realized_source_revision = source_revision;
	    d->realized_inputs_revision = inputs_revision;
	}
    }
    if (failed) {
	ged_draw_scene_ref_database_source_sync(ref, d, shape_data);
	return;
    }
    ged_draw_scene_ref_database_source_sync(ref, d, shape_data);
}


void
ged_draw_source_data_free(void *data)
{
    struct ged_draw_source_state *d = (struct ged_draw_source_state *)data;
    if (!d)
	return;
    if (d->fp) {
	db_free_full_path(d->fp);
	bu_free(d->fp, "ged draw source full path");
	d->fp = NULL;
    }
    if (d->mesh_lod) {
	bsg_mesh_lod_destroy(d->mesh_lod);
	d->mesh_lod = NULL;
    }
    BU_PUT(d, struct ged_draw_source_state);
}


void
ged_draw_scene_ref_source_data_set(bsg_scene_ref ref,
				   struct ged_draw_source_state *data)
{
    if (bsg_scene_ref_is_null(ref) || !data)
	return;
    ged_draw_shape_state *shape_data = ged_draw_shape_state_get_scene_ref(ref);
    if (!shape_data) {
	ged_draw_source_data_free(data);
	return;
    }
    if (shape_data->source_data && shape_data->source_data != data)
	ged_draw_source_data_free(shape_data->source_data);
    shape_data->source_data = data;
    ged_draw_scene_ref_database_source_sync(ref, data, shape_data);
}


int
ged_draw_scene_ref_source_ensure(bsg_scene_ref ref)
{
    return ged_draw_shape_state_get_scene_ref(ref) ? 1 : 0;
}


int
ged_draw_scene_ref_mark_view_inputs_changed(bsg_scene_ref ref)
{
    if (bsg_scene_ref_is_null(ref))
	return 0;

    ged_draw_shape_state *state = ged_draw_shape_state_get_scene_ref(ref);
    struct ged_draw_source_state *d = state ? state->source_data : NULL;
    ged_draw_shape_state *shape_data = ged_draw_shape_state_get_scene_ref(ref);
    if (d) {
	d->inputs_revision++;
	d->stale_reason = GED_DRAW_STALE_VIEW_INPUT_CHANGED;
    }

    if (shape_data) {
	shape_data->inputs_revision++;
	shape_data->stale_reason = GED_DRAW_STALE_VIEW_INPUT_CHANGED;
    }

    ged_draw_scene_ref_database_source_sync(ref, d, shape_data);
    return 1;
}


int
ged_draw_scene_ref_mark_realized_current(bsg_scene_ref ref)
{
    if (bsg_scene_ref_is_null(ref))
	return 0;

    ged_draw_shape_state *state = ged_draw_shape_state_get_scene_ref(ref);
    struct ged_draw_source_state *d = state ? state->source_data : NULL;
    ged_draw_shape_state *shape_data = ged_draw_shape_state_get_scene_ref(ref);

    if (d) {
	d->realized_source_revision = d->source_revision;
	d->realized_inputs_revision = d->inputs_revision;
	d->stale_reason = GED_DRAW_STALE_NONE;
    }
    if (shape_data) {
	shape_data->realized_source_revision = shape_data->source_revision;
	shape_data->realized_inputs_revision = shape_data->inputs_revision;
	shape_data->stale_reason = GED_DRAW_STALE_NONE;
    }
    ged_draw_scene_ref_database_source_sync(ref, d, shape_data);
    return 1;
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
