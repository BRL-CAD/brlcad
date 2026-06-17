/*                         S T A T E . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
/** @addtogroup libbsg
 *
 * @brief
 * Public traversal-state API.
 *
 * A bsg_state_ref is an owning handle to one resolved traversal-state frame.
 * Actions push/copy state as they traverse the graph, and consumers read
 * semantic state from the handle instead of probing graph internals.
 */
/** @{ */
/* @file bsg/state.h */

#ifndef BSG_STATE_H
#define BSG_STATE_H

#include "common.h"
#include <stdint.h>
#include "vmath.h"
#include "bsg/lod.h"
#include "bsg/node.h"
#include "bsg/camera.h"
#include "bsg/light.h"

__BEGIN_DECLS

typedef struct bsg_state_ref {
    void *opaque;
} bsg_state_ref;

#define BSG_STATE_REF_NULL_INIT { NULL }

BSG_EXPORT extern bsg_state_ref bsg_state_ref_null(void);
BSG_EXPORT extern int bsg_state_ref_is_null(bsg_state_ref ref);
BSG_EXPORT extern bsg_state_ref bsg_state_ref_create(void);
BSG_EXPORT extern bsg_state_ref bsg_state_ref_copy(bsg_state_ref ref);
BSG_EXPORT extern bsg_state_ref bsg_state_ref_push(bsg_state_ref parent);
BSG_EXPORT extern void bsg_state_ref_pop(bsg_state_ref ref);
BSG_EXPORT extern void bsg_state_ref_destroy(bsg_state_ref ref);

BSG_EXPORT extern bsg_node_ref bsg_state_ref_root(bsg_state_ref ref);
BSG_EXPORT extern bsg_node_ref bsg_state_ref_node(bsg_state_ref ref);
BSG_EXPORT extern void bsg_state_ref_transform(bsg_state_ref ref, mat_t mat);
BSG_EXPORT extern void bsg_state_ref_set_transform(bsg_state_ref ref, const mat_t mat);
BSG_EXPORT extern void bsg_state_ref_multiply_transform(bsg_state_ref ref, const mat_t mat);

BSG_EXPORT extern int bsg_state_ref_inherited_visible(bsg_state_ref ref);
BSG_EXPORT extern int bsg_state_ref_visible(bsg_state_ref ref);
BSG_EXPORT extern int bsg_state_ref_force_draw(bsg_state_ref ref);
BSG_EXPORT extern void bsg_state_ref_set_visibility(bsg_state_ref ref,
						    int inherited_visible,
						    int visible,
						    int force_draw);

BSG_EXPORT extern int bsg_state_ref_selected(bsg_state_ref ref);
BSG_EXPORT extern int bsg_state_ref_highlighted(bsg_state_ref ref);
BSG_EXPORT extern void bsg_state_ref_set_selection(bsg_state_ref ref,
						   int selected,
						   int highlighted);

BSG_EXPORT extern int bsg_state_ref_lod_level(bsg_state_ref ref);
BSG_EXPORT extern int bsg_state_ref_lod_level_count(bsg_state_ref ref);
BSG_EXPORT extern bsg_lod_source_ref bsg_state_ref_lod_source(bsg_state_ref ref);
BSG_EXPORT extern void bsg_state_ref_set_lod(bsg_state_ref ref,
					     bsg_lod_source_ref source,
					     int level,
					     int level_count);

BSG_EXPORT extern int bsg_state_ref_render_phase(bsg_state_ref ref);
BSG_EXPORT extern void bsg_state_ref_set_render_phase(bsg_state_ref ref, int phase);
BSG_EXPORT extern int bsg_state_ref_view_scope_visible(bsg_state_ref ref);
BSG_EXPORT extern void bsg_state_ref_set_view_scope_visible(bsg_state_ref ref, int visible);
BSG_EXPORT extern uint64_t bsg_state_ref_source_identity(bsg_state_ref ref);
BSG_EXPORT extern void bsg_state_ref_set_source_identity(bsg_state_ref ref, uint64_t source_identity);
BSG_EXPORT extern unsigned int bsg_state_ref_backend_capabilities(bsg_state_ref ref);
BSG_EXPORT extern void bsg_state_ref_set_backend_capabilities(bsg_state_ref ref, unsigned int capabilities);

BSG_EXPORT extern void bsg_state_ref_material_color(bsg_state_ref ref, unsigned char color[3]);
BSG_EXPORT extern void bsg_state_ref_set_material_color(bsg_state_ref ref, const unsigned char color[3]);
BSG_EXPORT extern double bsg_state_ref_material_transparency(bsg_state_ref ref);
BSG_EXPORT extern void bsg_state_ref_set_material_transparency(bsg_state_ref ref, double transparency);
BSG_EXPORT extern int bsg_state_ref_has_material(bsg_state_ref ref);
BSG_EXPORT extern uint32_t bsg_state_ref_material_revision(bsg_state_ref ref);
BSG_EXPORT extern uint32_t bsg_state_ref_appearance_revision(bsg_state_ref ref);
BSG_EXPORT extern void bsg_state_ref_set_appearance_revisions(bsg_state_ref ref,
							      uint32_t material_revision,
							      uint32_t appearance_revision);

BSG_EXPORT extern int bsg_state_ref_draw_mode(bsg_state_ref ref);
BSG_EXPORT extern int bsg_state_ref_line_width(bsg_state_ref ref);
BSG_EXPORT extern int bsg_state_ref_line_style(bsg_state_ref ref);
BSG_EXPORT extern int bsg_state_ref_has_draw_style(bsg_state_ref ref);
BSG_EXPORT extern void bsg_state_ref_set_draw_style(bsg_state_ref ref,
						    int draw_mode,
						    int line_width,
						    int line_style);
BSG_EXPORT extern double bsg_state_ref_complexity(bsg_state_ref ref);
BSG_EXPORT extern int bsg_state_ref_has_complexity(bsg_state_ref ref);
BSG_EXPORT extern void bsg_state_ref_set_complexity(bsg_state_ref ref, double complexity);

BSG_EXPORT extern int bsg_state_ref_has_camera(bsg_state_ref ref);
BSG_EXPORT extern int bsg_state_ref_camera_projection(bsg_state_ref ref);
BSG_EXPORT extern double bsg_state_ref_camera_perspective(bsg_state_ref ref);
BSG_EXPORT extern void bsg_state_ref_camera_position(bsg_state_ref ref, point_t position);
BSG_EXPORT extern void bsg_state_ref_camera_orientation(bsg_state_ref ref, mat_t orientation);
BSG_EXPORT extern double bsg_state_ref_camera_near_distance(bsg_state_ref ref);
BSG_EXPORT extern double bsg_state_ref_camera_far_distance(bsg_state_ref ref);
BSG_EXPORT extern void bsg_state_ref_set_camera(bsg_state_ref ref,
						int projection,
						double perspective,
						const point_t position,
						const mat_t orientation,
						double near_distance,
						double far_distance);

BSG_EXPORT extern int bsg_state_ref_has_light(bsg_state_ref ref);
BSG_EXPORT extern int bsg_state_ref_light_type(bsg_state_ref ref);
BSG_EXPORT extern void bsg_state_ref_light_position(bsg_state_ref ref, point_t position);
BSG_EXPORT extern void bsg_state_ref_light_direction(bsg_state_ref ref, vect_t direction);
BSG_EXPORT extern void bsg_state_ref_light_diffuse_color(bsg_state_ref ref, unsigned char color[3]);
BSG_EXPORT extern void bsg_state_ref_light_specular_color(bsg_state_ref ref, unsigned char color[3]);
BSG_EXPORT extern void bsg_state_ref_light_ambient_color(bsg_state_ref ref, unsigned char color[3]);
BSG_EXPORT extern double bsg_state_ref_light_intensity(bsg_state_ref ref);
BSG_EXPORT extern int bsg_state_ref_light_active(bsg_state_ref ref);
BSG_EXPORT extern void bsg_state_ref_set_light(bsg_state_ref ref,
					       int type,
					       const point_t position,
					       const vect_t direction,
					       const unsigned char diffuse_color[3],
					       const unsigned char specular_color[3],
					       const unsigned char ambient_color[3],
					       double intensity,
					       int active);

__END_DECLS

#endif /* BSG_STATE_H */

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
