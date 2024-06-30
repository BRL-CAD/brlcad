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
    //arb_internal.magic = RT_ARBN_INTERNAL_MAGIC;
    arb_internal.magic = RT_ARB_INTERNAL_MAGIC;
}

Solid::~Solid(void)
{
    /*if (arb_internal.pt != nullptr)
	bu_free(arb_internal.pt, "Solid::~Solid: pt");*/
}

void Solid::setName(const char* value)
{
    name = value;
}

void Solid::addNodes(
    const point_t& point1,
    const point_t& point2,
    const point_t& point3,
    const point_t& point4,
    const point_t& point5,
    const point_t& point6,
    const point_t& point7,
    const point_t& point8
) {
    VSET(arb_internal.pt[0], point1[0], point1[1], point1[2]);
    VSET(arb_internal.pt[1], point2[0], point2[1], point2[2]);
    VSET(arb_internal.pt[2], point3[0], point3[1], point3[2]);
    VSET(arb_internal.pt[3], point4[0], point4[1], point4[2]);
    VSET(arb_internal.pt[4], point5[0], point5[1], point5[2]);
    VSET(arb_internal.pt[5], point6[0], point6[1], point6[2]);
    VSET(arb_internal.pt[6], point7[0], point7[1], point7[2]);
    VSET(arb_internal.pt[7], point8[0], point8[1], point8[2]);
 //   for (int i = 0; i < nodes.size(); ++i) {
	////VSET(arb_internal.pt[i], nodes[i][1],nodes[i][2],nodes[i][3]);
 //   }
}

const char* Solid::getName(void) const
{
    return name.c_str();
}

void Solid::write(rt_wdb* wdbp)
{
    rt_arb_internal* arb_wdb;

    BU_GET(arb_wdb, rt_arb_internal);
    arb_wdb->magic = RT_ARB_INTERNAL_MAGIC;

    *arb_wdb = arb_internal;

    wdb_export(wdbp, name.c_str(), arb_wdb, ID_ARB8, 1.);

}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
