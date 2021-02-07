/* M A N I F O L D S U R F A C E S H A P E R E P R E S E N T A T I O N . H
 * BRL-CAD
 *
 * Copyright (c) 2020-2021 United States Government as represented by
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
/** @file ManifoldSurfaceShapeRepresentation.h
 *
 * Brief description
 *
 */

#ifndef CONV_STEP_STEP_G_MANIFOLDSURFACESHAPEREPRESENTATION_H
#define CONV_STEP_STEP_G_MANIFOLDSURFACESHAPEREPRESENTATION_H

#include "common.h"

/* system interface headers */
#include <list>
#include <string>

/* must come before step headers */
#include "opennurbs.h"

/* interface headers */
#include "ShapeRepresentation.h"

// forward declaration of class
class Axis2Placement3D;

class ManifoldSurfaceShapeRepresentation : public ShapeRepresentation
{
private:
    static std::string entityname;
    static EntityInstanceFunc GetInstance;

protected:

public:
    ManifoldSurfaceShapeRepresentation();
    ManifoldSurfaceShapeRepresentation(STEPWrapper *sw, int step_id);
    virtual ~ManifoldSurfaceShapeRepresentation();

    Axis2Placement3D *GetAxis2Placement3d();

    bool Load(STEPWrapper *sw, SDAI_Application_instance *sse);
    std::string Name() {
	return name;
    };
    virtual void Print(int level);

    //static methods
    static STEPEntity *Create(STEPWrapper *sw, SDAI_Application_instance *sse);
};


#endif /* CONV_STEP_STEP_G_MANIFOLDSURFACESHAPEREPRESENTATION_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
