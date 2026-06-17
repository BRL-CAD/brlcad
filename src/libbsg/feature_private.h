/*                  F E A T U R E _ P R I V A T E . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
/** @file libbsg/feature_private.h
 *
 * Storage-level feature helpers for libbsg internals and legacy database
 * scene code.  User-facing feature producers must use bsg/feature.h
 * transactions rather than fetching feature nodes.
 */

#ifndef LIBBSG_FEATURE_PRIVATE_H
#define LIBBSG_FEATURE_PRIVATE_H

#include "common.h"
#include "node_fwd_private.h"
#include "bsg/feature.h"

__BEGIN_DECLS

typedef int (*bsg_feature_node_visit_cb)(struct bsg_node *node, void *data);

extern struct bsg_node *
bsg_feature_node(bsg_feature_ref ref);

extern bsg_feature_ref
bsg_feature_ref_from_node(struct bsg_node *node, enum bsg_feature_family fallback_family);

extern void
bsg_feature_node_meta_clear(struct bsg_node *node);

extern void
bsg_feature_visit_nodes(struct bsg_view *v, int scope_mask, bsg_feature_node_visit_cb cb, void *data);

__END_DECLS

#endif /* LIBBSG_FEATURE_PRIVATE_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
