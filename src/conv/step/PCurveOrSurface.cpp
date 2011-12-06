/*                 PCurveOrSurface.cpp
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
/** @file step/PCurveOrSurface.cpp
 *
 * Routines to convert STEP "PCurveOrSurface" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "Surface.h"
#include "DefinitionalRepresentation.h"
#include "PCurve.h"
#include "Surface.h"

#include "PCurveOrSurface.h"

#define CLASSNAME "PCurveOrSurface"
#define ENTITYNAME "Pcurve_Or_Surface"
string PCurveOrSurface::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)PCurveOrSurface::Create);

const char *pcurve_or_surface_type_names[] = {
    "PCURVE",
    "SURFACE",
    NULL
};

PCurveOrSurface::PCurveOrSurface() {
    step = NULL;
    id = 0;
    pcurve = NULL;
    surface = NULL;
}

PCurveOrSurface::PCurveOrSurface(STEPWrapper *sw,int step_id) {
    step = sw;
    id = step_id;
    pcurve = NULL;
    surface = NULL;
}

PCurveOrSurface::~PCurveOrSurface() {
    pcurve = NULL;
    surface = NULL;
}

bool
PCurveOrSurface::Load(STEPWrapper *sw,SCLP23(Select) *sse) {
    step=sw;

    std::cout << sse->UnderlyingTypeName() << std::endl;
    SdaiPcurve_or_surface *v = (SdaiPcurve_or_surface *)sse;

    if ( v->IsPcurve() ) {
	type = PCURVE;
	SdaiPcurve *p = *v;
	pcurve = dynamic_cast<PCurve *>(Factory::CreateObject(sw,(SCLP23(Application_instance) *)p)); //CreateCurveObject(sw,(SCLP23(Application_instance) *)p));
    } else if (v->IsSurface()) {
	type = SURFACE;
	SdaiSurface *s = *v;
	surface = dynamic_cast<Surface *>(Factory::CreateObject(sw,(SCLP23(Application_instance)*)s)); //CreateSurfaceObject(sw,(SCLP23(Application_instance)*)s));
    }

    return true;
}

void
PCurveOrSurface::Print(int level) {
    TAB(level); std::cout << CLASSNAME << ":" << std::endl;
    if (type == PCURVE) {
	TAB(level); std::cout << "Type:" << pcurve_or_surface_type_names[type] << " Value:" << std::endl;
	pcurve->Print(level+1);
    } else if (type == SURFACE) {
	TAB(level); std::cout << "Type:" << pcurve_or_surface_type_names[type] << " Value:" << std::endl;
	surface->Print(level+1);
    }
}
STEPEntity *
PCurveOrSurface::Create(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
    Factory::OBJECTS::iterator i;
    if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
	PCurveOrSurface *object = new PCurveOrSurface(sw,sse->STEPfile_id);

	Factory::AddObject(object);

	if (!object->Load(sw, (SCLP23(Select) *)sse)) {
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
