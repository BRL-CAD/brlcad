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


namespace
{


HIDDEN std::string
make_name(const std::string &base)
{
    const char * const prefix = "RtDebugDraw::";

    std::ostringstream stream;
    stream.exceptions(std::ostream::failbit | std::ostream::badbit);

    stream << prefix << base << static_cast<std::size_t>(drand48() * 1.0e5 + 0.5) <<
	   ".s";
    return stream.str();
}


HIDDEN void
apply_color(db_i &db, const std::string &name, const btVector3 &color)
{
    RT_CK_DBI(&db);

    std::ostringstream stream;
    stream.exceptions(std::ostream::failbit | std::ostream::badbit);

    for (std::size_t i = 0; i < 3; ++i) {
	if (!(0.0 <= color[i] && color[i] <= 1.0))
	    bu_bomb("invalid color");

	stream << static_cast<unsigned>(color[i] * 255.0 + 0.5);

	if (i != 2)
	    stream.put(' ');
    }

    if (db5_update_attribute(name.c_str(), "color", stream.str().c_str(), &db))
	bu_bomb("db5_update_attribute() failed");
}


}


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
RtDebugDraw::reportErrorWarning(const char * const message)
{
    bu_log("WARNING: Bullet: %s\n", message);
    bu_bomb(message);
}


void
RtDebugDraw::drawLine(const btVector3 &from, const btVector3 &to,
		      const btVector3 &color)
{
    point_t from_pt = VINIT_ZERO;
    vect_t height = VINIT_ZERO;
    VMOVE(from_pt, from);
    VMOVE(height, to - from);

    const std::string name = make_name("line");

    if (mk_rcc(m_db.dbi_wdbp, name.c_str(), from_pt, height, 1.0e-8))
	bu_bomb("mk_rcc() failed");

    apply_color(m_db, name, color);
}


void
RtDebugDraw::draw3dText(const btVector3 &UNUSED(location),
			const char * const UNUSED(text))
{
    bu_bomb("not implemented");
}


void RtDebugDraw::drawAabb(const btVector3 &from, const btVector3 &to,
			   const btVector3 &color)
{
    point_t min_pt = VINIT_ZERO, max_pt = VINIT_ZERO;
    VMOVE(min_pt, from);
    VMIN(min_pt, to);
    VMOVE(max_pt, from);
    VMAX(max_pt, to);

    const std::string name = make_name("aabb");

    if (mk_rpp(m_db.dbi_wdbp, name.c_str(), min_pt, max_pt))
	bu_bomb("mk_rpp() failed");

    apply_color(m_db, name, color);
}


void
RtDebugDraw::drawContactPoint(const btVector3 &point_on_b,
			      const btVector3 &normal_world_on_b, const btScalar distance,
			      const int UNUSED(lifetime), const btVector3 &color)
{
    point_t point_on_b_pt = VINIT_ZERO;
    VMOVE(point_on_b_pt, point_on_b);

    const std::string name = make_name("contact");

    if (mk_sph(m_db.dbi_wdbp, name.c_str(), point_on_b_pt, 1.0))
	bu_bomb("mk_sph() failed");

    apply_color(m_db, name, color);
    drawLine(point_on_b, point_on_b + normal_world_on_b * distance * 1.0e3, color);
}


void
RtDebugDraw::setDebugMode(const int mode)
{
    m_debug_mode = mode;
}


int
RtDebugDraw::getDebugMode() const
{
    return m_debug_mode;
}


void
RtDebugDraw::setDefaultColors(const DefaultColors &default_colors)
{
    m_default_colors = default_colors;
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
