/*                    S E P A R A T O R . H
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
 * Public typed separator child-list API.
 */
/** @{ */
/* @file bsg/separator.h */

#ifndef BSG_SEPARATOR_H
#define BSG_SEPARATOR_H

#include "common.h"
#include "bsg/group.h"

__BEGIN_DECLS

BSG_EXPORT extern size_t bsg_separator_ref_child_count(bsg_separator_ref ref);
BSG_EXPORT extern bsg_node_ref bsg_separator_ref_child_at(bsg_separator_ref ref, size_t idx);
BSG_EXPORT extern void bsg_separator_ref_append_child(bsg_separator_ref parent, bsg_node_ref child);
BSG_EXPORT extern void bsg_separator_ref_remove_child(bsg_separator_ref parent, bsg_node_ref child);

__END_DECLS

#endif /* BSG_SEPARATOR_H */

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
