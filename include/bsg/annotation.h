/*                  A N N O T A T I O N . H
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
 * @brief Typed annotation records shared by geometry, render, export, and
 * backend handoff APIs.
 */
/** @{ */
/* @file bsg/annotation.h */

#ifndef BSG_ANNOTATION_H
#define BSG_ANNOTATION_H

#include "common.h"
#include <stddef.h>
#include "vmath.h"

__BEGIN_DECLS

typedef enum bsg_annotation_space {
    BSG_ANNOTATION_SPACE_MODEL = 0,
    BSG_ANNOTATION_SPACE_DISPLAY = 1
} bsg_annotation_space;

typedef enum bsg_annotation_segment_kind {
    BSG_ANNOTATION_SEGMENT_NONE = 0,
    BSG_ANNOTATION_SEGMENT_LINE,
    BSG_ANNOTATION_SEGMENT_CARC,
    BSG_ANNOTATION_SEGMENT_NURB,
    BSG_ANNOTATION_SEGMENT_BEZIER,
    BSG_ANNOTATION_SEGMENT_TEXT
} bsg_annotation_segment_kind;

typedef enum bsg_annotation_text_position {
    BSG_ANNOTATION_TEXT_POS_DEFAULT = 0,
    BSG_ANNOTATION_TEXT_POS_BOTTOM_LEFT = 1,
    BSG_ANNOTATION_TEXT_POS_BOTTOM_CENTER = 2,
    BSG_ANNOTATION_TEXT_POS_BOTTOM_RIGHT = 3,
    BSG_ANNOTATION_TEXT_POS_MIDDLE_LEFT = 4,
    BSG_ANNOTATION_TEXT_POS_MIDDLE_CENTER = 5,
    BSG_ANNOTATION_TEXT_POS_MIDDLE_RIGHT = 6,
    BSG_ANNOTATION_TEXT_POS_TOP_LEFT = 7,
    BSG_ANNOTATION_TEXT_POS_TOP_CENTER = 8,
    BSG_ANNOTATION_TEXT_POS_TOP_RIGHT = 9
} bsg_annotation_text_position;

/*
 * Segment point indexes address the annotation record's local point table.
 * MODEL-space consumers transform those points with model_mat.  DISPLAY-space
 * consumers anchor the local points at the record anchor and transform them
 * with display_mat in display units.
 */
struct bsg_annotation_segment {
    bsg_annotation_segment_kind kind;
    int reverse;
    union {
	struct {
	    /** Indexes into the annotation record's local point table. */
	    int start;
	    int end;
	} line;
	struct {
	    /** Indexes into the annotation record's local point table. */
	    int start;
	    int end;
	    fastf_t radius;
	    int center_is_left;
	    int orientation;
	    int center;
	} carc;
	struct {
	    int order;
	    int point_type;
	    size_t knot_count;
	    fastf_t *knots;
	    size_t control_point_count;
	    int *control_points;
	    fastf_t *weights;
	} nurb;
	struct {
	    int degree;
	    size_t control_point_count;
	    int *control_points;
	} bezier;
	struct {
	    /** Index into the annotation record's local point table. */
	    int ref_pt;
	    /** bsg_annotation_text_position value; 0 leaves the point unadjusted. */
	    int relative_position;
	    char *text;
	    fastf_t size;
	    fastf_t rotation;
	} text;
    } data;
};

__END_DECLS

#endif /* BSG_ANNOTATION_H */

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
