/*                      P O L Y G O N S . H
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
/** @addtogroup bview
 *
 * @brief
 * Functions related to bview polygon manipulation
 *
 */
#ifndef BVIEW_POLYGONS_H
#define BVIEW_POLYGONS_H

#include "common.h"
#include "bn/tol.h"
#include "dm/defines.h"
#include "bview/defines.h"

/** @{ */
/** @file bview/util.h */

__BEGIN_DECLS

#define BVIEW_POLYGON_GENERAL 0
#define BVIEW_POLYGON_CIRCLE 1
#define BVIEW_POLYGON_ELLIPSE 2
#define BVIEW_POLYGON_RECTANGLE 3
#define BVIEW_POLYGON_SQUARE 4
struct bview_polygon {
    int                 type;
    int                 cflag;             /* contour flag */
    int                 sflag;             /* point select flag */
    int                 mflag;             /* point move flag */
    int                 aflag;             /* point append flag */
    long                curr_contour_i;
    long                curr_point_i;
    point_t             prev_point;

    /* We stash the view state on creation, so we know how to return
     * to it for future 2D alterations */
    struct bview v;

    /* Actual polygon info */
    struct bg_polygon   polygon;
};

// Note - for these functions it is important that the bview
// gv_width and gv_height values are current!  I.e.:
//
//  v->gv_width  = dm_get_width((struct dm *)v->dmp);
//  v->gv_height = dm_get_height((struct dm *)v->dmp);
BVIEW_EXPORT extern struct bview_scene_obj *bview_create_polygon(struct bview *v, int type, int x, int y);
BVIEW_EXPORT extern int bview_update_polygon(struct bview_scene_obj *s);

__END_DECLS

/** @} */

#endif /* BVIEW_POLYGONS_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
