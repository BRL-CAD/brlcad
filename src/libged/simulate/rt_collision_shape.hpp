/*          R T _ C O L L I S I O N _ S H A P E . H P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2025 United States Government as represented by
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
/** @file rt_collision_shape.hpp
 *
 * A librt-based collision shape.
 *
 */


#ifndef SIMULATE_RT_COLLISION_SHAPE_H
#define SIMULATE_RT_COLLISION_SHAPE_H


#include "common.h"

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


#include <string>


namespace simulate
{


class RtCollisionShape : public btCollisionShape
{
public:
    static const int RT_COLLISION_SHAPE_TYPE = CUSTOM_POLYHEDRAL_SHAPE_TYPE;

    // `aabb_center_height` is the vector from the center of mass to
    // the center of the AABB
    explicit RtCollisionShape(const btVector3 &aabb_extents,
			      const btVector3 &aabb_center_height,
			      const std::string &name);

    virtual const char *getName() const;
    virtual void getAabb(const btTransform &transform, btVector3 &dest_aabb_min,
			 btVector3 &dest_aabb_max) const;
    virtual void calculateLocalInertia(btScalar mass,
				       btVector3 &dest_inertia) const;
    virtual const btVector3 &getLocalScaling() const;
    virtual btScalar getMargin() const;

    virtual void setLocalScaling(const btVector3 &local_scaling);
    virtual void setMargin(btScalar collision_margin);


private:
    const btVector3 m_aabb_center_height;
    const std::string m_name;
    btBoxShape m_box_shape;
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
