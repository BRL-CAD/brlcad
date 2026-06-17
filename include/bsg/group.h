/*                         G R O U P . H
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
 * Public typed group child-list API.
 */
/** @{ */
/* @file bsg/group.h */

#ifndef BSG_GROUP_H
#define BSG_GROUP_H

#include "common.h"
#include "bsg/node.h"

__BEGIN_DECLS

/*
 * Child-list operations do not transfer ownership.  Appending records a
 * parent relationship; callers that own child refs remain responsible for
 * releasing them unless another documented owner takes over.
 */
BSG_EXPORT extern size_t bsg_group_ref_child_count(bsg_group_ref ref);
BSG_EXPORT extern bsg_node_ref bsg_group_ref_child_at(bsg_group_ref ref, size_t idx);
BSG_EXPORT extern void bsg_group_ref_append_child(bsg_group_ref parent, bsg_node_ref child);
BSG_EXPORT extern void bsg_group_ref_remove_child(bsg_group_ref parent, bsg_node_ref child);

__END_DECLS

#endif /* BSG_GROUP_H */

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
