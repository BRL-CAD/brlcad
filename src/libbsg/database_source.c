/*                  D A T A B A S E _ S O U R C E . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */
/** @file libbsg/database_source.c
 *
 * Public database-source metadata fields.
 */

#include "common.h"

#include <string.h>

#include "bsg/database_source.h"
#include "bsg/group.h"
#include "bsg/separator.h"
#include "field_private.h"
#include "node_private.h"
#include "node_storage_private.h"
#include "object_private.h"


static bsg_node *
_node_from_ref(bsg_database_source_ref ref)
{
    return bsg_object_ref_node(bsg_node_ref_object(ref.node));
}


bsg_type_id
bsg_database_source_type(void)
{
    return bsg_type_register("BSGDatabaseSource", bsg_separator_type());
}


bsg_database_source_ref
bsg_database_source_ref_null(void)
{
    bsg_database_source_ref ref = BSG_DATABASE_SOURCE_REF_NULL_INIT;
    return ref;
}


int
bsg_database_source_ref_is_null(bsg_database_source_ref ref)
{
    return bsg_node_ref_is_null(ref.node);
}


bsg_database_source_ref
bsg_database_source_ref_create(struct bsg_view *v, const char *name)
{
    bsg_database_source_ref ref = BSG_DATABASE_SOURCE_REF_NULL_INIT;
    bsg_separator_ref sep = bsg_separator_ref_create(v, name);
    bsg_node *node = bsg_object_ref_node(bsg_node_ref_object(bsg_separator_ref_as_node(sep)));
    if (!node)
	return ref;
    bsg_node_set_source_flags(node, BSG_SOURCE_CHILD);
    bsg_node_set_object_type(node, bsg_database_source_type());
    ref.node = bsg_separator_ref_as_node(sep);
    return ref;
}


bsg_database_source_ref
bsg_database_source_ref_from_node(bsg_node_ref node)
{
    bsg_database_source_ref ref = BSG_DATABASE_SOURCE_REF_NULL_INIT;
    if (!bsg_node_ref_is_null(node))
	ref.node = node;
    return ref;
}


bsg_database_source_ref
bsg_database_source_ref_from_scene(bsg_scene_ref scene)
{
    return bsg_database_source_ref_from_node(
	    bsg_node_ref_from_object(bsg_object_ref_from_scene_ref(scene)));
}


bsg_node_ref
bsg_database_source_ref_as_node(bsg_database_source_ref ref)
{
    return ref.node;
}


bsg_scene_ref
bsg_database_source_ref_as_scene(bsg_database_source_ref ref)
{
    return bsg_scene_ref_from_object_ref(bsg_node_ref_object(ref.node));
}


bsg_separator_ref
bsg_database_source_ref_as_separator(bsg_database_source_ref ref)
{
    bsg_separator_ref sep = BSG_SEPARATOR_REF_NULL_INIT;
    if (bsg_node_is_a(ref.node, bsg_separator_type()))
	sep.group.node = ref.node;
    return sep;
}


int
bsg_database_source_ref_is_container(bsg_database_source_ref ref)
{
    return bsg_node_is_a(ref.node, bsg_database_source_type());
}


static bsg_field_ref
_field(bsg_database_source_ref ref, bsg_field_id_t id)
{
    return bsg_node_field_ref(_node_from_ref(ref), id);
}


bsg_field_ref
bsg_database_source_ref_database_path_field(bsg_database_source_ref ref)
{
    return _field(ref, BSG_FIELD_DATABASE_SOURCE_PATH);
}


bsg_field_ref
bsg_database_source_ref_draw_mode_field(bsg_database_source_ref ref)
{
    return _field(ref, BSG_FIELD_DATABASE_SOURCE_DRAW_MODE);
}


bsg_field_ref
bsg_database_source_ref_material_policy_field(bsg_database_source_ref ref)
{
    return _field(ref, BSG_FIELD_DATABASE_SOURCE_MATERIAL_POLICY);
}


bsg_field_ref
bsg_database_source_ref_source_revision_field(bsg_database_source_ref ref)
{
    return _field(ref, BSG_FIELD_DATABASE_SOURCE_REVISION);
}


bsg_field_ref
bsg_database_source_ref_inputs_revision_field(bsg_database_source_ref ref)
{
    return _field(ref, BSG_FIELD_DATABASE_SOURCE_INPUTS_REVISION);
}


bsg_field_ref
bsg_database_source_ref_realized_source_revision_field(bsg_database_source_ref ref)
{
    return _field(ref, BSG_FIELD_DATABASE_SOURCE_REALIZED_REVISION);
}


bsg_field_ref
bsg_database_source_ref_realized_inputs_revision_field(bsg_database_source_ref ref)
{
    return _field(ref, BSG_FIELD_DATABASE_SOURCE_REALIZED_INPUTS_REVISION);
}


bsg_field_ref
bsg_database_source_ref_stale_reason_field(bsg_database_source_ref ref)
{
    return _field(ref, BSG_FIELD_DATABASE_SOURCE_STALE_REASON);
}


bsg_field_ref
bsg_database_source_ref_realization_identity_field(bsg_database_source_ref ref)
{
    return _field(ref, BSG_FIELD_DATABASE_SOURCE_REALIZATION_IDENTITY);
}


bsg_field_ref
bsg_database_source_ref_realization_status_field(bsg_database_source_ref ref)
{
    return _field(ref, BSG_FIELD_DATABASE_SOURCE_REALIZATION_STATUS);
}


bsg_field_ref
bsg_database_source_ref_realization_role_flags_field(bsg_database_source_ref ref)
{
    return _field(ref, BSG_FIELD_DATABASE_SOURCE_REALIZATION_ROLE_FLAGS);
}


bsg_field_ref
bsg_database_source_ref_realization_view_dependent_field(bsg_database_source_ref ref)
{
    return _field(ref, BSG_FIELD_DATABASE_SOURCE_REALIZATION_VIEW_DEPENDENT);
}


bsg_field_ref
bsg_database_source_ref_realization_view_scale_field(bsg_database_source_ref ref)
{
    return _field(ref, BSG_FIELD_DATABASE_SOURCE_REALIZATION_VIEW_SCALE);
}


bsg_field_ref
bsg_database_source_ref_realization_bot_threshold_field(bsg_database_source_ref ref)
{
    return _field(ref, BSG_FIELD_DATABASE_SOURCE_REALIZATION_BOT_THRESHOLD);
}


bsg_field_ref
bsg_database_source_ref_realization_curve_scale_field(bsg_database_source_ref ref)
{
    return _field(ref, BSG_FIELD_DATABASE_SOURCE_REALIZATION_CURVE_SCALE);
}


bsg_field_ref
bsg_database_source_ref_realization_point_scale_field(bsg_database_source_ref ref)
{
    return _field(ref, BSG_FIELD_DATABASE_SOURCE_REALIZATION_POINT_SCALE);
}


int
bsg_database_source_ref_set_database_path(bsg_database_source_ref ref, const char *path)
{
    return bsg_field_set_string(bsg_database_source_ref_database_path_field(ref), path);
}


const char *
bsg_database_source_ref_database_path(bsg_database_source_ref ref)
{
    const char *path = NULL;
    if (!bsg_field_get_string(bsg_database_source_ref_database_path_field(ref), &path))
	return "";
    return path ? path : "";
}


int
bsg_database_source_ref_set_draw_mode(bsg_database_source_ref ref, bsg_draw_mode mode)
{
    return bsg_field_set_enum(bsg_database_source_ref_draw_mode_field(ref), (int)mode);
}


bsg_draw_mode
bsg_database_source_ref_draw_mode(bsg_database_source_ref ref)
{
    int mode = BSG_DRAW_MODE_WIRE;
    (void)bsg_field_get_enum(bsg_database_source_ref_draw_mode_field(ref), &mode);
    return (bsg_draw_mode)mode;
}


int
bsg_database_source_ref_set_material_policy(bsg_database_source_ref ref,
					    bsg_database_source_material_policy policy)
{
    return bsg_field_set_enum(bsg_database_source_ref_material_policy_field(ref), (int)policy);
}


bsg_database_source_material_policy
bsg_database_source_ref_material_policy(bsg_database_source_ref ref)
{
    int policy = BSG_DATABASE_SOURCE_MATERIAL_INHERIT;
    (void)bsg_field_get_enum(bsg_database_source_ref_material_policy_field(ref), &policy);
    return (bsg_database_source_material_policy)policy;
}


int
bsg_database_source_ref_set_source_revision(bsg_database_source_ref ref, uint64_t revision)
{
    return bsg_field_set_uint64(bsg_database_source_ref_source_revision_field(ref), revision);
}


uint64_t
bsg_database_source_ref_source_revision(bsg_database_source_ref ref)
{
    uint64_t revision = 0;
    (void)bsg_field_get_uint64(bsg_database_source_ref_source_revision_field(ref), &revision);
    return revision;
}


int
bsg_database_source_ref_set_inputs_revision(bsg_database_source_ref ref, uint64_t revision)
{
    return bsg_field_set_uint64(bsg_database_source_ref_inputs_revision_field(ref), revision);
}


uint64_t
bsg_database_source_ref_inputs_revision(bsg_database_source_ref ref)
{
    uint64_t revision = 0;
    (void)bsg_field_get_uint64(bsg_database_source_ref_inputs_revision_field(ref), &revision);
    return revision;
}


int
bsg_database_source_ref_set_realized_source_revision(bsg_database_source_ref ref, uint64_t revision)
{
    return bsg_field_set_uint64(bsg_database_source_ref_realized_source_revision_field(ref), revision);
}


uint64_t
bsg_database_source_ref_realized_source_revision(bsg_database_source_ref ref)
{
    uint64_t revision = 0;
    (void)bsg_field_get_uint64(bsg_database_source_ref_realized_source_revision_field(ref), &revision);
    return revision;
}


int
bsg_database_source_ref_set_realized_inputs_revision(bsg_database_source_ref ref, uint64_t revision)
{
    return bsg_field_set_uint64(bsg_database_source_ref_realized_inputs_revision_field(ref), revision);
}


uint64_t
bsg_database_source_ref_realized_inputs_revision(bsg_database_source_ref ref)
{
    uint64_t revision = 0;
    (void)bsg_field_get_uint64(bsg_database_source_ref_realized_inputs_revision_field(ref), &revision);
    return revision;
}


int
bsg_database_source_ref_set_stale_reason(bsg_database_source_ref ref,
					 bsg_database_source_stale_reason reason)
{
    return bsg_field_set_enum(bsg_database_source_ref_stale_reason_field(ref), (int)reason);
}


bsg_database_source_stale_reason
bsg_database_source_ref_stale_reason(bsg_database_source_ref ref)
{
    int reason = BSG_DATABASE_SOURCE_STALE_NONE;
    (void)bsg_field_get_enum(bsg_database_source_ref_stale_reason_field(ref), &reason);
    return (bsg_database_source_stale_reason)reason;
}


int
bsg_database_source_ref_set_realization_identity(bsg_database_source_ref ref, uint64_t identity)
{
    return bsg_field_set_uint64(bsg_database_source_ref_realization_identity_field(ref), identity);
}


uint64_t
bsg_database_source_ref_realization_identity(bsg_database_source_ref ref)
{
    uint64_t identity = 0;
    (void)bsg_field_get_uint64(bsg_database_source_ref_realization_identity_field(ref), &identity);
    return identity;
}


int
bsg_database_source_ref_set_realization_status(bsg_database_source_ref ref,
					       bsg_database_source_realization_status status)
{
    return bsg_field_set_enum(bsg_database_source_ref_realization_status_field(ref), (int)status);
}


bsg_database_source_realization_status
bsg_database_source_ref_realization_status(bsg_database_source_ref ref)
{
    int status = BSG_DATABASE_SOURCE_REALIZATION_STALE;
    (void)bsg_field_get_enum(bsg_database_source_ref_realization_status_field(ref), &status);
    return (bsg_database_source_realization_status)status;
}


int
bsg_database_source_ref_set_realization_role_flags(bsg_database_source_ref ref,
						   int role_flags)
{
    return bsg_field_set_int(bsg_database_source_ref_realization_role_flags_field(ref), role_flags);
}


int
bsg_database_source_ref_realization_role_flags(bsg_database_source_ref ref)
{
    int role_flags = BSG_DATABASE_SOURCE_REALIZATION_ROLE_NONE;
    (void)bsg_field_get_int(bsg_database_source_ref_realization_role_flags_field(ref), &role_flags);
    return role_flags;
}


int
bsg_database_source_ref_set_realization_view_policy(bsg_database_source_ref ref,
						    int view_dependent,
						    double view_scale,
						    uint64_t bot_threshold,
						    double curve_scale,
						    double point_scale)
{
    int ok = 1;
    ok = bsg_field_set_bool(bsg_database_source_ref_realization_view_dependent_field(ref),
	    view_dependent ? 1 : 0) && ok;
    ok = bsg_field_set_double(bsg_database_source_ref_realization_view_scale_field(ref),
	    view_scale) && ok;
    ok = bsg_field_set_uint64(bsg_database_source_ref_realization_bot_threshold_field(ref),
	    bot_threshold) && ok;
    ok = bsg_field_set_double(bsg_database_source_ref_realization_curve_scale_field(ref),
	    curve_scale) && ok;
    ok = bsg_field_set_double(bsg_database_source_ref_realization_point_scale_field(ref),
	    point_scale) && ok;
    return ok;
}


int
bsg_database_source_ref_realization_view_dependent(bsg_database_source_ref ref)
{
    int view_dependent = 0;
    (void)bsg_field_get_bool(bsg_database_source_ref_realization_view_dependent_field(ref),
	    &view_dependent);
    return view_dependent ? 1 : 0;
}


double
bsg_database_source_ref_realization_view_scale(bsg_database_source_ref ref)
{
    double view_scale = 0.0;
    (void)bsg_field_get_double(bsg_database_source_ref_realization_view_scale_field(ref),
	    &view_scale);
    return view_scale;
}


uint64_t
bsg_database_source_ref_realization_bot_threshold(bsg_database_source_ref ref)
{
    uint64_t bot_threshold = 0;
    (void)bsg_field_get_uint64(bsg_database_source_ref_realization_bot_threshold_field(ref),
	    &bot_threshold);
    return bot_threshold;
}


double
bsg_database_source_ref_realization_curve_scale(bsg_database_source_ref ref)
{
    double curve_scale = 0.0;
    (void)bsg_field_get_double(bsg_database_source_ref_realization_curve_scale_field(ref),
	    &curve_scale);
    return curve_scale;
}


double
bsg_database_source_ref_realization_point_scale(bsg_database_source_ref ref)
{
    double point_scale = 0.0;
    (void)bsg_field_get_double(bsg_database_source_ref_realization_point_scale_field(ref),
	    &point_scale);
    return point_scale;
}


int
bsg_database_source_ref_is_stale(bsg_database_source_ref ref)
{
    return bsg_database_source_ref_stale_reason(ref) != BSG_DATABASE_SOURCE_STALE_NONE ||
	bsg_database_source_ref_source_revision(ref) !=
	bsg_database_source_ref_realized_source_revision(ref) ||
	bsg_database_source_ref_inputs_revision(ref) !=
	bsg_database_source_ref_realized_inputs_revision(ref);
}


int
bsg_database_source_ref_mark_realized_current(bsg_database_source_ref ref)
{
    if (bsg_database_source_ref_is_null(ref))
	return 0;
    int ok = 1;
    ok = bsg_database_source_ref_set_realized_source_revision(ref,
	    bsg_database_source_ref_source_revision(ref)) && ok;
    ok = bsg_database_source_ref_set_realized_inputs_revision(ref,
	    bsg_database_source_ref_inputs_revision(ref)) && ok;
    ok = bsg_database_source_ref_set_stale_reason(ref,
	    BSG_DATABASE_SOURCE_STALE_NONE) && ok;
    return ok;
}


int
bsg_database_source_record_get(bsg_database_source_ref ref,
			       struct bsg_database_source_record *record)
{
    if (bsg_database_source_ref_is_null(ref) || !record)
	return 0;
    memset(record, 0, sizeof(*record));
    record->database_path = bsg_database_source_ref_database_path(ref);
    record->draw_mode = bsg_database_source_ref_draw_mode(ref);
    record->material_policy = bsg_database_source_ref_material_policy(ref);
    record->source_revision = bsg_database_source_ref_source_revision(ref);
    record->inputs_revision = bsg_database_source_ref_inputs_revision(ref);
    record->realized_source_revision = bsg_database_source_ref_realized_source_revision(ref);
    record->realized_inputs_revision = bsg_database_source_ref_realized_inputs_revision(ref);
    record->stale_reason = bsg_database_source_ref_stale_reason(ref);
    record->realization_identity = bsg_database_source_ref_realization_identity(ref);
    record->realization_status = bsg_database_source_ref_realization_status(ref);
    record->realization_role_flags = bsg_database_source_ref_realization_role_flags(ref);
    record->realization_view_dependent =
	bsg_database_source_ref_realization_view_dependent(ref);
    record->realization_view_scale =
	bsg_database_source_ref_realization_view_scale(ref);
    record->realization_bot_threshold =
	bsg_database_source_ref_realization_bot_threshold(ref);
    record->realization_curve_scale =
	bsg_database_source_ref_realization_curve_scale(ref);
    record->realization_point_scale =
	bsg_database_source_ref_realization_point_scale(ref);
    return 1;
}


int
bsg_database_source_record_apply(bsg_database_source_ref ref,
				 const struct bsg_database_source_record *record)
{
    if (bsg_database_source_ref_is_null(ref) || !record)
	return 0;
    int ok = 1;
    ok = bsg_database_source_ref_set_database_path(ref, record->database_path) && ok;
    ok = bsg_database_source_ref_set_draw_mode(ref, record->draw_mode) && ok;
    ok = bsg_database_source_ref_set_material_policy(ref, record->material_policy) && ok;
    ok = bsg_database_source_ref_set_source_revision(ref, record->source_revision) && ok;
    ok = bsg_database_source_ref_set_inputs_revision(ref, record->inputs_revision) && ok;
    ok = bsg_database_source_ref_set_realized_source_revision(ref,
	    record->realized_source_revision) && ok;
    ok = bsg_database_source_ref_set_realized_inputs_revision(ref,
	    record->realized_inputs_revision) && ok;
    ok = bsg_database_source_ref_set_stale_reason(ref, record->stale_reason) && ok;
    ok = bsg_database_source_ref_set_realization_identity(ref,
	    record->realization_identity) && ok;
    ok = bsg_database_source_ref_set_realization_status(ref,
	    record->realization_status) && ok;
    ok = bsg_database_source_ref_set_realization_role_flags(ref,
	    record->realization_role_flags) && ok;
    ok = bsg_database_source_ref_set_realization_view_policy(ref,
	    record->realization_view_dependent,
	    record->realization_view_scale,
	    record->realization_bot_threshold,
	    record->realization_curve_scale,
	    record->realization_point_scale) && ok;
    return ok;
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
