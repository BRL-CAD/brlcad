/*                 Axis2Placement3D.h
 * BRL-CAD
 *
 * Copyright (c) 1994-2025 United States Government as represented by
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
/** @file step/Axis2Placement3D.h
 *
 * Class definition used to convert STEP "Axis2Placement3D" to BRL-CAD BREP
 * structures.
 *
 */

#ifndef CONV_STEP_STEP_G_AXIS2PLACEMENT3D_H
#define CONV_STEP_STEP_G_AXIS2PLACEMENT3D_H

#include "Placement.h"

class Direction;

class Axis2Placement3D : public Placement
{
private:
    static string entityname;
    static EntityInstanceFunc GetInstance;
    double p[3][3];

    void BuildAxis();
    void FirstProjAxis(double *proj, double *zaxis, double *refdir);
    void ScalarTimesVector(double *v, double scalar, double *vec);
    void VectorDifference(double *result, double *v1, double *v2);
protected:
    Direction *axis;
    Direction *ref_direction;

public:
    Axis2Placement3D();
    virtual ~Axis2Placement3D();
    Axis2Placement3D(STEPWrapper *sw, int step_id);
    const double *GetAxis(int i);
    virtual const double *GetOrigin();
    virtual const double *GetNormal();
    virtual const double *GetXAxis();
    virtual const double *GetYAxis();
    const double *GetZAxis();
    bool Load(STEPWrapper *sw, SDAI_Application_instance *sse);
    virtual bool LoadONBrep(ON_Brep *brep);
    virtual void Print(int level);

    //static methods
    static STEPEntity *Create(STEPWrapper *sw, SDAI_Application_instance *sse);
};

#endif /* CONV_STEP_STEP_G_AXIS2PLACEMENT3D_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
