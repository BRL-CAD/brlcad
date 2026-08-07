/* GeometricallyBoundedWireframeShapeRepresentation.h
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#ifndef CONV_STEP_STEP_G_GEOMETRICALLYBOUNDEDWIREFRAMESHAPEREPRESENTATION_H
#define CONV_STEP_STEP_G_GEOMETRICALLYBOUNDEDWIREFRAMESHAPEREPRESENTATION_H

#include "ShapeRepresentation.h"

class GeometricallyBoundedWireframeShapeRepresentation : public ShapeRepresentation
{
private:
    static string entityname;
    static EntityInstanceFunc GetInstance;

public:
    GeometricallyBoundedWireframeShapeRepresentation();
    GeometricallyBoundedWireframeShapeRepresentation(STEPWrapper *sw, int step_id);
    virtual ~GeometricallyBoundedWireframeShapeRepresentation();
    bool Load(STEPWrapper *sw, SDAI_Application_instance *sse);
    virtual bool LoadONBrep(ON_Brep *brep);
    virtual void Print(int level);

    static STEPEntity *Create(STEPWrapper *sw, SDAI_Application_instance *sse);
};

#endif /* CONV_STEP_STEP_G_GEOMETRICALLYBOUNDEDWIREFRAMESHAPEREPRESENTATION_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 */
