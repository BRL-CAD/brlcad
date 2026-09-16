/*                   E X T R U D E D A R E A S O L I D . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#ifndef CONV_STEP_STEP_G_EXTRUDEDAREASOLID_H
#define CONV_STEP_STEP_G_EXTRUDEDAREASOLID_H

#include "SweptAreaSolid.h"

class ExtrudedAreaSolid : public SweptAreaSolid
{
private:
    static string entityname;
    static EntityInstanceFunc GetInstance;

public:
    ExtrudedAreaSolid();
    ExtrudedAreaSolid(STEPWrapper *sw, int step_id);
    virtual ~ExtrudedAreaSolid();
    bool Load(STEPWrapper *sw, SDAI_Application_instance *sse);
    static STEPEntity *Create(STEPWrapper *sw, SDAI_Application_instance *sse);
};

#endif /* CONV_STEP_STEP_G_EXTRUDEDAREASOLID_H */
