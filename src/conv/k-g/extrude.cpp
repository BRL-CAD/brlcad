/*                 E X T R U D E . C P P
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
 /** @file extrude.cpp
  *
  * LS Dyna keyword file to BRL-CAD converter:
  * intermediate extrude implementation
  */

#include "extrude.h"




Extrude::Extrude(void)
{
    BU_GET(m_extrude, rt_extrude_internal);
    m_extrude->magic = RT_EXTRUDE_INTERNAL_MAGIC;
}


void Extrude::setName
(
    const char* value
) {
    if (value != nullptr)
	name = value;
    else
	name = "";
}


void Extrude::extrudeSection(std::string sectionName,const point_t& V, vect_t h, vect_t u_vec, vect_t v_vec, rt_sketch_internal* skt)
{
    m_extrude->magic = RT_EXTRUDE_INTERNAL_MAGIC;
    VMOVE(m_extrude->V, V);
    VMOVE(m_extrude->h, h);
    VMOVE(m_extrude->u_vec, u_vec);
    VMOVE(m_extrude->v_vec, v_vec);
    m_extrude->sketch_name = bu_strdup(sectionName.c_str());
    //m_extrude->skt = skt;
    m_extrude->skt = (struct rt_sketch_internal*)NULL;
}


std::string Extrude::write(rt_wdb* wdbp)
{
    std::string ret;

    rt_extrude_internal* extrude_wdb;
    BU_GET(extrude_wdb, rt_extrude_internal);
    extrude_wdb->magic = RT_EXTRUDE_INTERNAL_MAGIC;
    extrude_wdb = m_extrude;

    if (extrude_wdb->sketch_name != "") {
	wdb_export(wdbp, name.c_str(), extrude_wdb, ID_EXTRUDE, 1);
	ret = name;
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
