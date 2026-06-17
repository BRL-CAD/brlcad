/*                  D A T A B A S E _ S O U R C E . H
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this file; see the file named COPYING for more information.
 */
/** @addtogroup libbsg
 *
 * @brief
 * Public database-source node metadata.
 *
 * Database-source refs are semantic bindings over node fields.  New producers
 * should create database-source separator nodes and place realized typed
 * geometry below them.  Existing producers may bind the same source fields to
 * their current source node while migrating away from private realization
 * storage.
 */
/** @{ */
/* @file bsg/database_source.h */

#ifndef BSG_DATABASE_SOURCE_H
#define BSG_DATABASE_SOURCE_H

#include "common.h"
#include <stdint.h>
#include "bsg/defines.h"
#include "bsg/draw_intent.h"
#include "bsg/field.h"
#include "bsg/node.h"
#include "bsg/object.h"
#include "bsg/scene_builder.h"

__BEGIN_DECLS

typedef enum bsg_database_source_material_policy {
    BSG_DATABASE_SOURCE_MATERIAL_INHERIT = 0,
    BSG_DATABASE_SOURCE_MATERIAL_DATABASE,
    BSG_DATABASE_SOURCE_MATERIAL_OVERRIDE
} bsg_database_source_material_policy;

typedef enum bsg_database_source_stale_reason {
    BSG_DATABASE_SOURCE_STALE_NONE = 0,
    BSG_DATABASE_SOURCE_STALE_SOURCE_CHANGED,
    BSG_DATABASE_SOURCE_STALE_VIEW_INPUT_CHANGED,
    BSG_DATABASE_SOURCE_STALE_SETTINGS_CHANGED,
    BSG_DATABASE_SOURCE_STALE_FORCED,
    BSG_DATABASE_SOURCE_STALE_UPDATE_FAILED
} bsg_database_source_stale_reason;

typedef enum bsg_database_source_realization_status {
    BSG_DATABASE_SOURCE_REALIZATION_STALE = 0,
    BSG_DATABASE_SOURCE_REALIZATION_CURRENT
} bsg_database_source_realization_status;

typedef enum bsg_database_source_realization_role {
    BSG_DATABASE_SOURCE_REALIZATION_ROLE_NONE = 0,
    BSG_DATABASE_SOURCE_REALIZATION_ROLE_CSG = 1,
    BSG_DATABASE_SOURCE_REALIZATION_ROLE_MESH = 2
} bsg_database_source_realization_role;

struct bsg_database_source_record {
    const char *database_path;          /**< borrowed from the source field */
    bsg_draw_mode draw_mode;
    bsg_database_source_material_policy material_policy;
    uint64_t source_revision;
    uint64_t inputs_revision;
    uint64_t realized_source_revision;
    uint64_t realized_inputs_revision;
    bsg_database_source_stale_reason stale_reason;
    uint64_t realization_identity;
    bsg_database_source_realization_status realization_status;
    int realization_role_flags;
    int realization_view_dependent;
    double realization_view_scale;
    uint64_t realization_bot_threshold;
    double realization_curve_scale;
    double realization_point_scale;
};

BSG_EXPORT extern bsg_type_id bsg_database_source_type(void);

BSG_EXPORT extern bsg_database_source_ref bsg_database_source_ref_null(void);
BSG_EXPORT extern int bsg_database_source_ref_is_null(bsg_database_source_ref ref);
BSG_EXPORT extern bsg_database_source_ref bsg_database_source_ref_create(struct bsg_view *v,
									  const char *name);
BSG_EXPORT extern bsg_database_source_ref bsg_database_source_ref_from_node(bsg_node_ref node);
BSG_EXPORT extern bsg_database_source_ref bsg_database_source_ref_from_scene(bsg_scene_ref scene);
BSG_EXPORT extern bsg_node_ref bsg_database_source_ref_as_node(bsg_database_source_ref ref);
BSG_EXPORT extern bsg_scene_ref bsg_database_source_ref_as_scene(bsg_database_source_ref ref);
BSG_EXPORT extern bsg_separator_ref bsg_database_source_ref_as_separator(bsg_database_source_ref ref);
BSG_EXPORT extern int bsg_database_source_ref_is_container(bsg_database_source_ref ref);

BSG_EXPORT extern bsg_field_ref bsg_database_source_ref_database_path_field(bsg_database_source_ref ref);
BSG_EXPORT extern bsg_field_ref bsg_database_source_ref_draw_mode_field(bsg_database_source_ref ref);
BSG_EXPORT extern bsg_field_ref bsg_database_source_ref_material_policy_field(bsg_database_source_ref ref);
BSG_EXPORT extern bsg_field_ref bsg_database_source_ref_source_revision_field(bsg_database_source_ref ref);
BSG_EXPORT extern bsg_field_ref bsg_database_source_ref_inputs_revision_field(bsg_database_source_ref ref);
BSG_EXPORT extern bsg_field_ref bsg_database_source_ref_realized_source_revision_field(bsg_database_source_ref ref);
BSG_EXPORT extern bsg_field_ref bsg_database_source_ref_realized_inputs_revision_field(bsg_database_source_ref ref);
BSG_EXPORT extern bsg_field_ref bsg_database_source_ref_stale_reason_field(bsg_database_source_ref ref);
BSG_EXPORT extern bsg_field_ref bsg_database_source_ref_realization_identity_field(bsg_database_source_ref ref);
BSG_EXPORT extern bsg_field_ref bsg_database_source_ref_realization_status_field(bsg_database_source_ref ref);
BSG_EXPORT extern bsg_field_ref bsg_database_source_ref_realization_role_flags_field(bsg_database_source_ref ref);
BSG_EXPORT extern bsg_field_ref bsg_database_source_ref_realization_view_dependent_field(bsg_database_source_ref ref);
BSG_EXPORT extern bsg_field_ref bsg_database_source_ref_realization_view_scale_field(bsg_database_source_ref ref);
BSG_EXPORT extern bsg_field_ref bsg_database_source_ref_realization_bot_threshold_field(bsg_database_source_ref ref);
BSG_EXPORT extern bsg_field_ref bsg_database_source_ref_realization_curve_scale_field(bsg_database_source_ref ref);
BSG_EXPORT extern bsg_field_ref bsg_database_source_ref_realization_point_scale_field(bsg_database_source_ref ref);

BSG_EXPORT extern int bsg_database_source_ref_set_database_path(bsg_database_source_ref ref,
							       const char *path);
BSG_EXPORT extern const char *bsg_database_source_ref_database_path(bsg_database_source_ref ref);
BSG_EXPORT extern int bsg_database_source_ref_set_draw_mode(bsg_database_source_ref ref,
							    bsg_draw_mode mode);
BSG_EXPORT extern bsg_draw_mode bsg_database_source_ref_draw_mode(bsg_database_source_ref ref);
BSG_EXPORT extern int bsg_database_source_ref_set_material_policy(bsg_database_source_ref ref,
								  bsg_database_source_material_policy policy);
BSG_EXPORT extern bsg_database_source_material_policy
bsg_database_source_ref_material_policy(bsg_database_source_ref ref);
BSG_EXPORT extern int bsg_database_source_ref_set_source_revision(bsg_database_source_ref ref,
								  uint64_t revision);
BSG_EXPORT extern uint64_t bsg_database_source_ref_source_revision(bsg_database_source_ref ref);
BSG_EXPORT extern int bsg_database_source_ref_set_inputs_revision(bsg_database_source_ref ref,
								  uint64_t revision);
BSG_EXPORT extern uint64_t bsg_database_source_ref_inputs_revision(bsg_database_source_ref ref);
BSG_EXPORT extern int bsg_database_source_ref_set_realized_source_revision(bsg_database_source_ref ref,
									   uint64_t revision);
BSG_EXPORT extern uint64_t bsg_database_source_ref_realized_source_revision(bsg_database_source_ref ref);
BSG_EXPORT extern int bsg_database_source_ref_set_realized_inputs_revision(bsg_database_source_ref ref,
									   uint64_t revision);
BSG_EXPORT extern uint64_t bsg_database_source_ref_realized_inputs_revision(bsg_database_source_ref ref);
BSG_EXPORT extern int bsg_database_source_ref_set_stale_reason(bsg_database_source_ref ref,
							       bsg_database_source_stale_reason reason);
BSG_EXPORT extern bsg_database_source_stale_reason
bsg_database_source_ref_stale_reason(bsg_database_source_ref ref);
BSG_EXPORT extern int bsg_database_source_ref_set_realization_identity(bsg_database_source_ref ref,
								       uint64_t identity);
BSG_EXPORT extern uint64_t bsg_database_source_ref_realization_identity(bsg_database_source_ref ref);
BSG_EXPORT extern int bsg_database_source_ref_set_realization_status(bsg_database_source_ref ref,
								     bsg_database_source_realization_status status);
BSG_EXPORT extern bsg_database_source_realization_status
bsg_database_source_ref_realization_status(bsg_database_source_ref ref);
BSG_EXPORT extern int bsg_database_source_ref_set_realization_role_flags(bsg_database_source_ref ref,
									 int role_flags);
BSG_EXPORT extern int bsg_database_source_ref_realization_role_flags(bsg_database_source_ref ref);
BSG_EXPORT extern int bsg_database_source_ref_set_realization_view_policy(bsg_database_source_ref ref,
									  int view_dependent,
									  double view_scale,
									  uint64_t bot_threshold,
									  double curve_scale,
									  double point_scale);
BSG_EXPORT extern int bsg_database_source_ref_realization_view_dependent(bsg_database_source_ref ref);
BSG_EXPORT extern double bsg_database_source_ref_realization_view_scale(bsg_database_source_ref ref);
BSG_EXPORT extern uint64_t bsg_database_source_ref_realization_bot_threshold(bsg_database_source_ref ref);
BSG_EXPORT extern double bsg_database_source_ref_realization_curve_scale(bsg_database_source_ref ref);
BSG_EXPORT extern double bsg_database_source_ref_realization_point_scale(bsg_database_source_ref ref);

BSG_EXPORT extern int bsg_database_source_ref_is_stale(bsg_database_source_ref ref);
BSG_EXPORT extern int bsg_database_source_ref_mark_realized_current(bsg_database_source_ref ref);
BSG_EXPORT extern int bsg_database_source_record_get(bsg_database_source_ref ref,
						     struct bsg_database_source_record *record);
BSG_EXPORT extern int bsg_database_source_record_apply(bsg_database_source_ref ref,
						       const struct bsg_database_source_record *record);

__END_DECLS

#endif /* BSG_DATABASE_SOURCE_H */

/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
