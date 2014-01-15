/*                        D M _ U T I L . H
 * BRL-CAD
 *
 * Copyright (c) 1988-2014 United States Government as represented by
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
/** @file libdm/dm_util.h
 */

#ifndef LIBDM_DM_UTIL_H
#define LIBDM_DM_UTIL_H

#include "common.h"

#include "vmath.h"
#include "dm.h"

__BEGIN_DECLS

int
drawLine3D(struct dm *dmp, point_t pt1, point_t pt2, char *log_bu, float *wireColor);

int
drawLines3D(struct dm *dmp, int npoints, point_t *points, int sflag, char *log_bu, float *wireColor);

int
drawLine2D(struct dm *dmp, fastf_t X1, fastf_t Y1, fastf_t X2, fastf_t Y2, char *log_bu);

int
draw_Line3D(struct dm *dmp, point_t pt1, point_t pt2);

__END_DECLS

#endif /* LIBDM_DM_UTIL_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
