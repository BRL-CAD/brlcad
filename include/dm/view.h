/*                         V I E W . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2021 United States Government as represented by
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
/** @addtogroup libdm */
/** @{ */
/** @file dm/view.h
 *
 * dm routines related to view management (migrated from libtclcad.) These may
 * ultimately take a different form or move elsewhere - the immediate idea here
 * is to extract the key logic from libtclcad for use in non-Tcl environments.
 *
 */

#include "common.h"

#include "vmath.h"

#include "bu/hash.h"
#include "bu/vls.h"
#include "bv/defines.h"
#include "dm/defines.h"

/* TODO - needed for dm_draw_labels, which cracks the database
 * objects to generate label info.  Need to think about how to
 * better handle this... - ideally should be a callback of some
 * sort on a bv scene object... */
#include "rt/wdb.h"

#ifndef DM_VIEW_H
#define DM_VIEW_H

__BEGIN_DECLS

struct dm_path_edit_params {
    int edit_mode;
    double dx;
    double dy;
    mat_t edit_mat;
};

struct dm_view_data {
    struct bu_hash_tbl  *edited_paths;
    struct bu_vls       *prim_label_list;
    int                 prim_label_list_size;
    int                 dlist_on;
    int                 refresh_on;
};

DM_EXPORT extern void dm_draw_faceplate(struct bview *v, double base2local, double local2base);
DM_EXPORT extern void dm_draw_viewobjs(struct rt_wdb *wdbp, struct bview *v, struct dm_view_data *d, double base2local, double local2base);

/* Stripped down form of dm_draw_viewobjs that does just what's needed for the new setup */
DM_EXPORT extern void dm_draw_objs(struct bview *v, double base2local, double local2base, void (*dm_draw_custom)(struct bview *, double, double, void *), void *u_data);

__END_DECLS

#endif /* DM_VIEW_H */

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
