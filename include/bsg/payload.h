/*                     P A Y L O A D . H
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
 * @brief
 * Public payload metadata used by render, export, and backend handoff records.
 *
 * This header deliberately exposes semantic payload kinds and resolved data
 * record layouts, not the private payload owner handle or mutation API.
 */
/** @{ */
/* @file bsg/payload.h */

#ifndef BSG_PAYLOAD_H
#define BSG_PAYLOAD_H

#include "common.h"
#include <stddef.h>
#include <stdint.h>
#include "bu/vls.h"
#include "vmath.h"
#include "bsg/annotation.h"
#include "bsg/defines.h"

__BEGIN_DECLS

#define BSG_PAYLOAD_PUBLIC_TYPES_DEFINED 1

struct bsg_grid_state;
struct bsg_mesh_lod;
struct fb;

typedef enum bsg_payload_realization_kind {
    BSG_REALIZE_NONE = 0,
    BSG_REALIZE_WIREFRAME,
    BSG_REALIZE_EVALUATED_WIREFRAME,
    BSG_REALIZE_SHADED_MESH,
    BSG_REALIZE_ADAPTIVE_WIREFRAME,
    BSG_REALIZE_LOD_MESH,
    BSG_REALIZE_BREP_DISPLAY,
    BSG_REALIZE_POLYGON,
    BSG_REALIZE_EDIT_PREVIEW
} bsg_payload_realization_kind;

typedef enum bsg_payload_realization_status {
    BSG_REALIZE_STATUS_CURRENT = 0,
    BSG_REALIZE_STATUS_STALE,
    BSG_REALIZE_STATUS_FAILED
} bsg_payload_realization_status;

typedef enum bsg_payload_stale_reason {
    BSG_PAYLOAD_STALE_NONE = 0,
    BSG_PAYLOAD_STALE_SOURCE_CHANGED,
    BSG_PAYLOAD_STALE_VIEW_INPUT_CHANGED,
    BSG_PAYLOAD_STALE_SETTINGS_CHANGED,
    BSG_PAYLOAD_STALE_FORCED,
    BSG_PAYLOAD_STALE_UPDATE_FAILED
} bsg_payload_stale_reason;

struct bsg_payload_realization {
    bsg_payload_realization_kind kind;
    bsg_payload_realization_status status;
    bsg_payload_stale_reason stale_reason;
    uint64_t source_revision;
    uint64_t inputs_revision;
    uint64_t generated_revision;
    uint64_t realized_source_revision;
    uint64_t realized_inputs_revision;
    int has_bounds;
    point_t bmin;
    point_t bmax;
    char failure_reason[128];
};

struct bsg_payload_line_set {
    size_t point_cnt;
    point_t *points;
    int *cmds;
};

struct bsg_payload_image {
    size_t width;
    size_t height;
    size_t channels;
    unsigned char *pixels;
};

struct bsg_payload_framebuffer {
    struct fb *fbp;
    int mode;
};

struct bsg_payload_annotation {
    struct bu_vls text;
    bsg_annotation_space space;
    point_t anchor;
    mat_t model_mat;
    mat_t display_mat;
    size_t point_cnt;
    point_t *points;
    size_t segment_cnt;
    struct bsg_annotation_segment *segments;
};

typedef enum {
    BSG_PL_NONE       = 0,
    BSG_PL_TEXT,
    BSG_PL_HUD_TEXT,
    BSG_PL_LINE_SET,
    BSG_PL_POLYGON,
    BSG_PL_MESH,
    BSG_PL_CSG,
    BSG_PL_BREP,
    BSG_PL_IMAGE,
    BSG_PL_FRAMEBUFFER,
    BSG_PL_AXES,
    BSG_PL_GRID,
    BSG_PL_SKETCH,
    BSG_PL_ANNOTATION,
    BSG_PL_EDIT_PREVIEW
} bsg_payload_type;

BSG_EXPORT extern bsg_payload_realization_kind
bsg_payload_default_realization_kind(bsg_payload_type type);

BSG_EXPORT extern const char *
bsg_payload_realization_kind_name(bsg_payload_realization_kind kind);

BSG_EXPORT extern const char *
bsg_payload_realization_status_name(bsg_payload_realization_status status);

BSG_EXPORT extern const char *
bsg_payload_stale_reason_name(bsg_payload_stale_reason reason);

__END_DECLS

#endif /* BSG_PAYLOAD_H */

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
