/*                  S T E P E N T I T Y . C P P
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
/** @file step/STEPEntity.cpp
 *
 * STEPEntity.cpp
 *
 */

/* inteface header */
#include "STEPEntity.h"


STEPEntity::STEPEntity()
{
    step=NULL;
    id=0;
    ON_id = -1;
}


STEPEntity::~STEPEntity()
{
}


int
STEPEntity::STEPid()
{
    return id;
}


STEPWrapper *STEPEntity::Step()
{
    return step;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
