/*              D R A W _ S O U R C E _ P R I V A T E . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */
/** @file draw_source_private.h
 *
 * Private source-realization fields for GED drawing internals.
 */

#ifndef BSG_DRAW_SOURCE_PRIVATE_H
#define BSG_DRAW_SOURCE_PRIVATE_H

#include "common.h"
#include "node_fwd_private.h"
#include "vmath.h"
#include "bsg/defines.h"

__BEGIN_DECLS

BSG_EXPORT extern void bsg_node_set_view(bsg_node *node, struct bsg_view *v);
BSG_EXPORT extern struct bsg_view *bsg_node_get_view(const bsg_node *node);
BSG_EXPORT extern void bsg_node_set_draw_mat(bsg_node *node, const mat_t mat);
BSG_EXPORT extern void bsg_node_get_draw_mat(const bsg_node *node, mat_t mat);
BSG_EXPORT extern fastf_t bsg_node_draw_size(const bsg_node *node);
BSG_EXPORT extern void bsg_node_set_draw_size(bsg_node *node, fastf_t size);
BSG_EXPORT extern void bsg_node_get_draw_center(const bsg_node *node, vect_t center);
BSG_EXPORT extern void bsg_node_set_draw_center(bsg_node *node, const vect_t center);

BSG_EXPORT extern void bsg_node_source_realization_set_roles(bsg_node *node, int csg_obj, int mesh_obj);
BSG_EXPORT extern int bsg_node_source_realization_is_mesh(const bsg_node *node);

__END_DECLS

#endif /* BSG_DRAW_SOURCE_PRIVATE_H */
