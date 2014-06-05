/*                         B O T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2012-2014 United States Government as represented by
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
/** @file Bot.cpp
 *
 * RAW geometry file to BRL-CAD converter:
 * bag of triangles intermediate data structure implementation
 *
 *  Origin -
 *	IABG mbH (Germany)
 */

#include "Bot.h"


Bot::Bot(void) : m_name(), m_thickness(0.), m_vertices(), m_faces() {}


void Bot::setName(const std::string& value)
{
    m_name = value;
}


void Bot::setThickness(double value)
{
    m_thickness = value;
}


size_t Bot::addPoint(point_t& point)
{
    size_t ret = m_vertices.size() / 3;

    // search for duplicate vertex
    for (size_t i = 0; i < (m_vertices.size() / 3); ++i) {
	if (NEAR_ZERO(m_vertices[i * 3] - point[X], SMALL_FASTF) &&
	    NEAR_ZERO(m_vertices[i * 3 + 1] - point[Y], SMALL_FASTF) &&
	    NEAR_ZERO(m_vertices[i * 3 + 2] - point[Z], SMALL_FASTF)) {
	    ret = i;
	    break;
	}
    }

    if (ret == (m_vertices.size() / 3)) {
	// add a new vertex
	m_vertices.push_back(point[X]);
	m_vertices.push_back(point[Y]);
	m_vertices.push_back(point[Z]);
    }

    return ret; // return index of vertex
}


void Bot::addTriangle(int a,
		      int b,
		      int c)
{
    // add a new triangle
    m_faces.push_back(a);
    m_faces.push_back(b);
    m_faces.push_back(c);
}


const std::string& Bot::name(void) const
{
    return m_name;
}


void Bot::write(rt_wdb* wdbp) const
{
    if (m_thickness > 0.) {
	if ((m_faces.size() / 3) > 0)
	    writePlate(wdbp);
	else
	    std::cout << "Ignore degenerated BOT " << m_name.c_str() << '\n';
    }
    else {
	if ((m_faces.size() / 3) > 3)
	    writeSolid(wdbp);
	else
	    std::cout << "Ignore degenerated BOT " << m_name.c_str() << '\n';
    }
}


void Bot::writeSolid(rt_wdb* wdbp) const
{
    fastf_t* vertices = static_cast<fastf_t*>(malloc(m_vertices.size() * sizeof(fastf_t)));
    int*     faces    = static_cast<int*>(malloc(m_faces.size() * sizeof(int)));

    for (size_t i = 0; i < m_vertices.size(); ++i)
	vertices[i] = m_vertices[i];

    for (size_t i = 0; i < m_faces.size(); ++i)
	faces[i] = m_faces[i];

    mk_bot(wdbp,
	   m_name.c_str(),
	   RT_BOT_SOLID,
	   RT_BOT_UNORIENTED,
	   0,
	   m_vertices.size() / 3,
	   m_faces.size() / 3,
	   vertices,
	   faces,
	   0,
	   0);

    free(vertices);
    free(faces);
}


void Bot::writePlate(rt_wdb* wdbp) const
{
    fastf_t* vertices    = static_cast<fastf_t*>(malloc(m_vertices.size() * sizeof(fastf_t)));
    int*     faces       = static_cast<int*>(malloc(m_faces.size() * sizeof(int)));
    fastf_t* thicknesses = static_cast<fastf_t*>(malloc(m_faces.size() * sizeof(fastf_t) / 3));
    bu_bitv* faceModes   = bu_bitv_new(m_faces.size() / 3);

    for (size_t i = 0; i < m_vertices.size(); ++i)
	vertices[i] = m_vertices[i];

    for (size_t i = 0; i < m_faces.size(); ++i)
	faces[i] = m_faces[i];

    for (size_t i = 0; i < (m_faces.size() / 3); ++i)
	thicknesses[i] = m_thickness;

    bu_bitv_clear(faceModes);

    mk_bot(wdbp,
	   m_name.c_str(),
	   RT_BOT_PLATE,
	   RT_BOT_UNORIENTED,
	   0,
	   m_vertices.size() / 3,
	   m_faces.size() / 3,
	   vertices,
	   faces,
	   thicknesses,
	   faceModes);

    free(vertices);
    free(faces);
    free(thicknesses);
    bu_bitv_free(faceModes);
}
