/*                       S W E P T A R E A S O L I D . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#ifndef CONV_STEP_STEP_G_SWEPTAREASOLID_H
#define CONV_STEP_STEP_G_SWEPTAREASOLID_H

#include "SolidModel.h"

/** Factory object retaining a swept_area_solid representation item.
 * Schema adapters perform the exact schema-specific sweep conversion.
 */
class SweptAreaSolid : public SolidModel
{
private:
    static string entityname;
    static EntityInstanceFunc GetInstance;

public:
    SweptAreaSolid();
    SweptAreaSolid(STEPWrapper *sw, int step_id);
    virtual ~SweptAreaSolid();
    bool Load(STEPWrapper *sw, SDAI_Application_instance *sse);
    virtual bool LoadONBrep(ON_Brep *brep);
    static STEPEntity *Create(STEPWrapper *sw, SDAI_Application_instance *sse);
};

#endif /* CONV_STEP_STEP_G_SWEPTAREASOLID_H */
