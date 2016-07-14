/*                 OrientedFace.cpp
 * BRL-CAD
 *
 * Copyright (c) 1994-2014 United States Government as represented by
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
/** @file step/OrientedFace.cpp
 *
 * Routines to convert STEP "OrientedFace" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "OrientedFace.h"
#include "Face.h"
#include "FaceBound.h"
//#include "FaceOuterBound.h"

#define CLASSNAME "OrientedFace"
#define ENTITYNAME "Oriented_Face"
string OrientedFace::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)OrientedFace::Create);

OrientedFace::OrientedFace()
{
    step = NULL;
    id = 0;
    face_element = NULL;
    orientation = BUnset;
}

OrientedFace::OrientedFace(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    face_element = NULL;
    orientation = BUnset;
}

OrientedFace::~OrientedFace()
{
    face_element = NULL;
    bounds.clear();
}

bool
OrientedFace::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{

    step = sw;
    id = sse->STEPfile_id;

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse, ENTITYNAME);

    if (face_element == NULL) {
	SDAI_Application_instance *entity = step->getEntityAttribute(sse, "face_element");
	if (entity) {
	    face_element = dynamic_cast<Face *>(Factory::CreateObject(sw, entity));
	    /* load parent Face from face element */
	    if (!Face::Load(sw, entity)) {
		std::cout << CLASSNAME << ":Error loading base class ::Face." << std::endl;
		return false;
	    }
	}
    }

    orientation = step->getBooleanAttribute(sse, "orientation");

    if (!orientation) {
	face_element->ReverseFace();
    }
/*
    LIST_OF_FACE_BOUNDS *bounds = face_element->GetBounds();
    if (!bounds->empty()) {
	LIST_OF_FACE_BOUNDS::iterator i;
	for (i = bounds->begin(); i != bounds->end(); i++) {
	    FaceBound *aFB = (*i);
	    if (aFB) {
		if (!orientation) {
		    aFB->ReverseOrientation();
		}
	    }
	}
    }
*/
    return true;
}

void
OrientedFace::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Attributes:" << std::endl;
    TAB(level + 1);
    std::cout << "face_element:" << std::endl;
    face_element->Print(level + 1);

    TAB(level + 1);
    std::cout << "orientation:" << step->getBooleanString((Boolean)orientation) << std::endl;

    TAB(level);
    std::cout << "Inherited Attributes:" << std::endl;
    Face::Print(level + 1);
}

STEPEntity *
OrientedFace::GetInstance(STEPWrapper *sw, int id)
{
    return new OrientedFace(sw, id);
}

STEPEntity *
OrientedFace::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
