/* S H A P E _ R E P R E S E N T A T I O N _ R E L A T I O N S H I P . C P P
 *
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
/** @file Shape_Representation_Relationship.cpp
 *
 */

#include "AP_Common.h"
#include "STEPGeneratedAPI.h"
#include "Shape_Representation_Relationship.h"

/* AP242 edition 2 widened rep_1 and rep_2 from REPRESENTATION to
 * REPRESENTATION_OR_REPRESENTATION_REFERENCE.  Setting the standardized
 * EXPRESS attributes through STEPcode's public conversion API works for both
 * the direct-reference attributes in the older APs and the later SELECT, and
 * avoids coupling common export code to one generated SELECT class. */
bool
Set_Representation_Relationship_Reference(STEPentity *relationship,
	const char *attribute_name,
	STEPentity *representation, InstMgr *instances)
{
    (void)instances;
    return brlcad::step::SetEntity(relationship, attribute_name,
	    representation);
}

/* Shape Representation Relationship
 *
 * SHAPE_REPRESENTATION_RELATIONSHIP (SdaiShape_representation_relationship -> SdaiRepresentation_relationship)
 * SHAPE_REPRESENTATION (SdaiShape_representation -> SdaiRepresentation
 * AXIS2_PLACEMENT_3D (SdaiAxis2_placement_3d -> SdaiPlacement)
 * DIRECTION (two of these) (SdaiDirection -> SdaiGeometric_representation_item -> SdaiRepresentation_item)
 * CARTESIAN_POINT (SdaiCartesian_point)
 */
STEPentity *
Add_Shape_Representation_Relationship(AP203_Contents *sc,
	STEPentity *shape_rep, STEPentity *manifold_shape)
{
    STEPentity *shape_rep_rel = brlcad::step::CreateEntity(sc->registry,
	    sc->instance_list, "SHAPE_REPRESENTATION_RELATIONSHIP");
    brlcad::step::SetString(shape_rep_rel, "name", "");
    brlcad::step::SetString(shape_rep_rel, "description", "");
    if (!Set_Representation_Relationship_Reference(shape_rep_rel, "rep_1", shape_rep,
	    sc->instance_list) ||
	    !Set_Representation_Relationship_Reference(shape_rep_rel, "rep_2", manifold_shape,
	    sc->instance_list))
	return NULL;

    return shape_rep_rel;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
