/*                 F A C E T E D B R E P . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#ifndef CONV_STEP_STEP_G_FACETEDBREP_H
#define CONV_STEP_STEP_G_FACETEDBREP_H

#include "ManifoldSolidBrep.h"

class FacetedBrep : public ManifoldSolidBrep
{
private:
    static string entityname;
    static EntityInstanceFunc GetInstance;

public:
    FacetedBrep();
    FacetedBrep(STEPWrapper *sw, int step_id);
    virtual ~FacetedBrep();
    bool Load(STEPWrapper *sw, SDAI_Application_instance *sse);
    virtual void Print(int level);

    static STEPEntity *Create(STEPWrapper *sw, SDAI_Application_instance *sse);
};

#endif /* CONV_STEP_STEP_G_FACETEDBREP_H */
