/*      S H E L L B A S E D S U R F A C E M O D E L . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file ShellBasedSurfaceModel.cpp
 *
 * Brief description
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "ShellBasedSurfaceModel.h"
#include "ClosedShell.h"
#include "OpenShell.h"

#include <algorithm>

#define CLASSNAME "ShellBasedSurfaceModel"
#define ENTITYNAME "Shell_Based_Surface_Model"
string ShellBasedSurfaceModel::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)ShellBasedSurfaceModel::Create);

ShellBasedSurfaceModel::ShellBasedSurfaceModel()
{
    step = NULL;
    id = 0;
    sbsm_boundary.clear();
}

ShellBasedSurfaceModel::ShellBasedSurfaceModel(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    sbsm_boundary.clear();
}

ShellBasedSurfaceModel::~ShellBasedSurfaceModel()
{
    sbsm_boundary.clear();
}

bool
ShellBasedSurfaceModel::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    // load base class attributes
    if (!GeometricRepresentationItem::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::GeometricRepresentationItem." << std::endl;
	sw->entity_status[id] = STEP_LOAD_ERROR;
	return false;
    }

    if (sbsm_boundary.empty()) {
	STEPattribute *attr = step->getAttribute(sse, "sbsm_boundary");
	if (attr) {
	    SelectAggregate *sa = static_cast<SelectAggregate *>(attr->ptr.a);
	    if (!sa) goto step_error;
	    SelectNode *sn = static_cast<SelectNode *>(sa->GetHead());

	    SDAI_Select *stype;
	    while (sn != NULL) {
		stype = static_cast<SDAI_Select *>(sn->node);
		SdaiShell *shell = static_cast<SdaiShell *>(stype);
		const TypeDescriptor *underlying_type = stype->CurrentUnderlyingType();
		ConnectedFaceSet *boundary = NULL;
		if (underlying_type == SCHEMA_NAMESPACE::e_open_shell || shell->IsOpen_shell()) {
		    if (shell->IsOpen_shell()) {
			SdaiOpen_shell *oshell = *shell;
			boundary = dynamic_cast<OpenShell *>(Factory::CreateObject(sw, (SDAI_Application_instance *)oshell));
		    }
		} else if (underlying_type == SCHEMA_NAMESPACE::e_closed_shell || shell->IsClosed_shell()) {
		    SdaiClosed_shell *cshell = *shell;
		    boundary = dynamic_cast<ClosedShell *>(Factory::CreateObject(sw, (SDAI_Application_instance *)cshell));
		}
		if (boundary)
		    sbsm_boundary.push_back(boundary);
		sn = (SelectNode *)sn->NextNode();
	    }
	}
    }

    sw->entity_status[id] = STEP_LOADED;
    return true;
step_error:
    sw->entity_status[id] = STEP_LOAD_ERROR;
    return false;
}

void
ShellBasedSurfaceModel::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level + 1);
    std::cout << "sbsm_boundary:" << std::endl;
    LIST_OF_SHELL_BOUNDARIES::iterator i;
    for (i = sbsm_boundary.begin(); i != sbsm_boundary.end(); ++i) {
	(*i)->Print(level + 1);
    }

    GeometricRepresentationItem::Print(level + 1);
}

size_t
ShellBasedSurfaceModel::MaximumPullbackSpanEstimate() const
{
    size_t maximum = 1;
    for (LIST_OF_SHELL_BOUNDARIES::const_iterator boundary =
	    sbsm_boundary.begin(); boundary != sbsm_boundary.end(); ++boundary) {
	if (*boundary)
	    maximum = std::max(maximum,
		(*boundary)->MaximumPullbackSpanEstimate());
    }
    return maximum;
}

STEPEntity *
ShellBasedSurfaceModel::GetInstance(STEPWrapper *sw, int id)
{
    return new ShellBasedSurfaceModel(sw, id);
}

STEPEntity *
ShellBasedSurfaceModel::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
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
