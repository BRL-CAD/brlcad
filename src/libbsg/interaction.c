/*                 I N T E R A C T I O N . C
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
/** @file libbsg/interaction.c */

#include "common.h"

#include <inttypes.h>
#include <string.h>

#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "bsg/feature.h"
#include "bsg/interaction.h"
#include "bsg/measure.h"
#include "bsg/pick.h"
#include "bsg/selection.h"
#include "bsg/snap_action.h"


struct bsg_interaction_result *
bsg_interaction_result_create(void)
{
    struct bsg_interaction_result *result;
    BU_GET(result, struct bsg_interaction_result);
    bu_ptbl_init(&result->records, 8, "bsg_interaction_result records");
    return result;
}


void
bsg_interaction_record_free(struct bsg_interaction_record *record)
{
    if (!record)
	return;
    bu_vls_free(&record->source_path);
    bu_vls_free(&record->instance_path);
    BU_PUT(record, struct bsg_interaction_record);
}


void
bsg_interaction_result_free(struct bsg_interaction_result *result)
{
    if (!result)
	return;
    for (size_t i = 0; i < BU_PTBL_LEN(&result->records); i++) {
	struct bsg_interaction_record *record =
	    (struct bsg_interaction_record *)BU_PTBL_GET(&result->records, i);
	bsg_interaction_record_free(record);
    }
    bu_ptbl_free(&result->records);
    BU_PUT(result, struct bsg_interaction_result);
}


size_t
bsg_interaction_result_count(const struct bsg_interaction_result *result)
{
    if (!result)
	return 0;
    return BU_PTBL_LEN(&result->records);
}


struct bsg_interaction_record *
bsg_interaction_result_get(const struct bsg_interaction_result *result, size_t idx)
{
    if (!result || idx >= BU_PTBL_LEN(&result->records))
	return NULL;
    return (struct bsg_interaction_record *)BU_PTBL_GET(&result->records, idx);
}


struct bsg_interaction_record *
bsg_interaction_record_create(bsg_interaction_kind kind)
{
    struct bsg_interaction_record *record;
    BU_GET(record, struct bsg_interaction_record);
    memset(record, 0, sizeof(*record));
    record->kind = kind;
    record->feature = (bsg_feature_ref)BSG_FEATURE_REF_NULL_INIT;
    BU_VLS_INIT(&record->source_path);
    BU_VLS_INIT(&record->instance_path);
    record->distance = -1.0;
    record->projection = 0.0;
    record->normal_alignment = 0.0;
    record->primitive_id = -1;
    record->subelement_id = -1;
    record->preview_op = BSG_EDIT_PREVIEW_BEGIN;
    return record;
}


struct bsg_interaction_record *
bsg_interaction_record_create_ref(struct bsg_view *v,
				  bsg_interaction_kind kind,
				  bsg_feature_ref feature,
				  const char *source_path,
				  const char *instance_path)
{
    struct bsg_interaction_record *record = bsg_interaction_record_create(kind);
    if (!record)
	return NULL;
    record->view = v;
    record->feature = feature;
    if (source_path && source_path[0])
	bu_vls_sprintf(&record->source_path, "%s", source_path);
    if (instance_path && instance_path[0])
	bu_vls_sprintf(&record->instance_path, "%s", instance_path);
    record->valid = (feature.token || bsg_interaction_record_path(record)) ? 1 : 0;
    return record;
}


struct bsg_interaction_record *
bsg_interaction_record_clone(const struct bsg_interaction_record *src)
{
    if (!src)
	return NULL;
    struct bsg_interaction_record *record = bsg_interaction_record_create(src->kind);
    if (!record)
	return NULL;
    record->view = src->view;
    record->feature = src->feature;
    VMOVE(record->point, src->point);
    VMOVE(record->point_b, src->point_b);
    record->screen_x = src->screen_x;
    record->screen_y = src->screen_y;
    record->distance = src->distance;
    record->projection = src->projection;
    record->normal_alignment = src->normal_alignment;
    record->primitive_id = src->primitive_id;
    record->subelement_id = src->subelement_id;
    record->snap_kind = src->snap_kind;
    record->preview_op = src->preview_op;
    record->valid = src->valid;
    bu_vls_sprintf(&record->source_path, "%s", bu_vls_cstr(&src->source_path));
    bu_vls_sprintf(&record->instance_path, "%s", bu_vls_cstr(&src->instance_path));
    return record;
}


static const char *
_skip_path_lead_slash(const char *path)
{
    if (!path)
	return NULL;
    while (*path == '/')
	path++;
    return path;
}


static int
_interaction_paths_equal(const char *a, const char *b)
{
    a = _skip_path_lead_slash(a);
    b = _skip_path_lead_slash(b);
    if (!a || !b || !a[0] || !b[0])
	return 0;
    return BU_STR_EQUAL(a, b);
}


static const char *
_interaction_kind_name(bsg_interaction_kind kind)
{
    switch (kind) {
	case BSG_INTERACTION_PICK_HIT: return "pick-hit";
	case BSG_INTERACTION_SELECTED_PATH: return "selected-path";
	case BSG_INTERACTION_HIGHLIGHTED_REF: return "highlighted-ref";
	case BSG_INTERACTION_SNAP_CANDIDATE: return "snap-candidate";
	case BSG_INTERACTION_MEASURE_CANDIDATE: return "measure-candidate";
	case BSG_INTERACTION_EDIT_HANDLE: return "edit-handle";
	case BSG_INTERACTION_EDIT_PREVIEW: return "edit-preview";
	default: return "unknown";
    }
}


static const char *
_preview_op_name(bsg_edit_preview_op op)
{
    switch (op) {
	case BSG_EDIT_PREVIEW_BEGIN: return "begin";
	case BSG_EDIT_PREVIEW_UPDATE: return "update";
	case BSG_EDIT_PREVIEW_COMMIT: return "commit";
	case BSG_EDIT_PREVIEW_CANCEL: return "cancel";
	case BSG_EDIT_PREVIEW_REPLACE_SOURCE: return "replace-source";
	case BSG_EDIT_PREVIEW_DISCARD: return "discard";
	default: return "unknown";
    }
}


void
bsg_interaction_result_append(struct bsg_interaction_result *result,
			      struct bsg_interaction_record *record)
{
    if (!result || !record)
	return;
    bu_ptbl_ins(&result->records, (long *)record);
}


const char *
bsg_interaction_record_path(const struct bsg_interaction_record *record)
{
    if (!record)
	return NULL;
    const char *path = bu_vls_cstr(&record->source_path);
    if (path && path[0])
	return path;
    path = bu_vls_cstr(&record->instance_path);
    if (path && path[0])
	return path;
    return NULL;
}


int
bsg_interaction_record_equal(const struct bsg_interaction_record *a,
			     const struct bsg_interaction_record *b)
{
    if (!a || !b)
	return 0;
    if (a->kind != b->kind)
	return 0;
    if (a->feature.token && b->feature.token && a->feature.token == b->feature.token)
	return 1;
    const char *ap = bsg_interaction_record_path(a);
    const char *bp = bsg_interaction_record_path(b);
    if (_interaction_paths_equal(ap, bp))
	return 1;
    return 0;
}


void
bsg_interaction_record_serialize(const struct bsg_interaction_record *record,
				 struct bu_vls *out)
{
    if (!out)
	return;
    if (!record) {
	bu_vls_printf(out, "{\"valid\":0}");
	return;
    }
    const char *path = bsg_interaction_record_path(record);
    bu_vls_printf(out,
	    "{\"kind\":\"%s\",\"valid\":%d,\"path\":\"%s\",\"feature\":%" PRIuPTR ",\"point\":[%.17g,%.17g,%.17g]",
	    _interaction_kind_name(record->kind),
	    record->valid,
	    path ? path : "",
	    (uintptr_t)record->feature.token,
	    V3ARGS(record->point));
    if (record->kind == BSG_INTERACTION_EDIT_PREVIEW)
	bu_vls_printf(out, ",\"preview\":\"%s\"", _preview_op_name(record->preview_op));
    if (record->kind == BSG_INTERACTION_SNAP_CANDIDATE)
	bu_vls_printf(out, ",\"snap_kind\":%d", (int)record->snap_kind);
    if (record->kind == BSG_INTERACTION_MEASURE_CANDIDATE)
	bu_vls_printf(out, ",\"point_b\":[%.17g,%.17g,%.17g],\"distance\":%.17g",
		V3ARGS(record->point_b), record->distance);
    bu_vls_printf(out, "}");
}


void
bsg_interaction_result_serialize(const struct bsg_interaction_result *result,
				 struct bu_vls *out)
{
    if (!out)
	return;
    bu_vls_printf(out, "[");
    size_t cnt = bsg_interaction_result_count(result);
    for (size_t i = 0; i < cnt; i++) {
	if (i)
	    bu_vls_printf(out, ",");
	bsg_interaction_record_serialize(bsg_interaction_result_get(result, i), out);
    }
    bu_vls_printf(out, "]");
}


struct bsg_interaction_record *
bsg_interaction_from_pick_record(const struct bsg_pick_record *pick)
{
    if (!pick)
	return NULL;
    struct bsg_interaction_record *record =
	bsg_interaction_record_create(BSG_INTERACTION_PICK_HIT);
    if (!record)
	return NULL;
    record->view = pick->pr_view;
    record->feature = pick->pr_feature;
    record->screen_x = pick->pr_screen_x;
    record->screen_y = pick->pr_screen_y;
    record->distance = pick->pr_hit_dist;
    record->primitive_id = pick->pr_primitive_id;
    record->subelement_id = pick->pr_subelement_id;
    record->valid = pick->pr_valid;
    bu_vls_sprintf(&record->source_path, "%s", bu_vls_cstr(&pick->pr_source_path));
    bu_vls_sprintf(&record->instance_path, "%s", bu_vls_cstr(&pick->pr_instance_path));
    return record;
}


struct bsg_interaction_result *
bsg_interaction_from_pick_result(const struct bsg_pick_result *pick_result)
{
    struct bsg_interaction_result *result = bsg_interaction_result_create();
    if (!result)
	return NULL;
    if (!pick_result)
	return result;
    for (size_t i = 0; i < bsg_pick_result_count(pick_result); i++) {
	struct bsg_pick_record *pick = bsg_pick_result_get(pick_result, i);
	struct bsg_interaction_record *record = bsg_interaction_from_pick_record(pick);
	if (record)
	    bsg_interaction_result_append(result, record);
    }
    return result;
}


struct bsg_interaction_record *
bsg_interaction_from_snap_candidate(struct bsg_view *v,
				    const struct bsg_snap_candidate *candidate)
{
    if (!candidate)
	return NULL;
    struct bsg_interaction_record *record =
	bsg_interaction_record_create(BSG_INTERACTION_SNAP_CANDIDATE);
    if (!record)
	return NULL;
    record->view = v;
    record->feature = candidate->sc_feature;
    VMOVE(record->point, candidate->sc_point);
    record->distance = candidate->sc_distance;
    record->snap_kind = candidate->sc_kind;
    record->valid = !candidate->sc_stale;
    const char *path = bu_vls_cstr(&candidate->sc_source_path);
    if (path && path[0])
	bu_vls_sprintf(&record->source_path, "%s", path);
    return record;
}


struct bsg_interaction_result *
bsg_interaction_from_snap_result(struct bsg_view *v,
				 const struct bsg_snap_result *snap_result)
{
    struct bsg_interaction_result *result = bsg_interaction_result_create();
    if (!result)
	return NULL;
    if (!snap_result)
	return result;
    for (size_t i = 0; i < snap_result->sr_cnt; i++) {
	struct bsg_interaction_record *record =
	    bsg_interaction_from_snap_candidate(v, &snap_result->sr_candidates[i]);
	if (record)
	    bsg_interaction_result_append(result, record);
    }
    return result;
}


struct bsg_interaction_record *
bsg_interaction_from_measure_result(struct bsg_view *v,
				    const point_t a,
				    const point_t b,
				    const struct bsg_measure_result *measure)
{
    if (!measure)
	return NULL;
    struct bsg_interaction_record *record =
	bsg_interaction_record_create(BSG_INTERACTION_MEASURE_CANDIDATE);
    if (!record)
	return NULL;
    record->view = v;
    VMOVE(record->point, a);
    VMOVE(record->point_b, b);
    record->distance = measure->mr_distance;
    record->projection = measure->mr_projection;
    record->normal_alignment = measure->mr_normal_alignment;
    record->valid = measure->mr_valid;
    return record;
}


struct bsg_interaction_record *
bsg_interaction_edit_preview_record(struct bsg_view *v,
				    bsg_feature_ref feature,
				    bsg_edit_preview_op op,
				    const char *source_path)
{
    struct bsg_interaction_record *record =
	bsg_interaction_record_create(BSG_INTERACTION_EDIT_PREVIEW);
    if (!record)
	return NULL;
    record->view = v;
    record->feature = feature;
    record->preview_op = op;
    record->valid = feature.token ? 1 : 0;
    if (source_path)
	bu_vls_sprintf(&record->source_path, "%s", source_path);
    return record;
}


void
bsg_interaction_selection_apply(struct bsg_selection *selection,
				const struct bsg_interaction_result *result,
				bsg_interaction_apply_op op)
{
    if (!selection || !result)
	return;
    if (op == BSG_INTERACTION_APPLY_SET)
	bsg_selection_clear(selection);
    for (size_t i = 0; i < bsg_interaction_result_count(result); i++) {
	const struct bsg_interaction_record *record =
	    bsg_interaction_result_get(result, i);
	if (op == BSG_INTERACTION_APPLY_REMOVE)
	    bsg_selection_remove_record(selection, record);
	else
	    bsg_selection_add_record(selection, record);
    }
}
