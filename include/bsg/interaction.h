/*                 I N T E R A C T I O N . H
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
/** @addtogroup libbsg
 *
 * @brief Unified interaction records for pick, selection, snap, measure,
 * highlight, and edit-feature workflows.
 */
/** @{ */
/* @file bsg/interaction.h */

#ifndef BSG_INTERACTION_H
#define BSG_INTERACTION_H

#include "common.h"
#include "bu/ptbl.h"
#include "bu/vls.h"
#include "vmath.h"
#include "bsg/defines.h"
#include "bsg/feature.h"
#include "bsg/snap_action.h"

__BEGIN_DECLS

struct bsg_measure_result;
struct bsg_pick_record;
struct bsg_pick_result;
struct bsg_selection;
struct bsg_snap_candidate;
struct bsg_snap_result;

typedef enum {
    BSG_INTERACTION_PICK_HIT = 1,
    BSG_INTERACTION_SELECTED_PATH,
    BSG_INTERACTION_HIGHLIGHTED_REF,
    BSG_INTERACTION_SNAP_CANDIDATE,
    BSG_INTERACTION_MEASURE_CANDIDATE,
    BSG_INTERACTION_EDIT_HANDLE,
    BSG_INTERACTION_EDIT_PREVIEW
} bsg_interaction_kind;

typedef enum {
    BSG_INTERACTION_APPLY_SET = 0,
    BSG_INTERACTION_APPLY_ADD = 1,
    BSG_INTERACTION_APPLY_REMOVE = 2
} bsg_interaction_apply_op;

typedef enum {
    BSG_EDIT_PREVIEW_BEGIN = 0,
    BSG_EDIT_PREVIEW_UPDATE,
    BSG_EDIT_PREVIEW_COMMIT,
    BSG_EDIT_PREVIEW_CANCEL,
    BSG_EDIT_PREVIEW_REPLACE_SOURCE,
    BSG_EDIT_PREVIEW_DISCARD
} bsg_edit_preview_op;

struct bsg_interaction_record {
    bsg_interaction_kind kind;
    struct bsg_view *view;
    bsg_feature_ref feature;
    struct bu_vls source_path;
    struct bu_vls instance_path;
    point_t point;
    point_t point_b;
    int screen_x;
    int screen_y;
    fastf_t distance;
    fastf_t projection;
    fastf_t normal_alignment;
    int primitive_id;
    int subelement_id;
    bsg_snap_kind snap_kind;
    bsg_edit_preview_op preview_op;
    int valid;
};

struct bsg_interaction_result {
    struct bu_ptbl records;
};

BSG_EXPORT extern struct bsg_interaction_result *
bsg_interaction_result_create(void);

BSG_EXPORT extern void
bsg_interaction_result_free(struct bsg_interaction_result *result);

BSG_EXPORT extern size_t
bsg_interaction_result_count(const struct bsg_interaction_result *result);

BSG_EXPORT extern struct bsg_interaction_record *
bsg_interaction_result_get(const struct bsg_interaction_result *result, size_t idx);

BSG_EXPORT extern struct bsg_interaction_record *
bsg_interaction_record_create(bsg_interaction_kind kind);

BSG_EXPORT extern struct bsg_interaction_record *
bsg_interaction_record_create_ref(struct bsg_view *v,
				  bsg_interaction_kind kind,
				  bsg_feature_ref feature,
				  const char *source_path,
				  const char *instance_path);

BSG_EXPORT extern struct bsg_interaction_record *
bsg_interaction_record_clone(const struct bsg_interaction_record *src);

BSG_EXPORT extern void
bsg_interaction_record_free(struct bsg_interaction_record *record);

BSG_EXPORT extern void
bsg_interaction_result_append(struct bsg_interaction_result *result,
			      struct bsg_interaction_record *record);

BSG_EXPORT extern const char *
bsg_interaction_record_path(const struct bsg_interaction_record *record);

BSG_EXPORT extern int
bsg_interaction_record_equal(const struct bsg_interaction_record *a,
			     const struct bsg_interaction_record *b);

BSG_EXPORT extern void
bsg_interaction_record_serialize(const struct bsg_interaction_record *record,
				 struct bu_vls *out);

BSG_EXPORT extern void
bsg_interaction_result_serialize(const struct bsg_interaction_result *result,
				 struct bu_vls *out);

BSG_EXPORT extern struct bsg_interaction_record *
bsg_interaction_from_pick_record(const struct bsg_pick_record *pick);

BSG_EXPORT extern struct bsg_interaction_result *
bsg_interaction_from_pick_result(const struct bsg_pick_result *pick_result);

BSG_EXPORT extern struct bsg_interaction_record *
bsg_interaction_from_snap_candidate(struct bsg_view *v,
				    const struct bsg_snap_candidate *candidate);

BSG_EXPORT extern struct bsg_interaction_result *
bsg_interaction_from_snap_result(struct bsg_view *v,
				 const struct bsg_snap_result *snap_result);

BSG_EXPORT extern struct bsg_interaction_record *
bsg_interaction_from_measure_result(struct bsg_view *v,
				    const point_t a,
				    const point_t b,
				    const struct bsg_measure_result *measure);

BSG_EXPORT extern struct bsg_interaction_record *
bsg_interaction_edit_preview_record(struct bsg_view *v,
				    bsg_feature_ref feature,
				    bsg_edit_preview_op op,
				    const char *source_path);

BSG_EXPORT extern void
bsg_interaction_selection_apply(struct bsg_selection *selection,
				const struct bsg_interaction_result *result,
				bsg_interaction_apply_op op);

__END_DECLS

#endif /* BSG_INTERACTION_H */

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
