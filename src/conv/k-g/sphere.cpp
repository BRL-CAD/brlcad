/*                 S P H E R E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2024-2025 United States Government as represented by
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
 /** @file sphere.cpp
  *
  * LS Dyna keyword file to BRL-CAD converter:
  * intermediate sphere implementation
  */

#include "sphere.h"


Sphere::Sphere(void) : m_name() {}

void Sphere::setName
(
    const char* value
) {
    if (value != nullptr)
	m_name = value;
    else
	m_name = "";
}



void Sphere::addSphere
(
    const char* sphereName,
    const point_t& center,
    const float& radius
) {
    rt_part_internal temp;

    temp.part_magic = RT_PART_INTERNAL_MAGIC;
    temp.part_type = ID_SPH;

    VMOVE(temp.part_V, center);
    temp.part_hrad = radius;
    temp.part_vrad = radius;
    m_sphere[sphereName] = temp;
}

std::vector<std::string> Sphere::write
(
    rt_wdb* wdbp
) {
    std::vector<std::string> ret;

    for (std::map<std::string, rt_part_internal>::iterator it = m_sphere.begin(); it != m_sphere.end(); it++) {
	std::string sphereName = m_name;
	sphereName += ".";
	sphereName += it->first;
	sphereName += ".sph";

	ret.push_back(sphereName);

	vect_t HH = { 0,0,0 };
	rt_part_internal* part_wdb;

	BU_GET(part_wdb, rt_part_internal);
	part_wdb->part_magic = RT_PART_INTERNAL_MAGIC;
	part_wdb->part_type = ID_SPH;

	*part_wdb = it->second;

	VMOVE(part_wdb->part_H, HH);

	if (part_wdb->part_vrad > 0) {
	    wdb_export(wdbp, sphereName.c_str(), (void*) part_wdb, ID_PARTICLE, 1);
	}
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
