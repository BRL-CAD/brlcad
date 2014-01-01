/* S H A P E _ R E P R E S E N T A T I O N _ R E L A T I O N S H I P . C P P
 *
 * BRL-CAD
 *
 * Copyright (c) 2013-2014 United States Government as represented by
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


#include "common.h"

#include "G_STEP_internal.h"

/* Shape Representation Relationship
 *
 * SHAPE_REPRESENTATION_RELATIONSHIP (SdaiShape_representation_relationship -> SdaiRepresentation_relationship)
 * SHAPE_REPRESENTATION (SdaiShape_representation -> SdaiRepresentation
 * AXIS2_PLACEMENT_3D (SdaiAxis2_placement_3d -> SdaiPlacement)
 * DIRECTION (two of these) (SdaiDirection -> SdaiGeometric_representation_item -> SdaiRepresentation_item)
 * CARTESIAN_POINT (SdaiCartesian_point)
 */
STEPentity *
Add_Shape_Representation_Relationship(Registry *registry, InstMgr *instance_list, SdaiRepresentation *shape_rep, SdaiRepresentation *manifold_shape)
{
    STEPentity *ret_entity = registry->ObjCreate("SHAPE_REPRESENTATION_RELATIONSHIP");
    instance_list->Append(ret_entity, completeSE);

    SdaiShape_representation_relationship *shape_rep_rel = (SdaiShape_representation_relationship *) ret_entity;
    shape_rep_rel->name_("''");
    shape_rep_rel->description_("''");
    shape_rep_rel->rep_1_(shape_rep);
    shape_rep_rel->rep_2_(manifold_shape);

    return ret_entity;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
