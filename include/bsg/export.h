/*                    E X P O R T . H
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
 * @brief Semantic export/report records over resolved BSG render batches.
 *
 * Export records answer "what is visible/exportable/reportable" without
 * exposing draw-tree layout.  They are populated from `bsg_render_item`
 * records and carry source identity, transform, geometry kind, realization
 * state, appearance, selection, bounds, and LoD state.
 */
/** @{ */
/* @file bsg/export.h */

#ifndef BSG_EXPORT_H
#define BSG_EXPORT_H

#include "common.h"

#include "vmath.h"
#include "bu/ptbl.h"
#include "bu/vls.h"
#include "bsg/defines.h"
#include "bsg/render_item.h"

__BEGIN_DECLS

typedef enum bsg_export_record_role {
    BSG_EXPORT_RECORD_SCENE       = 0x01u,
    BSG_EXPORT_RECORD_BOUNDS      = 0x04u,
    BSG_EXPORT_RECORD_LABEL       = 0x08u,
    BSG_EXPORT_RECORD_ANNOTATION  = 0x10u,
    BSG_EXPORT_RECORD_DIAGNOSTIC  = 0x20u,
    BSG_EXPORT_RECORD_GEOMETRY    = 0x40u
} bsg_export_record_role;

#define BSG_EXPORT_QUERY_VISIBLE_ONLY 0x0001u
#define BSG_EXPORT_QUERY_DB_OBJECTS   0x0002u
#define BSG_EXPORT_QUERY_VIEW_OBJECTS 0x0004u
#define BSG_EXPORT_QUERY_LOCAL_ONLY   0x0008u

#define BSG_EXPORT_DRAW_MODE_ANY      (-1)

struct bsg_export_request {
    struct bsg_view *view;       /**< @brief target view context */
    unsigned int query_flags;    /**< @brief BSG_EXPORT_QUERY_* filters */
    unsigned int render_flags;   /**< @brief BSG_RENDER_FLAG_* overrides */
    const char *glob;            /**< @brief optional path/name glob */
    int draw_mode;               /**< @brief draw mode filter, or BSG_EXPORT_DRAW_MODE_ANY */
};

struct bsg_export_record {
    struct bu_vls path;
    struct bsg_render_source source;
    mat_t model_mat;
    unsigned int roles;
    bsg_render_phase phase;
    struct bsg_render_geometry geometry;
    bsg_payload_realization_kind realization_kind;
    bsg_payload_realization_status realization_status;
    bsg_payload_stale_reason stale_reason;
    uint64_t geometry_revision;
    uint64_t payload_revision;
    uint64_t cache_identity;
    uint64_t settings_hash;
    unsigned int request_flags;
    unsigned int backend_capabilities;
    unsigned int capability_gaps;
    int non_database_source;
    int visible;
    int selected;
    int highlighted;
    int draw_mode;
    int line_style;
    int evaluated_region;
    unsigned char color[3];
    fastf_t transparency;
    point_t bounds_center;
    fastf_t bounds_radius;
    int has_bounds;
    int graph_depth;
    size_t sequence;
    int lod_level;
    int lod_level_count;
    uint64_t lod_identity;
    uint64_t lod_policy_identity;
    size_t vlist_structure_count;
    size_t vlist_command_count;
    size_t vlist_point_count;
};

struct bsg_export_result {
    struct bu_ptbl records;
};

typedef int (*bsg_export_segment_cb)(const point_t a,
				     const point_t b,
				     void *data);

typedef int (*bsg_export_point_cb)(const point_t pt,
				   void *data);

BSG_EXPORT extern struct bsg_export_result *
bsg_export_result_create(void);

BSG_EXPORT extern void
bsg_export_result_free(struct bsg_export_result *result);

BSG_EXPORT extern size_t
bsg_export_result_count(const struct bsg_export_result *result);

BSG_EXPORT extern const struct bsg_export_record *
bsg_export_result_get(const struct bsg_export_result *result, size_t idx);

BSG_EXPORT extern void
bsg_export_request_init(struct bsg_export_request *request,
			struct bsg_view *view);

BSG_EXPORT extern struct bsg_export_result *
bsg_export_query(const struct bsg_export_request *request);

BSG_EXPORT extern struct bsg_export_result *
bsg_export_scene(struct bsg_view *view,
		 unsigned int flags);

BSG_EXPORT extern void
bsg_export_result_serialize(const struct bsg_export_result *result,
			    struct bu_vls *out);

BSG_EXPORT extern void
bsg_export_record_geometry_report(const struct bsg_export_record *record,
				  struct bu_vls *out);

BSG_EXPORT extern size_t
bsg_export_record_foreach_segment(const struct bsg_export_record *record,
				  bsg_export_segment_cb cb,
				  void *data);

BSG_EXPORT extern int
bsg_export_record_has_segments(const struct bsg_export_record *record);

BSG_EXPORT extern size_t
bsg_export_record_foreach_point(const struct bsg_export_record *record,
				bsg_export_point_cb cb,
				void *data);

__END_DECLS

#endif /* BSG_EXPORT_H */

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
