/*                         B R E P I N T E R S E C T . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file proc-db/brepintersect.h
 *
 */

#include "common.h"

#include "raytrace.h"
#include "rtgeom.h"
#include "wdb.h"
#include "bn.h"
#include "bu.h"
#include "vmath.h"
#include "opennurbs_array.h"


bool PointInTriangle(
    const ON_3dPoint& a,
    const ON_3dPoint& b,
    const ON_3dPoint& c,
    const ON_3dPoint& P,
    double tol
    );

int SegmentSegmentIntersect(
    const ON_3dPoint& x1,
    const ON_3dPoint& x2,
    const ON_3dPoint& x3,
    const ON_3dPoint& x4,
    ON_3dPoint x[2],
    double tol
    );

int SegmentTriangleIntersect(
    const ON_3dPoint& a,
    const ON_3dPoint& b,
    const ON_3dPoint& c,
    const ON_3dPoint& p,
    const ON_3dPoint& q,
    ON_3dPoint out[2], 
    double tol
    );

int TriangleTriangleIntersect(
    const ON_3dPoint T1[3],
    const ON_3dPoint T2[3],
    ON_Line& out,
    double tol
    );

int TriangleBrepIntersect(
    const ON_3dPoint T1[3],
    const ON_Brep brep,
    ON_Curve& out,
    double tol
    );


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
