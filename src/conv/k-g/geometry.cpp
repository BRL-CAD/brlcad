/*                 G E O M E T R Y . C P P
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
 /** @file geometry.cpp
  *
  * LS Dyna keyword file to BRL-CAD converter:
  * intermediate geometry structure implementation
  */

#include "geometry.h"


void Geometry::setBaseName
(
    const char* value
) {
    if (value != nullptr)
	m_name = value;
    else
	m_name = "";

    m_bot.setName(m_name.c_str());
    m_arbs.setName(m_name.c_str());
    m_pipe.setName(m_name.c_str());
}


void Geometry::setThickness
(
    double value
) {
    m_bot.setThickness(value);
}


void Geometry::addTriangle
(
    const point_t& point1,
    const point_t& point2,
    const point_t& point3
) {
    m_bot.addTriangle(point1, point2, point3);
}


void Geometry::addArb
(
    const char*    arbName,
    const point_t& point1,
    const point_t& point2,
    const point_t& point3,
    const point_t& point4,
    const point_t& point5,
    const point_t& point6,
    const point_t& point7,
    const point_t& point8
) {
    m_arbs.addArb(arbName, point1, point2, point3, point4, point5, point6, point7, point8);
}


void Geometry::addPipePnt
(
    pipePoint point
) {
    m_pipe.addPoint(point);
}


void Geometry::addBeamResultant
(
    std::string         beamName,
    std::string         sectionType,
    const point_t&      node1,
    const point_t&      node2,
    const point_t&      node3,
    std::vector<double> D
) {
    BeamResultant temp;
    rt_sketch_internal* tempSkt;

    BU_GET(tempSkt, rt_sketch_internal);
    tempSkt->magic = RT_SKETCH_INTERNAL_MAGIC;

    std::string sectionName = beamName;
    sectionName += ".skt";
    std::string extrudeName = beamName;
    extrudeName += ".ext";
    temp.skt.setName(sectionName.c_str());
    temp.ext.setName(extrudeName.c_str());

    vect_t h;
    h[X] = node2[X] - node1[X];
    h[Y] = node2[Y] - node1[Y];
    h[Z] = node2[Z] - node1[Z];

    tempSkt = temp.skt.creatSketch(sectionType, node1, node2, node3, D);

    temp.ext.extrudeSection(sectionName.c_str(), node1, h, tempSkt->u_vec, tempSkt->v_vec);

    m_BeamsResultant.push_back(temp);
}


const char* Geometry::getBaseName(void) const
{
    return m_name.c_str();
}


Bot& Geometry::getBot(void)
{
    return m_bot;
}


Arbs& Geometry::getArbs(void)
{
    return m_arbs;
}


Pipe& Geometry::getPipe(void)
{
    return m_pipe;
}


std::vector<std::string> Geometry::write
(
    rt_wdb* wdbp
) {
    std::vector<std::string> ret      = m_bot.write(wdbp);
    std::vector<std::string> arbNames = m_arbs.write(wdbp);
    std::vector<std::string> pipeName = m_pipe.write(wdbp);

    for (size_t i = 0; i < m_BeamsResultant.size(); ++i) {
	ret.push_back(m_BeamsResultant[i].skt.write(wdbp));
	ret.push_back(m_BeamsResultant[i].ext.write(wdbp));
    }

    ret.insert(ret.end(), arbNames.begin(), arbNames.end());
    ret.insert(ret.end(), pipeName.begin(), pipeName.end());

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
