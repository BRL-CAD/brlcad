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


// Note - for these functions it is important that the bview
// gv_width and gv_height values are current!  I.e.:
//
//  v->gv_width  = dm_get_width((struct dm *)v->dmp);
//  v->gv_height = dm_get_height((struct dm *)v->dmp);

BVIEW_EXPORT extern struct bview_scene_obj *bview_create_circle(struct bview *v, int x, int y);

BVIEW_EXPORT extern int bview_update_circle(struct bview_scene_obj *s);

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
