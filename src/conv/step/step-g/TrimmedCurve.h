/*                 TrimmedCurve.h
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
/** @file step/TrimmedCurve.h
 *
 * Class definition used to convert STEP "TrimmedCurve" to BRL-CAD BREP
 * structures.
 *
 */

#ifndef CONV_STEP_STEP_G_TRIMMEDCURVE_H
#define CONV_STEP_STEP_G_TRIMMEDCURVE_H

#include "BoundedCurve.h"

class TrimmingSelect;
typedef list<TrimmingSelect *> LIST_OF_TRIMMING_SELECT;

class TrimmedCurve : public BoundedCurve
{
private:
    static string entityname;
    static EntityInstanceFunc GetInstance;

protected:
    Curve *basis_curve;
    LIST_OF_TRIMMING_SELECT trim_1;
    LIST_OF_TRIMMING_SELECT trim_2;
    Boolean sense_agreement;
    Trimming_preference master_representation;

public:
    TrimmedCurve();
    virtual ~TrimmedCurve();
    TrimmedCurve(STEPWrapper *sw, int step_id);
    bool Load(STEPWrapper *sw, SDAI_Application_instance *sse);
    virtual bool LoadONBrep(ON_Brep *brep);
    virtual const double *PointAtEnd();
    virtual const double *PointAtStart();
    virtual void Print(int level);

    //static methods
    static STEPEntity *Create(STEPWrapper *sw, SDAI_Application_instance *sse);
};

#endif /* CONV_STEP_STEP_G_TRIMMEDCURVE_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
