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

    //std::string botName(name);
    //std::string arbsName(name);
    //botName += ".bot";
    //arbsName += ".s";

    //m_bot.setName(botName.c_str());
    //m_arbs.setName(arbsName.c_str());
}

void Geometry::setThickness
(
    double value
) {
    m_bot.setThickness(value);
}

void Geometry::setType(GeometryType type)
{
    m_type = type;
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

const char* Geometry::getName(void) const
{
    return name.c_str();
}

std::vector<std::string> Geometry::getNames(void) const
{
    std::vector<std::string> ret;
    if (m_type == GeometryType::Bot) {
	std::string botName = name;
	botName += ".bot";
	ret.push_back(botName);
    }
    else if (m_type == GeometryType::Arbs) {
	std::map<std::string, rt_arb_internal> temp = m_arbs.getArbs();
	for (std::map<std::string, rt_arb_internal>::iterator it = temp.begin(); it != temp.end(); it++) {
	    std::string arbName = name;
	    arbName.append(it->first);
	    arbName += ".arb";
	    ret.push_back(arbName);
	}
    }
    
    return ret;
}

Bot Geometry::getBot(void) const
{
    return m_bot;
}

Arbs Geometry::getArbs(void) const
{
    return m_arbs;
}

GeometryType Geometry::getType(void) const
{
    return m_type;
}
