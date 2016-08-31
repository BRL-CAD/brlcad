/*                 SurfaceCurve.cpp
 * BRL-CAD
 *
 * Copyright (c) 1994-2016 United States Government as represented by
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
/** @file step/SurfaceCurve.cpp
 *
 * Routines to convert STEP "SurfaceCurve" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"
#include "Surface.h"
#include "DefinitionalRepresentation.h"
#include "PCurveOrSurface.h"

#include "SurfaceCurve.h"

#define CLASSNAME "SurfaceCurve"
#define ENTITYNAME "Surface_Curve"
string SurfaceCurve::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)SurfaceCurve::Create);

static const char *Preferred_surface_curve_representation_string[] = {
    "curve_3d",
    "pcurve_s1",
    "pcurve_s2",
    "unset"
};

SurfaceCurve::SurfaceCurve()
{
    step = NULL;
    id = 0;
    curve_3d = NULL;
    master_representation = Preferred_surface_curve_representation_unset;
}

SurfaceCurve::SurfaceCurve(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    curve_3d = NULL;
    master_representation = Preferred_surface_curve_representation_unset;
}

SurfaceCurve::~SurfaceCurve()
{
    curve_3d = NULL;

    LIST_OF_PCURVE_OR_SURFACE::iterator i = associated_geometry.begin();

    while (i != associated_geometry.end()) {
	delete (*i);
	i = associated_geometry.erase(i);
    }
}

bool
SurfaceCurve::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    if (!Curve::Load(sw, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::Curve." << std::endl;
	goto step_error;
    }

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse, ENTITYNAME);

    if (curve_3d == NULL) {
	SDAI_Application_instance *entity = step->getEntityAttribute(sse, "curve_3d");
	if (entity) {
	    curve_3d = dynamic_cast<Curve *>(Factory::CreateObject(sw, entity)); //CreateCurveObject(sw,entity));
	}
	if (!entity || !curve_3d) {
	    std::cout << CLASSNAME << ":Error loading attribute 'curve_3d'." << std::endl;
	    goto step_error;
	}
    }

    /* TODO: get a sample to work with
       LIST_OF_ENTITIES *l = step->getListOfEntities(sse,"associated_geometry");
       LIST_OF_ENTITIES::iterator i;
       for (i=l->begin();i!=l->end();i++) {
       SDAI_Application_instance *entity = (*i);
       if (entity) {
       PCurveOrSurface *aPCOS = (PCurveOrSurface *)Factory::CreateObject(sw,entity);

       associated_geometry.push_back(aPCOS);
       } else {
       std::cerr << CLASSNAME  << ": Unhandled entity in attribute 'associated_geometry'." << std::endl;
       goto step_error;
       }
       }
       l->clear();
       delete l;
    */

    if (associated_geometry.empty()) {
	STEPattribute *attr = step->getAttribute(sse, "associated_geometry");
	if (attr) {
	    SelectAggregate *sa = static_cast<SelectAggregate *>(attr->ptr.a);
	    if (!sa) goto step_error;
	    SelectNode *sn = static_cast<SelectNode *>(sa->GetHead());

	    SDAI_Select *p_or_s;
	    while (sn != NULL) {
		p_or_s = static_cast<SDAI_Select *>(sn->node);
		if (!p_or_s) goto step_error;

		const TypeDescriptor *underlying_type = p_or_s->CurrentUnderlyingType();

		if (underlying_type == SCHEMA_NAMESPACE::e_pcurve ||
		    underlying_type == SCHEMA_NAMESPACE::e_surface)
		{
		    PCurveOrSurface *aPCOS = new PCurveOrSurface();

		    if (!aPCOS->Load(step, (SDAI_Application_instance *)p_or_s)) {
			std::cout << CLASSNAME << ":Error loading PCurveOrSurface select." << std::endl;
			delete aPCOS;
			goto step_error;
		    }
		    associated_geometry.push_back(aPCOS);
		} else {
		    std::cout << CLASSNAME << ":Unhandled select in attribute 'associated_geometry': " << p_or_s->CurrentUnderlyingType()->Description() << std::endl;
		    goto step_error;
		}
		sn = static_cast<SelectNode *>(sn->NextNode());
	    }
	}
    }

    /* TODO - is this something to fail on if get isn't successful? */
    master_representation = (Preferred_surface_curve_representation)step->getEnumAttribute(sse, "master_representation");

    sw->entity_status[id] = STEP_LOADED;
    return true;
step_error:
    sw->entity_status[id] = STEP_LOAD_ERROR;
    return false;
}

const double *
SurfaceCurve::PointAtEnd()
{
    return curve_3d->PointAtEnd();
}

const double *
SurfaceCurve::PointAtStart()
{
    return curve_3d->PointAtStart();
}

void
SurfaceCurve::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Attributes:" << std::endl;
    TAB(level + 1);
    std::cout << "curve_3d:" << std::endl;
    curve_3d->Print(level + 1);
    TAB(level + 1);
    std::cout << "associated_geometry:" << std::endl;
    LIST_OF_PCURVE_OR_SURFACE::iterator i;
    for (i = associated_geometry.begin(); i != associated_geometry.end(); i++) {
	(*i)->Print(level + 1);
    }

    TAB(level + 1);
    std::cout << "master_representation:" << Preferred_surface_curve_representation_string[master_representation] << std::endl;
}

STEPEntity *
SurfaceCurve::GetInstance(STEPWrapper *sw, int id)
{
    return new SurfaceCurve(sw, id);
}

STEPEntity *
SurfaceCurve::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
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
