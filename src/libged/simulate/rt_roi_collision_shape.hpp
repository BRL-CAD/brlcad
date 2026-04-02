/*          R T _ R O I _ C O L L I S I O N _ S H A P E . H P P
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
/** @file rt_roi_collision_shape.hpp
 *
 * A librt-based collision shape with a Region-of-Interest (ROI) proxy.
 *
 * This collision shape represents static objects using a moving local AABB
 * (the ROI) inside the object's global AABB. The ROI shrinks the broadphase
 * proxy to a region around dynamic bodies, improving Bullet's robustness
 * when small dynamic objects interact with very large static objects.
 *
 */


#ifndef SIMULATE_RT_ROI_COLLISION_SHAPE_H
#define SIMULATE_RT_ROI_COLLISION_SHAPE_H


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


class RtRoiCollisionShape : public btCollisionShape
{
public:
    static const int RT_ROI_COLLISION_SHAPE_TYPE = CUSTOM_POLYHEDRAL_SHAPE_TYPE + 1;

    // Construct ROI collision shape for a static object
    // global_aabb_min/max: world-space AABB bounds (Bullet units)
    // name: object name for debugging
    explicit RtRoiCollisionShape(const btVector3 &global_aabb_min,
				 const btVector3 &global_aabb_max,
				 const std::string &name);

    virtual ~RtRoiCollisionShape();

    virtual const char *getName() const;
    virtual void getAabb(const btTransform &transform, btVector3 &dest_aabb_min,
			 btVector3 &dest_aabb_max) const;
    virtual void calculateLocalInertia(btScalar mass,
				       btVector3 &dest_inertia) const;
    virtual const btVector3 &getLocalScaling() const;
    virtual btScalar getMargin() const;

    virtual void setLocalScaling(const btVector3 &local_scaling);
    virtual void setMargin(btScalar collision_margin);

    // Update the ROI AABB, clamped to global bounds
    // The ROI center becomes the new transform origin for the rigid body
    // Minimum extents safeguards are applied to prevent degenerate boxes
    void setRoiAabb(const btVector3 &roi_min, const btVector3 &roi_max);

    // Get the current ROI AABB bounds
    btVector3 getRoiMin() const { return m_roi_min; }
    btVector3 getRoiMax() const { return m_roi_max; }

    // Get the global AABB bounds
    btVector3 getGlobalMin() const { return m_global_aabb_min; }
    btVector3 getGlobalMax() const { return m_global_aabb_max; }


private:
    const btVector3 m_global_aabb_min;
    const btVector3 m_global_aabb_max;
    const std::string m_name;
    btVector3 m_roi_min;
    btVector3 m_roi_max;
    btBoxShape *m_box_shape;

    void updateBoxShape();
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
