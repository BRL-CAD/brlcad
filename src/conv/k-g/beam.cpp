/*                 G E O M E T R Y . C P P
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
 /** @file beam.cpp
  *
  * LS Dyna keyword file to BRL-CAD converter:
  * intermediate beam implementation
  */

#include "beam.h"






Beam::Beam(void):name() {}

void Beam::setName(const char* value)
{
    name = value;
}

void Beam::addBeam(const char* beamName, const beamPoint& point1, const beamPoint& point2)
{
    struct wdb_pipe_pnt* temp1;
    struct wdb_pipe_pnt* temp2;
    struct bu_list              tempList;
    BU_LIST_INIT(&tempList);

    BU_ALLOC(temp1, struct wdb_pipe_pnt);
    BU_ALLOC(temp2, struct wdb_pipe_pnt);

    temp1->l.magic = WDB_PIPESEG_MAGIC;
    temp1->pp_id = point1.innerDiameter;
    temp1->pp_od = point1.outerDiameter;
    temp1->pp_bendradius = 25.0;
    VMOVE(temp1->pp_coord, point1.coords);

    temp2->l.magic = WDB_PIPESEG_MAGIC;
    temp2->pp_id = point2.innerDiameter;
    temp2->pp_od = point2.outerDiameter;
    temp2->pp_bendradius = 25.0;
    VMOVE(temp2->pp_coord, point2.coords);

    BU_LIST_INSERT(&tempList, &temp1->l);
    BU_LIST_INSERT(&tempList, &temp2->l);

    m_list[beamName] = tempList;
}

void Beam::addBeam(const char* beamName, const beamPoint& point1, const beamPoint& point2, const beamPoint& point3)
{
}

std::vector<std::string> Beam::write
(
    rt_wdb* wdbp
) {
    std::vector<std::string> ret;

    for (std::map<std::string, bu_list>::iterator it = m_list.begin(); it != m_list.end(); ++it) {
	std::string beamName = name;
	beamName += ".";
	beamName += it->first;
	beamName += ".beam";
	ret.push_back(beamName);

	rt_pipe_internal* pipe_wdb;
	BU_GET(pipe_wdb, rt_pipe_internal);
	pipe_wdb->pipe_magic = RT_PIPE_INTERNAL_MAGIC;
	BU_LIST_INIT(&pipe_wdb->pipe_segs_head);
	BU_LIST_APPEND_LIST(&pipe_wdb->pipe_segs_head, &(it->second));
	wdb_export(wdbp, beamName.c_str(), pipe_wdb, ID_PIPE, 1);
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