/*                 Axis2Placement.h
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
/** @file step/Axis2Placement.h
 *
 * Class definition used to convert STEP "Axis2Placement" to BRL-CAD BREP
 * structures.
 *
 */

#ifndef CONV_STEP_STEP_G_AXIS2PLACEMENT_H
#define CONV_STEP_STEP_G_AXIS2PLACEMENT_H

#include "STEPEntity.h"

class Placement;

class Axis2Placement : virtual public STEPEntity
{
private:
    static string entityname;
    static EntityInstanceFunc GetInstance;

public:
    enum axis2_placement_type {
	AXIS2_PLACEMENT_2D,
	AXIS2_PLACEMENT_3D,
	AXIS2_PLACEMENT_UNKNOWN
    };

protected:
    Placement *value;
    axis2_placement_type type;

public:
    Axis2Placement();
    virtual ~Axis2Placement();
    Axis2Placement(STEPWrapper *sw, int step_id);
    const double *GetOrigin();
    const double *GetNormal();
    const double *GetXAxis();
    const double *GetYAxis();
    bool Load(STEPWrapper *sw, SDAI_Application_instance *sse);
    virtual void Print(int level);

    //static methods
    static STEPEntity *Create(STEPWrapper *sw, SDAI_Application_instance *sse);
};

#endif /* CONV_STEP_STEP_G_AXIS2PLACEMENT_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
