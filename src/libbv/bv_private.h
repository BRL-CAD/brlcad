/*                    B V _ P R I V A T E . h
 * BRL-CAD
 *
 * Copyright (c) 2020-2025 United States Government as represented by
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
/** @file bv_private.h
 *
 * Internal shared data structures
 *
 */

#include "common.h"
#include "bu/list.h"
#include "bu/malloc.h"
#include "bu/ptbl.h"
#include "bv/defines.h"
#include "bv/objs.h"

struct bv_obj_pool_internal {
    struct bv_scene_obj head;
    struct bu_list vlfree;
};

BV_EXPORT extern struct bv_scene_obj *
bv_obj_pool_get(struct bv_obj_pool *p);

BV_EXPORT extern void
bv_obj_pool_put(struct bv_obj_pool *p, struct bv_scene_obj *s);

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
