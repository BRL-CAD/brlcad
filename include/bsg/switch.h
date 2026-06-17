/*                        S W I T C H . H
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
 * Public typed switch child-list and selection API.
 */
/** @{ */
/* @file bsg/switch.h */

#ifndef BSG_SWITCH_H
#define BSG_SWITCH_H

#include "common.h"
#include "bsg/group.h"

__BEGIN_DECLS

#define BSG_SWITCH_NONE (-1)
#define BSG_SWITCH_ALL  (-2)

BSG_EXPORT extern size_t bsg_switch_ref_child_count(bsg_switch_ref ref);
BSG_EXPORT extern bsg_node_ref bsg_switch_ref_child_at(bsg_switch_ref ref, size_t idx);
BSG_EXPORT extern void bsg_switch_ref_append_child(bsg_switch_ref parent, bsg_node_ref child);
BSG_EXPORT extern void bsg_switch_ref_remove_child(bsg_switch_ref parent, bsg_node_ref child);
BSG_EXPORT extern void bsg_switch_ref_set_which_child(bsg_switch_ref ref, int which_child);
BSG_EXPORT extern int bsg_switch_ref_which_child(bsg_switch_ref ref);

__END_DECLS

#endif /* BSG_SWITCH_H */

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
