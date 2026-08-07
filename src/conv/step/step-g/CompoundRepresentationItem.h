/*          C O M P O U N D R E P R E S E N T A T I O N I T E M . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#ifndef CONV_STEP_STEP_G_COMPOUNDREPRESENTATIONITEM_H
#define CONV_STEP_STEP_G_COMPOUNDREPRESENTATIONITEM_H

#include "RepresentationItem.h"

#include <vector>

/** A STEP compound_representation_item container.
 *
 * The selected item_element is either a LIST or SET of representation_item.
 * It is a grouping construct, not geometry in its own right.  Conversion code
 * recursively visits Elements() and retains STEP identity for styles and
 * diagnostics.
 */
class CompoundRepresentationItem : public RepresentationItem
{
private:
    static string entityname;
    static EntityInstanceFunc GetInstance;
    std::vector<RepresentationItem *> elements;

public:
    CompoundRepresentationItem();
    CompoundRepresentationItem(STEPWrapper *sw, int step_id);
    virtual ~CompoundRepresentationItem();

    bool Load(STEPWrapper *sw, SDAI_Application_instance *sse);
    const std::vector<RepresentationItem *> &Elements() const { return elements; }
    virtual bool LoadONBrep(ON_Brep *brep);
    virtual void Print(int level);

    static STEPEntity *Create(STEPWrapper *sw, SDAI_Application_instance *sse);
};

#endif /* CONV_STEP_STEP_G_COMPOUNDREPRESENTATIONITEM_H */
