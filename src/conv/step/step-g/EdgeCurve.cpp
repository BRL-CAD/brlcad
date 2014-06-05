/*                 EdgeCurve.cpp
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
/** @file step/EdgeCurve.cpp
 *
 * Routines to convert STEP "EdgeCurve" to BRL-CAD BREP
 * structures.
 *
 */
#include "STEPWrapper.h"
#include "Factory.h"

#include "Factory.h"

#include "EdgeCurve.h"
#include "VertexPoint.h"
// possible curve types
#include "QuasiUniformCurve.h"

#define CLASSNAME "EdgeCurve"
#define ENTITYNAME "Edge_Curve"
string EdgeCurve::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)EdgeCurve::Create);

EdgeCurve::EdgeCurve()
{
    step = NULL;
    id = 0;
    edge_start = NULL;
    edge_end = NULL;
    edge_geometry = NULL;
    same_sense = BUnset;
}

EdgeCurve::EdgeCurve(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    edge_start = NULL;
    edge_end = NULL;
    edge_geometry = NULL;
    same_sense = BUnset;
}

EdgeCurve::~EdgeCurve()
{
    edge_geometry = NULL;
}

bool
EdgeCurve::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    if (!Edge::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::Edge." << std::endl;
	return false;
    }

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse, ENTITYNAME);

    if (edge_geometry == NULL) {
	SDAI_Application_instance *entity = step->getEntityAttribute(sse, "edge_geometry");
	if (entity) {
	    edge_geometry = dynamic_cast<Curve *>(Factory::CreateObject(sw, entity)); //CreateCurveObject(sw,entity));
	    if (edge_geometry != NULL) {
		same_sense = step->getBooleanAttribute(sse, "same_sense");

		if (same_sense) {
		    edge_geometry->Start(edge_start);
		    edge_geometry->End(edge_end);
		} else {
		    edge_geometry->Start(edge_end);
		    edge_geometry->End(edge_start);
		}
	    } else {
		std::cout << CLASSNAME << ":Error loading member field \"edge_geometry\"." << std::endl;
		return false;
	    }
	} else {
	    std::cout << CLASSNAME << ":Error loading member field \"edge_geometry\"." << std::endl;
	    return false;
	}
    }

    return true;
}

void
EdgeCurve::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Attributes:" << std::endl;
    TAB(level + 1);
    std::cout << "edge_geometry:" << std::endl;
    edge_geometry->Print(level + 1);

    TAB(level + 1);
    std::cout << "same_sense:" << step->getBooleanString((Boolean)same_sense) << std::endl;

    TAB(level);
    std::cout << "Inherited Attributes:" << std::endl;
    Edge::Print(level + 1);
}

STEPEntity *
EdgeCurve::GetInstance(STEPWrapper *sw, int id)
{
    return new EdgeCurve(sw, id);
}

STEPEntity *
EdgeCurve::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
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
