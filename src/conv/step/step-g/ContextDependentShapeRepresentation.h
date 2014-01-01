/*                 ContextDependentShapeRepresentation.h
 * BRL-CAD
 *
 * Copyright (c) 1994-2014 United States Government as represented by
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
/** @file step/ContextDependentShapeRepresentation.h
 *
 * Class definition used to convert STEP "ContextDependentShapeRepresentation" to BRL-CAD BREP
 * structures.
 *
 */

#ifndef CONV_STEP_STEP_G_CONTEXTDEPENDENTSHAPEREPRESENTATION_H
#define CONV_STEP_STEP_G_CONTEXTDEPENDENTSHAPEREPRESENTATION_H

#include "STEPEntity.h"

#include "sdai.h"

// forward declaration of class
class ON_Brep;
class RepresentationRelationship;
class ShapeRepresentationRelationship;
class ProductDefinitionShape;

typedef std::list<RepresentationRelationship *> LIST_OF_REPRESENTATION_RELATIONSHIPS;

class ContextDependentShapeRepresentation: virtual public STEPEntity
{
private:
    static string entityname;
    static EntityInstanceFunc GetInstance;

protected:
    LIST_OF_REPRESENTATION_RELATIONSHIPS representation_relation;
    ProductDefinitionShape *represented_product_relation;

public:
    ContextDependentShapeRepresentation();
    virtual ~ContextDependentShapeRepresentation();
    ContextDependentShapeRepresentation(STEPWrapper *sw, int step_id);
    bool Load(STEPWrapper *sw, SDAI_Application_instance *sse);
    virtual bool LoadONBrep(ON_Brep *brep);
    string ClassName();
    virtual void Print(int level);
    double GetLengthConversionFactor();
    double GetPlaneAngleConversionFactor();
    double GetSolidAngleConversionFactor();

    //static methods
    static STEPEntity *Create(STEPWrapper *sw, SDAI_Application_instance *sse);
};

#endif /* CONV_STEP_STEP_G_CONTEXTDEPENDENTSHAPEREPRESENTATION_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
