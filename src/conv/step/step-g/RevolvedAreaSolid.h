/*                     R E V O L V E D A R E A S O L I D . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#ifndef CONV_STEP_STEP_G_REVOLVEDAREASOLID_H
#define CONV_STEP_STEP_G_REVOLVEDAREASOLID_H

#include "SweptAreaSolid.h"

class RevolvedAreaSolid : public SweptAreaSolid
{
private:
    static string entityname;
    static EntityInstanceFunc GetInstance;

public:
    RevolvedAreaSolid();
    RevolvedAreaSolid(STEPWrapper *sw, int step_id);
    virtual ~RevolvedAreaSolid();
    bool Load(STEPWrapper *sw, SDAI_Application_instance *sse);
    static STEPEntity *Create(STEPWrapper *sw, SDAI_Application_instance *sse);
};

#endif /* CONV_STEP_STEP_G_REVOLVEDAREASOLID_H */
