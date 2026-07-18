/*       F A C E T E D B R E P S H A P E R E P R E S E N T A T I O N . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#ifndef CONV_STEP_STEP_G_FACETEDBREPSHAPEREPRESENTATION_H
#define CONV_STEP_STEP_G_FACETEDBREPSHAPEREPRESENTATION_H

#include "ShapeRepresentation.h"

class FacetedBrepShapeRepresentation : public ShapeRepresentation
{
private:
    static string entityname;
    static EntityInstanceFunc GetInstance;

public:
    FacetedBrepShapeRepresentation();
    FacetedBrepShapeRepresentation(STEPWrapper *sw, int step_id);
    virtual ~FacetedBrepShapeRepresentation();
    bool Load(STEPWrapper *sw, SDAI_Application_instance *sse);
    virtual void Print(int level);

    static STEPEntity *Create(STEPWrapper *sw, SDAI_Application_instance *sse);
};

#endif /* CONV_STEP_STEP_G_FACETEDBREPSHAPEREPRESENTATION_H */
