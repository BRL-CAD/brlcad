/*                         B O T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2024 United States Government as represented by
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
/** @file solid.cpp
 *
 * LS Dyna keyword file to BRL-CAD converter:
 * intermediate solid data implementation
 */

#include "common.h"

#include "solid.h"


Solid::Solid(void):name() {

}

Solid::~Solid(void){

}

void Solid::setName(const char* value)
{
    name = value;
}

void Solid::addArb(Arb arb)
{
    arbs.push_back(arb);
}



const char* Solid::getName(void) const
{
    return name.c_str();
}

std::vector<Arb> Solid::getArbs(void) const
{
    return arbs;
}

void Solid::write(rt_wdb* wdbp)
{
    for (int i = 0; i < arbs.size(); ++i) {
	arbs[i].write(wdbp);
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
