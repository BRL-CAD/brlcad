/*                  G _ S T E P _ U T I L . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013-2026 United States Government as represented by
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
/** @file G_STEP_util.cpp
 *
 * General utilities for translating data to STEPcode containers
 *
 */

#include "AP_Common.h"
#include "STEPGeneratedAPI.h"

void
XYZ_to_Cartesian_point(double x, double y, double z, STEPentity *step_pnt) {
    RealAggregate *coord_vals = dynamic_cast<RealAggregate *>(
	brlcad::step::Aggregate(step_pnt, "coordinates"));
    if (!coord_vals) return;
    RealNode *xnode = new RealNode();
    xnode->value = x;
    coord_vals->AddNode(xnode);
    RealNode *ynode = new RealNode();
    ynode->value = y;
    coord_vals->AddNode(ynode);
    RealNode *znode = new RealNode();
    znode->value = z;
    coord_vals->AddNode(znode);
}


void
XYZ_to_Direction(double x, double y, double z, STEPentity *step_direction) {
    RealAggregate *coord_vals = dynamic_cast<RealAggregate *>(
	brlcad::step::Aggregate(step_direction, "direction_ratios"));
    if (!coord_vals) return;
    RealNode *xnode = new RealNode();
    xnode->value = x;
    coord_vals->AddNode(xnode);
    RealNode *ynode = new RealNode();
    ynode->value = y;
    coord_vals->AddNode(ynode);
    RealNode *znode = new RealNode();
    znode->value = z;
    coord_vals->AddNode(znode);
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
