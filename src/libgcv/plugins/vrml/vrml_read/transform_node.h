/*		T R A N S F O R M _ N O D E. H
 * BRL-CAD
 *
 * Copyright (c) 2015-2021 United States Government as represented by
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
/** @file vrml/transform_node.h
 *
 * Class definition for class TRANSFORM
 *
 */

#ifndef TRANSFORM_NODE_H
#define TRANSFORM_NODE_H

#include "common.h"

#include "node.h"


class TRANSFORM
{
    public:
    void transformChild(NODE *pnode);
    void matrotate(double *Result, double Theta, double x, double y, double z);
    void transformSceneVert(vector<NODE *> &scenenoderef);
};


#endif


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
