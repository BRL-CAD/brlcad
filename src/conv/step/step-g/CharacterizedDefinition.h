/*                 CharacterizedDefinition.h
 * BRL-CAD
 *
 * Copyright (c) 1994-2012 United States Government as represented by
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
/** @file step/CharacterizedDefinition.h
 *
 * Class definition used to convert STEP "CharacterizedDefinition" to BRL-CAD BREP
 * structures.
 *
 */

#ifndef CONV_STEP_STEP_G_CHARACTERIZEDDEFINITION_H
#define CONV_STEP_STEP_G_CHARACTERIZEDDEFINITION_H

#include "STEPEntity.h"

#include "sdai.h"

class CharacterizedProductDefinition;
class ShapeDefinition;

class CharacterizedDefinition: virtual public STEPEntity {
public:
    enum CharacterizedDefinition_type {
#ifdef AP203e
	CHARACTERIZED_OBJECT,
#endif
	CHARACTERIZED_PRODUCT_DEFINITION,
	SHAPE_DEFINITION,
	UNKNOWN
    };

private:
    static string entityname;
    static EntityInstanceFunc GetInstance;

    STEPEntity *definition;
    CharacterizedDefinition_type type;

protected:

public:
    CharacterizedDefinition();
    CharacterizedDefinition(STEPWrapper *sw, int step_id);
    virtual ~CharacterizedDefinition();
    CharacterizedProductDefinition *GetCharacterizedProductDefinition();
    ShapeDefinition *GetShapeDefinition();
    string GetProductName();bool Load(STEPWrapper *sw, SDAI_Select *sse);
    virtual bool LoadONBrep(ON_Brep *brep);
    virtual void Print(int level);
    virtual CharacterizedDefinition_type CharacterizedDefinitionType()
    {
	return type;
    };

    //static methods
    static STEPEntity *Create(STEPWrapper *sw, SDAI_Application_instance *sse);
};

#endif /* CONV_STEP_STEP_G_CHARACTERIZEDDEFINITION_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
