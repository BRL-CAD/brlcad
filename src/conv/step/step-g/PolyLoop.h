/*                 P O L Y L O O P . H
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

#ifndef CONV_STEP_STEP_G_POLYLOOP_H
#define CONV_STEP_STEP_G_POLYLOOP_H

#include "Loop.h"

class CartesianPoint;
typedef list<CartesianPoint *> LIST_OF_POINTS;

/** The polygonal loop used by classic AP203 faceted_brep topology. */
class PolyLoop : public Loop
{
private:
    static string entityname;
    static EntityInstanceFunc GetInstance;

protected:
    LIST_OF_POINTS polygon;

public:
    PolyLoop();
    PolyLoop(STEPWrapper *sw, int step_id);
    virtual ~PolyLoop();
    bool Load(STEPWrapper *sw, SDAI_Application_instance *sse);
    virtual bool LoadONBrep(ON_Brep *brep);
    virtual void Print(int level);
    const LIST_OF_POINTS &Polygon() const { return polygon; }

    static STEPEntity *Create(STEPWrapper *sw, SDAI_Application_instance *sse);
};

#endif /* CONV_STEP_STEP_G_POLYLOOP_H */
