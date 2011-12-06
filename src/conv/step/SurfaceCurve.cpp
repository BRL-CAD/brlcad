/*                 SurfaceCurve.cpp
 * BRL-CAD
 *
 * Copyright (c) 1994-2011 United States Government as represented by
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
string SurfaceCurve::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)SurfaceCurve::Create);

static const char *Preferred_surface_curve_representation_string[] = {
    "curve_3d",
    "pcurve_s1",
    "pcurve_s2",
    "unset"
};

SurfaceCurve::SurfaceCurve() {
    step = NULL;
    id = 0;
    curve_3d = NULL;
}

SurfaceCurve::SurfaceCurve(STEPWrapper *sw,int step_id) {
    step = sw;
    id = step_id;
    curve_3d = NULL;
}

SurfaceCurve::~SurfaceCurve() {
    curve_3d = NULL;
    associated_geometry.clear();
}

bool
SurfaceCurve::Load(STEPWrapper *sw,SCLP23(Application_instance) *sse) {
    step=sw;
    id = sse->STEPfile_id;

    if ( !Curve::Load(sw,sse) ) {
	std::cout << CLASSNAME << ":Error loading base class ::Curve." << std::endl;
	return false;
    }

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse,ENTITYNAME);

    if (curve_3d ==NULL) {
	SCLP23(Application_instance) *entity = step->getEntityAttribute(sse,"curve_3d");
	if (entity) {
	    curve_3d = dynamic_cast<Curve *>(Factory::CreateObject(sw,entity)); //CreateCurveObject(sw,entity));
	} else {
	    std::cout << CLASSNAME << ":Error loading attribute 'curve_3d'." << std::endl;
	    return false;
	}
    }

    /* TODO: get a sample to work with
       LIST_OF_ENTITIES *l = step->getListOfEntities(sse,"associated_geometry");
       LIST_OF_ENTITIES::iterator i;
       for(i=l->begin();i!=l->end();i++) {
       SCLP23(Application_instance) *entity = (*i);
       if (entity) {
       PCurveOrSurface *aPCOS = (PCurveOrSurface *)Factory::CreateObject(sw,entity);

       associated_geometry.push_back(aPCOS);
       } else {
       std::cerr << CLASSNAME  << ": Unhandled entity in attribute 'associated_geometry'." << std::endl;
       return false;
       }
       }
       l->clear();
       delete l;
    */

    if (associated_geometry.empty()) {
	STEPattribute *attr = step->getAttribute(sse, "associated_geometry");
	if (attr) {
	    STEPaggregate *sa = (STEPaggregate *) (attr->ptr.a);
	    EntityNode *sn = (EntityNode *) sa->GetHead();
	    SCLP23(Select) *p_or_s;
	    while (sn != NULL) {
		p_or_s = (SCLP23(Select) *) sn->node;

		if (p_or_s->CurrentUnderlyingType() == config_control_designt_pcurve_or_surface) {
		    PCurveOrSurface *aPCOS = new PCurveOrSurface();

		    associated_geometry.push_back(aPCOS);

		    if (!aPCOS->Load(step, p_or_s)) {
			std::cout << CLASSNAME << ":Error loading PCurveOrSurface select." << std::endl;
			return false;
		    }
		} else {
		    std::cout << CLASSNAME << ":Unhandled select in attribute 'associated_geometry': " << p_or_s->CurrentUnderlyingType()->Description() << std::endl;
		    return false;
		}
		sn = (EntityNode *) sn->NextNode();
	    }
	}
    }

    master_representation = (Preferred_surface_curve_representation)step->getEnumAttribute(sse,"master_representation");

    return true;
}

const double *
SurfaceCurve::PointAtEnd() {
    return curve_3d->PointAtEnd();
}

const double *
SurfaceCurve::PointAtStart() {
    return curve_3d->PointAtStart();
}

void
SurfaceCurve::Print(int level) {
    TAB(level); std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level); std::cout << "Attributes:" << std::endl;
    TAB(level+1); std::cout << "curve_3d:" << std::endl;
    curve_3d->Print(level+1);
    TAB(level+1); std::cout << "associated_geometry:" << std::endl;
    LIST_OF_PCURVE_OR_SURFACE::iterator i;
    for(i=associated_geometry.begin();i!=associated_geometry.end();i++) {
	(*i)->Print(level+1);
    }

    TAB(level+1); std::cout << "master_representation:" << Preferred_surface_curve_representation_string[master_representation] << std::endl;
}

STEPEntity *
SurfaceCurve::Create(STEPWrapper *sw,SCLP23(Application_instance) *sse){
    Factory::OBJECTS::iterator i;
    if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
	SurfaceCurve *object = new SurfaceCurve(sw,sse->STEPfile_id);

	Factory::AddObject(object);

	if (!object->Load(sw, sse)) {
	    std::cerr << CLASSNAME << ":Error loading class in ::Create() method." << std::endl;
	    delete object;
	    return NULL;
	}
	return static_cast<STEPEntity *>(object);
    } else {
	return (*i).second;
    }
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
