/*                         C s g S o l i d . h
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#ifndef CONV_STEP_STEP_G_CSGSOLID_H
#define CONV_STEP_STEP_G_CSGSOLID_H

#include "SolidModel.h"

/** Lightweight factory object for a CSG_SOLID representation item.
 *
 * Schema-specific adapters retain and convert the CSG expression.  This
 * shared class lets Representation load the item without discarding its
 * context, product mapping, or siblings.
 */
class CsgSolid : public SolidModel
{
private:
    static string entityname;
    static EntityInstanceFunc GetInstance;

public:
    CsgSolid();
    CsgSolid(STEPWrapper *sw, int step_id);
    virtual ~CsgSolid();
    bool Load(STEPWrapper *sw, SDAI_Application_instance *sse);
    virtual bool LoadONBrep(ON_Brep *brep);
    static STEPEntity *Create(STEPWrapper *sw, SDAI_Application_instance *sse);
};

#endif /* CONV_STEP_STEP_G_CSGSOLID_H */
