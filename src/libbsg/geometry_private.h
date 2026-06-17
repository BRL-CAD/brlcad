/*                 G E O M E T R Y _ P R I V A T E . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */
/** @file geometry_private.h
 *
 * Source-private geometry-node bridge for legacy internals that still own raw
 * bsg_node pointers during the Stage 9 migration.
 */

#ifndef BSG_GEOMETRY_PRIVATE_H
#define BSG_GEOMETRY_PRIVATE_H

#include "common.h"
#include "node_fwd_private.h"
#include "vmath.h"
#include "bsg/geometry.h"

__BEGIN_DECLS

struct bsg_node;
struct bsg_axes;
struct bsg_grid_state;
struct bsg_mesh_lod;
struct bsg_payload;

BSG_EXPORT extern int bsg_geometry_node_set_kind(struct bsg_node *node,
	bsg_geometry_node_kind kind);
BSG_EXPORT extern bsg_geometry_node_kind bsg_geometry_node_kind_get(const struct bsg_node *node);
BSG_EXPORT extern uint64_t bsg_geometry_node_revision(const struct bsg_node *node);
BSG_EXPORT extern int bsg_geometry_node_bump_revision(struct bsg_node *node);
BSG_EXPORT extern int bsg_geometry_node_set_line_set(struct bsg_node *node,
	const point_t *points, const int *commands, size_t point_count);
BSG_EXPORT extern int bsg_geometry_node_set_line_set_fields(struct bsg_node *node,
	const point_t *points, const int *commands, size_t point_count);
BSG_EXPORT extern int bsg_geometry_node_set_point_set_fields(struct bsg_node *node,
	const point_t *points, size_t point_count);
BSG_EXPORT extern int bsg_geometry_ref_set_indexed_face_set_fields(bsg_geometry_ref ref,
	const point_t *points, size_t point_count,
	const vect_t *normals, size_t normal_count,
	const int *indices, size_t index_count);
BSG_EXPORT extern int bsg_geometry_node_set_indexed_face_set_fields(struct bsg_node *node,
	const point_t *points, size_t point_count,
	const vect_t *normals, size_t normal_count,
	const int *indices, size_t index_count);
BSG_EXPORT extern int bsg_geometry_node_set_indexed_face_set(struct bsg_node *node,
	const point_t *points, size_t point_count,
	const vect_t *normals, size_t normal_count,
	const int *indices, size_t index_count);
BSG_EXPORT extern int bsg_geometry_node_set_hud_text(struct bsg_node *node,
	const char *text, fastf_t x, fastf_t y, int size);
BSG_EXPORT extern int bsg_geometry_node_set_text_label(struct bsg_node *node,
	const struct bsg_label *label);
BSG_EXPORT extern int bsg_geometry_node_set_axes_overlay(struct bsg_node *node,
	const struct bsg_axes *axes);
BSG_EXPORT extern int bsg_geometry_node_set_grid_overlay(struct bsg_node *node,
	const struct bsg_grid_state *grid);
BSG_EXPORT extern int bsg_geometry_node_set_mesh_realization(struct bsg_node *node,
	struct bsg_mesh_lod *mesh);
BSG_EXPORT extern int bsg_geometry_node_set_framebuffer_mode(struct bsg_node *node, int mode);
BSG_EXPORT extern int bsg_geometry_node_clear_private_realization(struct bsg_node *node);
BSG_EXPORT extern int bsg_geometry_node_set_private_realization(struct bsg_node *node,
	struct bsg_payload *payload);

__END_DECLS

#endif /* BSG_GEOMETRY_PRIVATE_H */
