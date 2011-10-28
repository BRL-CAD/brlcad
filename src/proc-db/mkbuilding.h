/*                    M K B U I L D I N G . H
 * BRL-CAD
 *
 * Copyright (c) 2009-2011 United States Government as represented by
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
/** @file proc-db/mkbuilding.h
 *
 *
 *
 */

#ifndef __MKBUILDING_H__
#define __MKBUILDING_H__

#define USE2X4   1
#define USE2X6   2


#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "wdb.h"


void mkbldg_makeWallSegment(char *name, struct rt_wdb *db_filepointer, point_t p1, point_t p2);

void mkbldg_makeframe(struct bu_vls *name, struct rt_wdb *db_fileptr, point_t p1, point_t p2, int thickness);

#endif

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
