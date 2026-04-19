/*               R T _ D E B U G _ D R A W . H P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2026 United States Government as represented by
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
/** @file rt_debug_draw.hpp
 *
 * Bullet debug renderer.
 *
 */


#ifndef SIMULATE_RT_DEBUG_DRAW_H
#define SIMULATE_RT_DEBUG_DRAW_H


#include "common.h"

#include "rt/db_instance.h"

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic push
#endif
#if defined(__clang__)
#  pragma clang diagnostic push
#endif
#if defined(__GNUC__) && !defined(__clang__)
#  if (__GNUC__ < 13)
     // There seems to be a problem with multiple pragma diagnostic
     // calls in GCC 12... try https://stackoverflow.com/a/56887760
#    if GCC_PREREQ(8,0)
#      pragma GCC system_header
#    endif
#  else
#    pragma GCC diagnostic ignored "-Wfloat-equal"
#    pragma GCC diagnostic ignored "-Wundef"
#  endif
#endif
#if defined(__clang__)
#    pragma clang diagnostic ignored "-Wfloat-equal"
#    pragma clang diagnostic ignored "-Wundef"
#endif

#include <btBulletDynamicsCommon.h>

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
#if defined(__clang__)
#  pragma clang diagnostic pop
#endif


namespace simulate
{


class RtDebugDraw : public btIDebugDraw
{
public:
    explicit RtDebugDraw(db_i &db);

    virtual void reportErrorWarning(const char *message);

    virtual void drawLine(const btVector3 &from, const btVector3 &to,
			  const btVector3 &color);
    virtual void draw3dText(const btVector3 &location, const char *text);

    virtual void drawAabb(const btVector3 &from, const btVector3 &to,
			  const btVector3 &color);
    virtual void drawContactPoint(const btVector3 &point_on_b,
				  const btVector3 &normal_world_on_b, btScalar distance, int lifetime,
				  const btVector3 &color);

    virtual void setDebugMode(int mode);
    virtual int getDebugMode() const;

    virtual void setDefaultColors(const DefaultColors &default_colors);
    virtual DefaultColors getDefaultColors() const;


private:
    db_i &m_db;
    DefaultColors m_default_colors;
    int m_debug_mode;
};


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
