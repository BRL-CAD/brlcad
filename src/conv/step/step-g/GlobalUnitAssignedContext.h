/*                 GlobalUnitAssignedContext.h
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
/** @file step/GlobalUnitAssignedContext.h
 *
 * Class definition used to convert STEP "GlobalUnitAssignedContext" to BRL-CAD BREP
 * structures.
 *
 */

#ifndef CONV_STEP_STEP_G_GLOBALUNITASSIGNEDCONTEXT_H
#define CONV_STEP_STEP_G_GLOBALUNITASSIGNEDCONTEXT_H

#include "RepresentationContext.h"

class Unit;
typedef list<Unit *> LIST_OF_UNITS;

class GlobalUnitAssignedContext : public RepresentationContext
{
private:
    static string entityname;
    static EntityInstanceFunc GetInstance;

protected:
    LIST_OF_UNITS units;

public:
    GlobalUnitAssignedContext();
    virtual ~GlobalUnitAssignedContext();
    GlobalUnitAssignedContext(STEPWrapper *sw, int step_id);
    double GetLengthConversionFactor();
    double GetPlaneAngleConversionFactor();
    double GetSolidAngleConversionFactor();
    bool Load(STEPWrapper *sw, SDAI_Application_instance *sse);
    virtual void Print(int level);

    //static methods
    static STEPEntity *Create(STEPWrapper *sw, SDAI_Application_instance *sse);
};

#endif /* CONV_STEP_STEP_G_GLOBALUNITASSIGNEDCONTEXT_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
