/*              D R A W _ I N T E N T _ P R I V A T E . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */
/** @file libbsg/draw_intent_private.h
 *
 * Private raw-node draw-intent binding and traversal helpers.
 */

#ifndef LIBBSG_DRAW_INTENT_PRIVATE_H
#define LIBBSG_DRAW_INTENT_PRIVATE_H

#include "common.h"
#include "bu/ptbl.h"
#include "bsg/draw_intent.h"
#include "node_private.h"

__BEGIN_DECLS

BSG_EXPORT extern void bsg_node_set_draw_intent(bsg_node *node, struct bsg_draw_intent *di);
BSG_EXPORT extern struct bsg_draw_intent *bsg_node_get_draw_intent(const bsg_node *node);
BSG_EXPORT extern bsg_node *bsg_draw_intent_find(bsg_node *root, const char *path);
BSG_EXPORT extern void bsg_collect_draw_groups(bsg_node *root, struct bu_ptbl *groups,
					       int include_overlays);
BSG_EXPORT extern int bsg_draw_intent_revalidate(bsg_node *root,
						 const struct bsg_db_event *event);
BSG_EXPORT extern int bsg_draw_intent_erase_by_path(bsg_node *root, const char *path);
BSG_EXPORT extern int bsg_draw_intent_erase(bsg_node *root, struct bsg_draw_intent *intent);
BSG_EXPORT extern int bsg_draw_intent_simplify(bsg_node *root);
BSG_EXPORT extern void bsg_draw_intent_collect_visible(bsg_node *root, struct bu_ptbl *out,
						       const struct bsg_view *v);
BSG_EXPORT extern void bsg_draw_intent_collect_for_export(bsg_node *root, struct bu_ptbl *out,
							  unsigned long long flags);
BSG_EXPORT extern void bsg_draw_intent_match(bsg_node *root, const char *pattern,
					     struct bu_ptbl *out);

__END_DECLS

#endif /* LIBBSG_DRAW_INTENT_PRIVATE_H */
