/*         S H A P E _ R E P R E S E N T A T I O N . C P P
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
/** @file Shape_Representation.cpp
 *
 */

#include "AP_Common.h"
#include "Shape_Representation.h"

SdaiRepresentation *
Add_Shape_Representation(AP203_Contents *sc, SdaiRepresentation_context *context)
{

    SdaiShape_representation *shape_rep = (SdaiShape_representation *)sc->registry->ObjCreate("SHAPE_REPRESENTATION");
    sc->instance_list->Append((STEPentity *)shape_rep, completeSE);
    shape_rep->name_("''");
    shape_rep->context_of_items_(context);

    EntityAggregate *axis_items = shape_rep->items_();

    /* create an axis */

    SdaiAxis2_placement_3d *axis3d = (SdaiAxis2_placement_3d *)sc->registry->ObjCreate("AXIS2_PLACEMENT_3D");
    sc->instance_list->Append((STEPentity *)axis3d, completeSE);
    axis3d->name_("''");

    /* set the axis origin */

    SdaiCartesian_point *origin= (SdaiCartesian_point *)sc->registry->ObjCreate("CARTESIAN_POINT");
    sc->instance_list->Append((STEPentity *)origin, completeSE);

    RealNode *xnode = new RealNode();
    xnode->value = 0.0;
    RealNode *ynode= new RealNode();
    ynode->value = 0.0;
    RealNode *znode= new RealNode();
    znode->value = 0.0;
    origin->coordinates_()->AddNode(xnode);
    origin->coordinates_()->AddNode(ynode);
    origin->coordinates_()->AddNode(znode);
    origin->name_("''");
    axis3d->location_(origin);

    /* set the axis up direction (i-vector) */

    SdaiDirection *axis = (SdaiDirection *)sc->registry->ObjCreate("DIRECTION");
    sc->instance_list->Append((STEPentity *)axis, completeSE);

    RealNode *axis_xnode = new RealNode();
    axis_xnode->value = 0.0;
    RealNode *axis_ynode= new RealNode();
    axis_ynode->value = 0.0;
    RealNode *axis_znode= new RealNode();
    axis_znode->value = 1.0;
    axis->direction_ratios_()->AddNode(axis_xnode);
    axis->direction_ratios_()->AddNode(axis_ynode);
    axis->direction_ratios_()->AddNode(axis_znode);
    axis->name_("''");
    axis3d->axis_(axis);

    /* add the axis front direction (j-vector) */

    SdaiDirection *ref_dir = (SdaiDirection *)sc->registry->ObjCreate("DIRECTION");
    sc->instance_list->Append((STEPentity *)ref_dir, completeSE);

    RealNode *ref_dir_xnode = new RealNode();
    ref_dir_xnode->value = 1.0;
    RealNode *ref_dir_ynode= new RealNode();
    ref_dir_ynode->value = 0.0;
    RealNode *ref_dir_znode= new RealNode();
    ref_dir_znode->value = 0.0;
    ref_dir->direction_ratios_()->AddNode(ref_dir_xnode);
    ref_dir->direction_ratios_()->AddNode(ref_dir_ynode);
    ref_dir->direction_ratios_()->AddNode(ref_dir_znode);
    ref_dir->name_("''");
    axis3d->ref_direction_(ref_dir);

    /* add the axis to the shape definition */

    axis_items->AddNode(new EntityNode((SDAI_Application_instance *)axis3d));

    return (SdaiRepresentation *)shape_rep;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
