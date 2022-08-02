/*			 N O D E _ T Y P E . H
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
/** @file vrml/node_type.h
 *
 * Class definition for class NODETYPE
 *
 */

#ifndef NODE_TYPE_H
#define NODE_TYPE_H

#include "common.h"

#include "node.h"

#define NODETYPEMAX             14

using namespace std;

class NODE;  //forward declaration of class

class NODETYPE
{
    public:
    int findNodeType(string instring);
    int findNodeType(NODE *innode);
};

#endif


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
