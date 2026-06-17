/*               V I S I B I L I T Y _ P R I V A T E . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */
/** @file libbsg/visibility_private.h
 *
 * Private raw-node visibility predicates.
 */

#ifndef LIBBSG_VISIBILITY_PRIVATE_H
#define LIBBSG_VISIBILITY_PRIVATE_H

#include "common.h"
#include "bsg/visibility.h"
#include "node_private.h"

__BEGIN_DECLS

BSG_EXPORT extern int bsg_visibility_local(const bsg_node *node);
BSG_EXPORT extern int bsg_visibility_scope_allows_view(const bsg_node *node,
						       const struct bsg_view *v);
BSG_EXPORT extern int bsg_visibility_visible_in_view(const bsg_node *node,
						     const struct bsg_view *v);
BSG_EXPORT extern int bsg_visibility_hidden_in_view(const bsg_node *node,
						    const struct bsg_view *v);
BSG_EXPORT extern int bsg_visibility_inherited_visible(const bsg_node *node,
						       const struct bsg_view *v,
						       int parent_visible);

__END_DECLS

#endif /* LIBBSG_VISIBILITY_PRIVATE_H */
