/*                    B V _ P R I V A T E . C
 * BRL-CAD
 *
 * Copyright (c) 2020-2022 United States Government as represented by
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
#include "bu/ptbl.h"
#include "bv/defines.h"

__BEGIN_DECLS

struct bview_set_internal {
    struct bu_ptbl views;
    struct bu_ptbl shared_db_objs;
    struct bu_ptbl shared_view_objs;

    struct bv_scene_obj  *free_scene_obj;
};

__END_DECLS

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
