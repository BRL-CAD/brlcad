/*			 N O D E _ T Y P E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2015-2022 United States Government as represented by
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
 *
 */
/** @file vrml/node_type.cpp
 *
 * Routine for nodetype class (determine node type)
 *
 */

#include "common.h"

#include "node_type.h"

#include <cstring>

#include "bu.h"

#include "string_util.h"
#include "node.h"

using namespace std;

static const char *nodeTypeString[] = {
    "",
    "Appearance",
    "Box",
    "Cone",
    "Coordinate",
    "Cylinder",
    "Group",
    "IndexedFaceSet",
    "IndexedLineSet",
    "Material",
    "Shape",
    "Sphere",
    "Transform",
    "PROTO"
};


int
NODETYPE::findNodeType(string instring)
{
    int i;

    for (i = 1; i < NODETYPEMAX; i++) {
	if (bu_strcmp(instring.c_str(), nodeTypeString[i]) == 0) {
	    return i;
	}
    }

    return 0;
}

int
NODETYPE::findNodeType(NODE *innode)
{
    if (innode) {
	return innode->nnodetype;
    }
    return 0;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
