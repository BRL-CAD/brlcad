/*                 GeometricallyBoundedSurfaceShapeRepresentation.h
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
/** @file step/GeometricallyBoundedSurfaceShapeRepresentation.h
 *
 * Class definition used to convert STEP "GeometricallyBoundedSurfaceShapeRepresentation" to BRL-CAD BREP
 * structures.
 *
 */
#ifndef CONV_STEP_STEP_G_GEOMETRICALLYBOUNDEDSURFACESHAPEREPRESENTATION_H
#define CONV_STEP_STEP_G_GEOMETRICALLYBOUNDEDSURFACESHAPEREPRESENTATION_H

#include "common.h"

/* interface header */
#include "ShapeRepresentation.h"

/* system headers */
#include <string>


class ON_Brep;

class GeometricallyBoundedSurfaceShapeRepresentation : public ShapeRepresentation
{
private:
    static std::string entityname;
    static EntityInstanceFunc GetInstance;

protected:

public:
    GeometricallyBoundedSurfaceShapeRepresentation();
    GeometricallyBoundedSurfaceShapeRepresentation(STEPWrapper *sw, int step_id);
    virtual ~GeometricallyBoundedSurfaceShapeRepresentation();
    bool Load(STEPWrapper *sw, SDAI_Application_instance *sse);
    virtual void Print(int level);

    //static methods
    static STEPEntity *Create(STEPWrapper *sw, SDAI_Application_instance *sse);
};


#endif /* CONV_STEP_STEP_G_GEOMETRICALLYBOUNDEDSURFACESHAPEREPRESENTATION_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
