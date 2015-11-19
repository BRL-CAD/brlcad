/*                   T W I S T E D C U B E . H
 * BRL-CAD
 *
 * Copyright (c) 2013 United States Government as represented by
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
/** @file twistedcube_private.h
 *
 * Private header for TwistedCube functions used in brep_simple.cpp,
 * brep_cube.cpp, and surfaceintersect.cpp
 *
 */

#ifndef PROC_DB_TWISTEDCUBE_H
#define PROC_DB_TWISTEDCUBE_H

#include "common.h"
#include "wdb.h"

extern ON_Curve* TwistedCubeEdgeCurve(const ON_3dPoint& from, const ON_3dPoint& to);

extern void MakeTwistedCubeEdge(ON_Brep& brep, int from, int to, int curve);

extern ON_Surface* TwistedCubeSideSurface(const ON_3dPoint& SW, const ON_3dPoint& SE, const ON_3dPoint& NE, const ON_3dPoint& NW);

extern ON_Curve* TwistedCubeTrimmingCurve(const ON_Surface& s, int side);

extern int MakeTwistedCubeTrimmingLoop(ON_Brep& brep,
                                       ON_BrepFace& face,
                                       int UNUSED(v0), int UNUSED(v1), int UNUSED(v2), int UNUSED(v3), // indices of corner vertices
                                       int e0, int eo0, // edge index + orientation w.r.t surface trim
                                       int e1, int eo1,
                                       int e2, int eo2,
                                       int e3, int eo3);

extern void MakeTwistedCubeFace(ON_Brep& brep,
                                int surf,
                                int orientation,
                                int v0, int v1, int v2, int v3, // the indices of corner vertices
                                int e0, int eo0, // edge index + orientation
                                int e1, int eo1,
                                int e2, int eo2,
                                int e3, int eo3);

extern void printPoints(struct rt_brep_internal* bi);

#endif /* PROC_DB_TWISTEDCUBE_H */

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
