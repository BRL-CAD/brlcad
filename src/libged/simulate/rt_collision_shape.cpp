/*          R T _ C O L L I S I O N _ S H A P E . C P P
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
/** @file rt_collision_shape.cpp
 *
 * A librt-based collision shape.
 *
 */


#include "common.h"


#ifdef HAVE_BULLET


#include "rt_collision_shape.hpp"

#include "bu/log.h"


namespace simulate
{


RtCollisionShape::RtCollisionShape(const btVector3 &aabb_extents,
				   const btVector3 &aabb_center_height) :
    m_aabb_center_height(aabb_center_height),
    m_box_shape(aabb_extents / 2.0)
{
    m_shapeType = RT_COLLISION_SHAPE_TYPE;

    for (std::size_t i = 0; i < 3; ++i)
	if (aabb_extents[i] < 0.0)
	    bu_bomb("invalid argument");
}


const char *
RtCollisionShape::getName() const
{
    return "RtCollisionShape";
}


void
RtCollisionShape::getAabb(const btTransform &transform,
			  btVector3 &dest_aabb_min, btVector3 &dest_aabb_max) const
{
    const btTransform aabb_center_transform(transform.getBasis(),
					    transform(transform.inverse()(transform.getOrigin()) + m_aabb_center_height));

    return m_box_shape.getAabb(aabb_center_transform, dest_aabb_min, dest_aabb_max);
}


void
RtCollisionShape::calculateLocalInertia(const btScalar mass,
					btVector3 &dest_inertia) const
{
    if (mass < 0.0)
	bu_bomb("invalid argument");

    m_box_shape.calculateLocalInertia(mass, dest_inertia);
}


const btVector3 &
RtCollisionShape::getLocalScaling() const
{
    return m_box_shape.getLocalScaling();
}


btScalar
RtCollisionShape::getMargin() const
{
    return m_box_shape.getMargin();
}


void
RtCollisionShape::setLocalScaling(const btVector3 &local_scaling)
{
    for (std::size_t i = 0; i < 3; ++i)
	if (local_scaling[i] < 0.0)
	    bu_bomb("invalid argument");

    m_box_shape.setLocalScaling(local_scaling);
}


void
RtCollisionShape::setMargin(const btScalar collision_margin)
{
    if (collision_margin < 0.0)
	bu_bomb("invalid argument");

    m_box_shape.setMargin(collision_margin);
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
