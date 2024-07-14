/*                         A R B S . C P P
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
/** @file arbs.cpp
 *
 * LS Dyna keyword file to BRL-CAD converter:
 * intermediate arbs data implementation
 */

#include "common.h"

#include "arbs.h"


Arbs::Arbs(void):name() {
    //arb_internal.magic = RT_ARBN_INTERNAL_MAGIC;
    //arb_internal.magic = RT_ARB_INTERNAL_MAGIC;
}

Arbs::~Arbs(void)
{
    /*if (arb_internal.pt != nullptr)
	bu_free(arb_internal.pt, "Solid::~Solid: pt");*/
}

void Arbs::setName(const char* value)
{
    name = value;
}

void Arbs::addArb(
    const char* arbName,
    const point_t& point1,
    const point_t& point2,
    const point_t& point3,
    const point_t& point4,
    const point_t& point5,
    const point_t& point6,
    const point_t& point7,
    const point_t& point8
) {
    rt_arb_internal temp;
    temp.magic = RT_ARB_INTERNAL_MAGIC;

    VSET(temp.pt[0], point1[0], point1[1], point1[2]);
    VSET(temp.pt[1], point2[0], point2[1], point2[2]);
    VSET(temp.pt[2], point3[0], point3[1], point3[2]);
    VSET(temp.pt[3], point4[0], point4[1], point4[2]);
    VSET(temp.pt[4], point5[0], point5[1], point5[2]);
    VSET(temp.pt[5], point6[0], point6[1], point6[2]);
    VSET(temp.pt[6], point7[0], point7[1], point7[2]);
    VSET(temp.pt[7], point8[0], point8[1], point8[2]);

    arbs[arbName] = temp;
}

const char* Arbs::getName(void) const
{
    return name.c_str();
}

std::map<std::string, rt_arb_internal> Arbs::getArbs(void) const
{
    return arbs;
}


std::vector<std::string> Arbs::write
(
    rt_wdb* wdbp
){
    std::vector<std::string> ret;

    for (std::map<std::string, rt_arb_internal>::iterator it = arbs.begin(); it != arbs.end(); it++) {
	std::string arbName = name;
	arbName += it->first;
	arbName += ".arb";
	ret.push_back(arbName);

	rt_arb_internal* arb_wdb;

	BU_GET(arb_wdb, rt_arb_internal);
	arb_wdb->magic = RT_ARB_INTERNAL_MAGIC;
	*arb_wdb = it->second;

	wdb_export(wdbp, arbName.c_str(), arb_wdb, ID_ARB8, 1.);
    }

    return ret;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
