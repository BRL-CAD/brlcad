/*                 Polyline.h
 * BRL-CAD
 *
 * Copyright (c) 1994-2020 United States Government as represented by
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
/** @file step/Polyline.h
 *
 * Class definition used to convert STEP "Polyline" to BRL-CAD BREP
 * structures.
 *
 */

#ifndef CONV_STEP_STEP_G_POLYLINE_H
#define CONV_STEP_STEP_G_POLYLINE_H

#include "BoundedCurve.h"

class PolylineSegment;
typedef list<CartesianPoint *> LIST_OF_POINTS;

class Polyline : public BoundedCurve
{
private:
    static string entityname;
    static EntityInstanceFunc GetInstance;

protected:
    LIST_OF_POINTS points;

public:
    Polyline();
    virtual ~Polyline();
    Polyline(STEPWrapper *sw, int step_id);
    bool Load(STEPWrapper *sw, SDAI_Application_instance *sse);
    virtual const double *PointAtEnd();
    virtual const double *PointAtStart();
    virtual void Print(int level);

    //static methods
    static STEPEntity *Create(STEPWrapper *sw, SDAI_Application_instance *sse);
};

#endif /* CONV_STEP_STEP_G_POLYLINE_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
