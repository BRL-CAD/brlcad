/*                    S T E P E N T I T Y . H
 * BRL-CAD
 *
 * Copyright (c) 2009-2014 United States Government as represented by
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

#ifndef CONV_STEP_STEP_G_STEPENTITY_H
#define CONV_STEP_STEP_G_STEPENTITY_H

#include "common.h"

/* system headers */
#include <iostream>

#include "STEPWrapper.h"

class ON_Brep;
class STEPEntity;

// A generic pseudo-constructor function type. Descendants of STEPEntity have
// a private static member of this type (GetInstance) which returns a pointer
// to a new instance of the subtype cast to a STEPEntity*.
typedef STEPEntity *(EntityInstanceFunc)(STEPWrapper *sw, int id);

#define POINT_CLOSENESS_TOLERANCE 1e-6
#define TAB(j) \
	{ \
		for ( int tab_index=0; tab_index< j; tab_index++) \
			std::cout << "    "; \
	}

class STEPEntity
{
protected:
    int id;
    int ON_id;
    STEPWrapper *step;
    static STEPEntity *CreateEntity(
	STEPWrapper *sw,
	SDAI_Application_instance *sse,
	EntityInstanceFunc Instance,
	const char *classname);

public:
    STEPEntity();
    virtual ~STEPEntity();

    int GetId() {
	return id;
    }
    void SetId(int nid) {
	id = nid;
    }
    int GetONId() {
	return ON_id;
    }
    void SetONId(int on_id) {
	ON_id = on_id;
    }
    int STEPid();
    STEPWrapper *Step();
    virtual bool Load(STEPWrapper *UNUSED(sw), SDAI_Application_instance *UNUSED(sse)) {
	return false;
    };
};


#endif /* CONV_STEP_STEP_G_STEPENTITY_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
