/*                 CartesianTransformationOperator.cpp
 * BRL-CAD
 *
 * Copyright (c) 1994-2026 United States Government as represented by
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
/** @file step/CartesianTransformationOperator.cpp
 *
 * Routines to convert STEP "CartesianTransformationOperator" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "Direction.h"
#include "CartesianPoint.h"
#include "CartesianTransformationOperator3D.h"

#include "CartesianTransformationOperator.h"

#include <cmath>

#define CLASSNAME "CartesianTransformationOperator"
#define ENTITYNAME "Cartesian_Transformation_Operator"
string CartesianTransformationOperator::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)CartesianTransformationOperator::Create);

CartesianTransformationOperator::CartesianTransformationOperator()
{
    step = NULL;
    id = 0;
    axis1 = NULL;
    axis2 = NULL;
    local_origin = NULL;
    scale = 1.0;
}

CartesianTransformationOperator::CartesianTransformationOperator(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    axis1 = NULL;
    axis2 = NULL;
    local_origin = NULL;
    scale = 1.0;
}

CartesianTransformationOperator::~CartesianTransformationOperator()
{
}

bool
CartesianTransformationOperator::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    if (!GeometricRepresentationItem::Load(sw, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::Curve." << std::endl;
	sw->entity_status[id] = STEP_LOAD_ERROR;
	return false;
    }

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse, ENTITYNAME);

    if (axis1 == NULL) {
	SDAI_Application_instance *entity = step->getEntityAttribute(sse, "axis1");
	if (entity) { //this attribute is optional
	    axis1 = dynamic_cast<Direction *>(Factory::CreateObject(sw, entity));
	}
    }

    if (axis2 == NULL) {
	SDAI_Application_instance *entity = step->getEntityAttribute(sse, "axis2");
	if (entity) { // this attribute is optional
	    axis2 = dynamic_cast<Direction *>(Factory::CreateObject(sw, entity));
	}
    }

    if (local_origin == NULL) {
	SDAI_Application_instance *entity = step->getEntityAttribute(sse, "local_origin");
	if (entity) {
	    local_origin = dynamic_cast<CartesianPoint *>(Factory::CreateObject(sw, entity));
	} else {
	    std::cerr << CLASSNAME << ": error loading 'local_origin' attribute." << std::endl;
	    sw->entity_status[id] = STEP_LOAD_ERROR;
	    return false;
	}
    }

    STEPattribute *attr = step->getAttribute(sse, "scale");
    if (attr) { //this attribute is optional
	scale = step->getRealAttribute(sse, "scale");
    } else {
	scale = 1.0;
    }

    sw->entity_status[id] = STEP_LOADED;

    return true;
}

bool
CartesianTransformationOperator::GetONTransform(ON_Xform &xform,
    double length_factor) const
{
    if (!local_origin || !std::isfinite(scale) || scale <= 0.0 ||
	!std::isfinite(length_factor) || length_factor <= 0.0)
	return false;

    ON_3dVector xaxis(1.0, 0.0, 0.0);
    ON_3dVector yaxis(0.0, 1.0, 0.0);
    ON_3dVector zaxis(0.0, 0.0, 1.0);
    const CartesianTransformationOperator3D *operator_3d =
	dynamic_cast<const CartesianTransformationOperator3D *>(this);
    Direction *axis3 = operator_3d ? operator_3d->Axis3() : NULL;
    if (axis1) {
	const double *ratios = axis1->DirectionRatios();
	xaxis.Set(ratios[0], ratios[1], ratios[2]);
    }
    if (!xaxis.Unitize())
	return false;
    if (axis2) {
	const double *ratios = axis2->DirectionRatios();
	yaxis.Set(ratios[0], ratios[1], ratios[2]);
    }
    if (axis3 && !axis2) {
	const double *ratios = axis3->DirectionRatios();
	zaxis.Set(ratios[0], ratios[1], ratios[2]);
	if (!zaxis.Unitize())
	    return false;
	yaxis = ON_CrossProduct(zaxis, xaxis);
	if (!yaxis.Unitize())
	    return false;
	zaxis = ON_CrossProduct(xaxis, yaxis);
	if (!zaxis.Unitize())
	    return false;
    } else {
	yaxis -= (yaxis * xaxis) * xaxis;
	if (!yaxis.Unitize())
	    return false;
	zaxis = ON_CrossProduct(xaxis, yaxis);
	if (!zaxis.Unitize())
	    return false;
	if (axis3) {
	    const double *ratios = axis3->DirectionRatios();
	    const ON_3dVector asserted_z(ratios[0], ratios[1], ratios[2]);
	    if (zaxis * asserted_z < 0.0) {
		yaxis.Reverse();
		zaxis.Reverse();
	    }
	}
    }

    const double *origin = local_origin->Point3d();
    xform.Identity();
    xform[0][0] = scale * xaxis.x;
    xform[0][1] = scale * yaxis.x;
    xform[0][2] = scale * zaxis.x;
    xform[0][3] = length_factor * origin[0];
    xform[1][0] = scale * xaxis.y;
    xform[1][1] = scale * yaxis.y;
    xform[1][2] = scale * zaxis.y;
    xform[1][3] = length_factor * origin[1];
    xform[2][0] = scale * xaxis.z;
    xform[2][1] = scale * yaxis.z;
    xform[2][2] = scale * zaxis.z;
    xform[2][3] = length_factor * origin[2];
    return true;
}

void
CartesianTransformationOperator::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << GeometricRepresentationItem::name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Attributes:" << std::endl;
    if (axis1) {
	TAB(level + 1);
	std::cout << "axis1:" << std::endl;
	axis1->Print(level + 1);
    }
    if (axis2) {
	TAB(level + 1);
	std::cout << "axis2:" << std::endl;
	axis2->Print(level + 1);
    }
    TAB(level + 1);
    std::cout << "local_origin:" << std::endl;
    local_origin->Print(level + 1);

    TAB(level + 1);
    std::cout << "scale: " << scale << std::endl;

    TAB(level);
    std::cout << "Inherited Attributes:" << std::endl;
    GeometricRepresentationItem::Print(level + 1);
}

STEPEntity *
CartesianTransformationOperator::GetInstance(STEPWrapper *sw, int id)
{
    return new CartesianTransformationOperator(sw, id);
}

STEPEntity *
CartesianTransformationOperator::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

bool
CartesianTransformationOperator::LoadONBrep(ON_Brep *brep)
{
    std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << std::dec << ">) not implemented for " << entityname << std::endl;
    return false;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
