/*         S H A P E _ R E P R E S E N T A T I O N . C P P
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
/** @file Shape_Representation.cpp
 *
 */

#include "AP_Common.h"
#include "STEPGeneratedAPI.h"
#include "Shape_Representation.h"

STEPentity *
Add_Shape_Representation(AP203_Contents *sc, STEPentity *context)
{
    STEPentity *shape_rep = brlcad::step::CreateEntity(sc->registry,
	    sc->instance_list, "SHAPE_REPRESENTATION");
    brlcad::step::SetString(shape_rep, "name", "");
    brlcad::step::SetEntity(shape_rep, "context_of_items", context);

    /* create an axis */

    STEPentity *axis3d = brlcad::step::CreateEntity(sc->registry,
	    sc->instance_list, "AXIS2_PLACEMENT_3D");
    brlcad::step::SetString(axis3d, "name", "");

    /* set the axis origin */

    STEPentity *origin = brlcad::step::CreateEntity(sc->registry,
	    sc->instance_list, "CARTESIAN_POINT");
    brlcad::step::AddReal(origin, "coordinates", 0.0);
    brlcad::step::AddReal(origin, "coordinates", 0.0);
    brlcad::step::AddReal(origin, "coordinates", 0.0);
    brlcad::step::SetString(origin, "name", "");
    brlcad::step::SetEntity(axis3d, "location", origin);

    /* set the axis up direction (i-vector) */

    STEPentity *axis = brlcad::step::CreateEntity(sc->registry,
	    sc->instance_list, "DIRECTION");
    brlcad::step::AddReal(axis, "direction_ratios", 0.0);
    brlcad::step::AddReal(axis, "direction_ratios", 0.0);
    brlcad::step::AddReal(axis, "direction_ratios", 1.0);
    brlcad::step::SetString(axis, "name", "");
    brlcad::step::SetEntity(axis3d, "axis", axis);

    /* add the axis front direction (j-vector) */

    STEPentity *ref_dir = brlcad::step::CreateEntity(sc->registry,
	    sc->instance_list, "DIRECTION");
    brlcad::step::AddReal(ref_dir, "direction_ratios", 1.0);
    brlcad::step::AddReal(ref_dir, "direction_ratios", 0.0);
    brlcad::step::AddReal(ref_dir, "direction_ratios", 0.0);
    brlcad::step::SetString(ref_dir, "name", "");
    brlcad::step::SetEntity(axis3d, "ref_direction", ref_dir);

    /* add the axis to the shape definition */

    brlcad::step::AddEntity(shape_rep, "items", axis3d);

    return shape_rep;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
