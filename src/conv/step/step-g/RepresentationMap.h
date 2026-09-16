/*              R E P R E S E N T A T I O N M A P . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#ifndef CONV_STEP_STEP_G_REPRESENTATIONMAP_H
#define CONV_STEP_STEP_G_REPRESENTATIONMAP_H

#include "STEPEntity.h"

class Representation;
class RepresentationItem;

class RepresentationMap : virtual public STEPEntity
{
private:
    static string entityname;
    static EntityInstanceFunc GetInstance;

protected:
    RepresentationItem *mapping_origin;
    Representation *mapped_representation;

public:
    RepresentationMap();
    RepresentationMap(STEPWrapper *sw, int step_id);
    virtual ~RepresentationMap();
    bool Load(STEPWrapper *sw, SDAI_Application_instance *sse);
    virtual void Print(int level);
    RepresentationItem *MappingOrigin() const { return mapping_origin; }
    Representation *MappedRepresentation() const { return mapped_representation; }

    static STEPEntity *Create(STEPWrapper *sw, SDAI_Application_instance *sse);
};

#endif /* CONV_STEP_STEP_G_REPRESENTATIONMAP_H */
