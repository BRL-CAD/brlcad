/*                 AdvancedFace.cpp
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
/** @file step/AdvancedFace.cpp
 *
 * Routines to convert STEP "AdvancedFace" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "AdvancedFace.h"
#include "QuasiUniformSurface.h"
#include "FaceOuterBound.h"

#define CLASSNAME "AdvancedFace"
#define ENTITYNAME "Advanced_Face"
string AdvancedFace::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)AdvancedFace::Create);

AdvancedFace::AdvancedFace()
{
    step = NULL;
    id = 0;
}

AdvancedFace::AdvancedFace(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
}

AdvancedFace::~AdvancedFace()
{
}

bool
AdvancedFace::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    if (!FaceSurface::Load(sw, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::FaceSurface." << std::endl;
	return false;
    }
    return true;
}

void
AdvancedFace::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Inherited Attributes:" << std::endl;
    FaceSurface::Print(level + 1);
}

STEPEntity *
AdvancedFace::GetInstance(STEPWrapper *sw, int id)
{
    return new AdvancedFace(sw, id);
}

STEPEntity *
AdvancedFace::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

bool
AdvancedFace::LoadONBrep(ON_Brep *brep)
{
    if (!brep || !FaceSurface::LoadONBrep(brep)) {
	std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }
    return true;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
