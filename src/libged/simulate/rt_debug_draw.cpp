/*               R T _ D E B U G _ D R A W . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2017 United States Government as represented by
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
/** @file rt_debug_draw.cpp
 *
 * Bullet debug renderer.
 *
 */


#include "common.h"


#ifdef HAVE_BULLET


#include "rt_debug_draw.hpp"

#include "wdb.h"

#include <sstream>


namespace simulate
{


RtDebugDraw::RtDebugDraw(db_i &db) :
    m_db(db),
    m_default_colors(),
    m_debug_mode(DBG_NoDebug)
{
    RT_CK_DBI(&db);
    RT_CK_WDB(m_db.dbi_wdbp);

    m_default_colors.m_aabb = btVector3(0.0, 0.75, 0.0);
}


void
RtDebugDraw::setDefaultColors(const DefaultColors &default_colors)
{
    m_default_colors = default_colors;
}


void
RtDebugDraw::drawLine(const btVector3 &from, const btVector3 &to,
		      const btVector3 &color)
{
    point_t from_pt = VINIT_ZERO;
    vect_t height = VINIT_ZERO;
    VMOVE(from_pt, from);
    VMOVE(height, to - from);

    std::ostringstream name_str, color_str;
    name_str << "RtDebugDraw::line_" << static_cast<std::size_t>
	     (drand48() * 1.0e5 + 0.5) << ".s";

    for (std::size_t i = 0; i < 3; ++i) {
	color_str << static_cast<unsigned>(color[i] * 255.0 + 0.5);

	if (i != 2)
	    color_str.put(' ');
    }

    if (mk_rcc(m_db.dbi_wdbp, name_str.str().c_str(), from_pt, height, 1.0e-8))
	bu_bomb("mk_rcc() failed");

    if (db5_update_attribute(name_str.str().c_str(), "color",
			     color_str.str().c_str(), &m_db))
	bu_bomb("db5_update_attributes() failed");
}


void
RtDebugDraw::drawContactPoint(const btVector3 &point_on_b,
			      const btVector3 &normal_world_on_b, btScalar distance, int UNUSED(lifetime),
			      const btVector3 &color)
{
    point_t point_on_b_pt = VINIT_ZERO;
    VMOVE(point_on_b_pt, point_on_b);

    std::ostringstream name_str, color_str;
    name_str << "RtDebugDraw::contact_" << static_cast<std::size_t>
	     (drand48() * 1.0e5 + 0.5) << ".s";

    for (std::size_t i = 0; i < 3; ++i) {
	color_str << static_cast<unsigned>(color[i] * 255.0 + 0.5);

	if (i != 2)
	    color_str.put(' ');
    }

    if (mk_sph(m_db.dbi_wdbp, name_str.str().c_str(), point_on_b_pt, 3.0))
	bu_bomb("mk_sph() failed");

    if (db5_update_attribute(name_str.str().c_str(), "color",
			     color_str.str().c_str(), &m_db))
	bu_bomb("db5_update_attributes() failed");

    drawLine(point_on_b, normal_world_on_b * distance, color);
}


void
RtDebugDraw::reportErrorWarning(const char * const message)
{
    bu_log("WARNING: Bullet: %s\n", message);
    bu_bomb(message);
}


void
RtDebugDraw::draw3dText(const btVector3 &UNUSED(location),
			const char * const UNUSED(text))
{
    bu_bomb("not implemented");
}


void
RtDebugDraw::setDebugMode(const int mode)
{
    m_debug_mode = static_cast<DebugDrawModes>(mode);
}


int
RtDebugDraw::getDebugMode() const
{
    return m_debug_mode;
}


btIDebugDraw::DefaultColors
RtDebugDraw::getDefaultColors() const
{
    return m_default_colors;
}


}


#endif


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
