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
    rt_ell_internal temp;

    temp.magic = RT_ELL_INTERNAL_MAGIC;

    VMOVE(temp.v, center);
    VSET(temp.a, radius, 0.0, 0.0);
    VSET(temp.b, 0.0, radius, 0.0);
    VSET(temp.c, 0.0, 0.0, radius);

    m_sphere[sphereName] = temp;
}

std::vector<std::string> Sphere::write
(
    rt_wdb* wdbp
) {
    std::vector<std::string> ret;

    for (std::map<std::string, rt_ell_internal>::iterator it = m_sphere.begin(); it != m_sphere.end(); it++) {
	std::string sphereName = m_name;
	sphereName += ".";
	sphereName += it->first;
	sphereName += ".sph";

	ret.push_back(sphereName);

	rt_ell_internal* sph_wdb;

	BU_GET(sph_wdb, rt_ell_internal);
	
	sph_wdb->magic = RT_ELL_INTERNAL_MAGIC;
	VMOVE(sph_wdb->v, it->second.v);
	VMOVE(sph_wdb->a, it->second.a);
	VMOVE(sph_wdb->b, it->second.b);
	VMOVE(sph_wdb->c, it->second.c);
	
	if (sph_wdb->a[0] > 0) {
	    wdb_export(wdbp, sphereName.c_str(), (void*) sph_wdb, ID_SPH, 1);
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
