/*                    M A P P E D I T E M . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#ifndef CONV_STEP_STEP_G_MAPPEDITEM_H
#define CONV_STEP_STEP_G_MAPPEDITEM_H

#include "RepresentationItem.h"

class RepresentationMap;

class MappedItem : public RepresentationItem
{
private:
    static string entityname;
    static EntityInstanceFunc GetInstance;

protected:
    RepresentationMap *mapping_source;
    RepresentationItem *mapping_target;

public:
    MappedItem();
    MappedItem(STEPWrapper *sw, int step_id);
    virtual ~MappedItem();
    bool Load(STEPWrapper *sw, SDAI_Application_instance *sse);
    virtual bool LoadONBrep(ON_Brep *brep);
    virtual void Print(int level);
    RepresentationMap *MappingSource() const { return mapping_source; }
    RepresentationItem *MappingTarget() const { return mapping_target; }

    static STEPEntity *Create(STEPWrapper *sw, SDAI_Application_instance *sse);
};

#endif /* CONV_STEP_STEP_G_MAPPEDITEM_H */
