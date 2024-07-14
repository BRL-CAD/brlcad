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
 /** @file geometry.cpp
  *
  * LS Dyna keyword file to BRL-CAD converter:
  * intermediate geometry structure implementation
  */

#include "geometry.h"


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

Geometry::Geometry(void)
{
}

Geometry::~Geometry(void)
{
}

void Geometry::setBaseName
(
    const char* value
) {
    if (value != nullptr)
	name = value;
    else
	name = "";

    m_bot.setName(name.c_str());
    m_arbs.setName(name.c_str());
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
){
    m_bot.addTriangle(point1, point2, point3);
}

void Geometry::addArb
(
    const char* arbName,
    const point_t& point1,
    const point_t& point2,
    const point_t& point3,
    const point_t& point4,
    const point_t& point5,
    const point_t& point6,
    const point_t& point7,
    const point_t& point8
){
    m_arbs.addArb(arbName, point1, point2, point3, point4, point5, point6, point7, point8);
}

const char* Geometry::getBaseName(void) const
{
    return name.c_str();
}


Bot Geometry::getBot(void) const
{
    return m_bot;
}

Arbs Geometry::getArbs(void) const
{
    return m_arbs;
}


std::vector<std::string> Geometry::write(rt_wdb* wdbp)
{
    std::vector<std::string> ret      = m_bot.write(wdbp);
    std::vector<std::string> arbNames = m_arbs.write(wdbp);

    ret.insert(ret.end(), arbNames.begin(), arbNames.end());

    return ret;
}
