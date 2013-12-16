/*                 CartesianTransformationOperator.h
 * BRL-CAD
 *
 * Copyright (c) 1994-2013 United States Government as represented by
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
/** @file step/CartesianTransformationOperator.h
 *
 * Class definition used to convert STEP "CartesianTransformationOperator" to BRL-CAD BREP
 * structures.
 *
 */

#ifndef CONV_STEP_STEP_G_CARTESIANTRANSFORMATIONOPERATOR_H
#define CONV_STEP_STEP_G_CARTESIANTRANSFORMATIONOPERATOR_H

#include "GeometricRepresentationItem.h"
#include "FunctionallyDefinedTransformation.h"

class Direction;
class CartesianPoint;

class CartesianTransformationOperator : public GeometricRepresentationItem,
    public FunctionallyDefinedTransformation
{
private:
    static string entityname;
    static EntityInstanceFunc GetInstance;

protected:
    Direction *axis1;//optional
    Direction *axis2;//optional
    CartesianPoint *local_origin;
    double scale; //optional

public:
    CartesianTransformationOperator();
    virtual ~CartesianTransformationOperator();
    CartesianTransformationOperator(STEPWrapper *sw, int step_id);
    bool Load(STEPWrapper *sw, SDAI_Application_instance *sse);
    virtual bool LoadONBrep(ON_Brep *brep);
    virtual void Print(int level);

    //static methods
    static STEPEntity *Create(STEPWrapper *sw, SDAI_Application_instance *sse);
};

#endif /* CONV_STEP_STEP_G_CARTESIANTRANSFORMATIONOPERATOR_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
