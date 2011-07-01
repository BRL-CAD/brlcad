/*                    S T E P E N T I T Y . H
 * BRL-CAD
 *
 * Copyright (c) 2009-2011 United States Government as represented by
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
/** @file step/STEPEntity.h
 *
 * STEPEntity.h
 *
 */

#ifndef STEPENTITY_H_
#define STEPENTITY_H_

#include "common.h"

/* system headers */
#include <iostream>


class STEPWrapper;
class ON_Brep;


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

public:
	STEPEntity();
	virtual ~STEPEntity();

	int GetId() {return id;}
	int GetONId() {return ON_id;}
	void SetONId(int on_id) {ON_id = on_id;}
	int STEPid();
	STEPWrapper *Step();
};


#endif /* STEPENTITY_H_ */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
