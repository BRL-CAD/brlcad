/*                  P O L Y G O N _ P R I V A T E . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */
/** @file polygon_private.h
 *
 * Private polygon node selectors.  Public callers use polygon refs.
 */

#ifndef BSG_POLYGON_PRIVATE_H
#define BSG_POLYGON_PRIVATE_H

#include "common.h"
#include "node_fwd_private.h"
#include "bsg/polygon.h"

__BEGIN_DECLS

typedef int (*bsg_polygon_visit_cb)(struct bsg_node *node, struct bsg_polygon *polygon, void *data);

BSG_EXPORT extern struct bsg_node *bsg_create_polygon_obj(struct bsg_view *v, int flags, struct bsg_polygon *p);
BSG_EXPORT extern struct bsg_polygon *bsg_node_polygon(const struct bsg_node *node);
BSG_EXPORT extern struct bsg_node *bsg_create_polygon(struct bsg_view *v, int flags, int type, point_t *fp);
BSG_EXPORT extern int bsg_polygon_realize(struct bsg_node *node);
BSG_EXPORT extern struct bsg_node *bsg_select_polygon(struct bu_ptbl *objs, point_t *cp);
BSG_EXPORT extern struct bsg_node *bsg_view_select_polygon(struct bsg_view *v, point_t *cp);
BSG_EXPORT extern struct bsg_node *bsg_view_polygon_find(struct bsg_view *v, const char *name);
BSG_EXPORT extern struct bsg_node *bsg_view_polygon_dup(struct bsg_view *v, const char *name, const char *new_name);
BSG_EXPORT extern void bsg_view_polygon_visit(struct bsg_view *v, bsg_polygon_visit_cb cb, void *data);
BSG_EXPORT extern int bsg_update_polygon(struct bsg_node *s, struct bsg_view *v, int utype);
BSG_EXPORT extern int bsg_move_polygon(struct bsg_node *s, point_t *cp, point_t *pp);
BSG_EXPORT extern struct bsg_node *bsg_dup_view_polygon(const char *nname, struct bsg_node *s);
BSG_EXPORT extern int bsg_polygon_csg(struct bsg_node *target, struct bsg_node *stencil, bg_clip_t op);

__END_DECLS

#endif /* BSG_POLYGON_PRIVATE_H */
