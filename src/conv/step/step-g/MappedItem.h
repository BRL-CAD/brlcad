/*                 MappedItem.h
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
/** @file step/MappedItem.h
 *
 * Class definition used to convert STEP "MappedItem" to BRL-CAD
 * structures.  A mapped_item instances the geometry of a
 * representation_map's mapped_representation, transformed so that the
 * map's mapping_origin coincides with the item's mapping_target.
 *
 */

#ifndef CONV_STEP_STEP_G_MAPPEDITEM_H
#define CONV_STEP_STEP_G_MAPPEDITEM_H

#include "RepresentationItem.h"

// forward declaration of class
class RepresentationMap;
class Axis2Placement3D;
class ON_Brep;

class MappedItem : public RepresentationItem
{
private:
    static string entityname;
    static EntityInstanceFunc GetInstance;

protected:
    RepresentationMap *mapping_source;
    Axis2Placement3D *mapping_target;

public:
    MappedItem();
    virtual ~MappedItem();
    MappedItem(STEPWrapper *sw, int step_id);
    RepresentationMap *GetMappingSource() {
	return mapping_source;
    };
    Axis2Placement3D *GetMappingTarget() {
	return mapping_target;
    };
    bool Load(STEPWrapper *sw, SDAI_Application_instance *sse);
    virtual bool LoadONBrep(ON_Brep *brep);
    virtual void Print(int level);

    //static methods
    static STEPEntity *Create(STEPWrapper *sw, SDAI_Application_instance *sse);
};

#endif /* CONV_STEP_STEP_G_MAPPEDITEM_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
