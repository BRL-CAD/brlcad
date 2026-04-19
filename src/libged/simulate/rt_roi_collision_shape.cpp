/*          R T _ R O I _ C O L L I S I O N _ S H A P E . C P P
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
/** @file rt_roi_collision_shape.cpp
 *
 * A librt-based collision shape with a Region-of-Interest (ROI) proxy.
 *
 */


#include "common.h"


#ifdef HAVE_BULLET


#include "rt_roi_collision_shape.hpp"
#include "utility.hpp"

#include "bu/log.h"


namespace
{


// Minimum ROI extents to prevent degenerate collision shapes
// This is in Bullet units (meters)
static const btScalar MIN_ROI_EXTENT = 0.1;

// Maximum extents for Bullet collision detection
// Based on Bullet's numeric precision limits
static const btScalar MAX_EXTENT = 1000.0;


static bool
bullet_dimensions_valid(const btVector3 &extents)
{
    const std::pair<btScalar, btScalar> bullet_limits(0.05, 10.0);
    const unsigned tolerance_factor = 100;

    const std::pair<btScalar, btScalar> bounds(
	bullet_limits.first / tolerance_factor,
	bullet_limits.second * tolerance_factor);

    if (extents.getX() < bounds.first || extents.getY() < bounds.first
       || extents.getZ() < bounds.first)
	return false;

    if (extents.getX() > bounds.second || extents.getY() > bounds.second
       || extents.getZ() > bounds.second)
	return false;

    return true;
}


}


namespace simulate
{


RtRoiCollisionShape::RtRoiCollisionShape(const btVector3 &global_aabb_min,
					 const btVector3 &global_aabb_max,
					 const std::string &name) :
    m_global_aabb_min(global_aabb_min),
    m_global_aabb_max(global_aabb_max),
    m_name(name),
    m_roi_min(global_aabb_min),
    m_roi_max(global_aabb_max),
    m_box_shape(NULL)
{
    m_shapeType = RT_ROI_COLLISION_SHAPE_TYPE;

    // For ROI shapes, the global AABB may be very large (e.g., terrain).
    // Start with a reasonable initial ROI at the center of the global AABB.
    // The ROI will be updated to track dynamic bodies during simulation.
    btVector3 global_center = (global_aabb_min + global_aabb_max) / 2.0;
    btScalar initial_roi_extent = 50.0;  // 50 meters initial ROI
    m_roi_min = global_center - btVector3(initial_roi_extent, initial_roi_extent, initial_roi_extent);
    m_roi_max = global_center + btVector3(initial_roi_extent, initial_roi_extent, initial_roi_extent);
    
    // Clamp initial ROI to global bounds
    m_roi_min.setMax(m_global_aabb_min);
    m_roi_max.setMin(m_global_aabb_max);

    // Initialize with the clamped ROI
    // The rigid body's transform origin should be set to the ROI center
    updateBoxShape();
}


RtRoiCollisionShape::~RtRoiCollisionShape()
{
    delete m_box_shape;
}


const char *
RtRoiCollisionShape::getName() const
{
    return m_name.c_str();
}


void
RtRoiCollisionShape::getAabb(const btTransform &transform,
			     btVector3 &dest_aabb_min, btVector3 &dest_aabb_max) const
{
    // The transform origin is at the ROI center
    // The box shape represents the ROI extents
    m_box_shape->getAabb(transform, dest_aabb_min, dest_aabb_max);
}


void
RtRoiCollisionShape::calculateLocalInertia(const btScalar mass,
					   btVector3 &dest_inertia) const
{
    if (mass < 0.0)
	bu_bomb("mass must be non-negative");

    // Static bodies (mass=0) don't need inertia calculation,
    // but we delegate to the box shape for consistency
    m_box_shape->calculateLocalInertia(mass, dest_inertia);
}


const btVector3 &
RtRoiCollisionShape::getLocalScaling() const
{
    return m_box_shape->getLocalScaling();
}


btScalar
RtRoiCollisionShape::getMargin() const
{
    return m_box_shape->getMargin();
}


void
RtRoiCollisionShape::setLocalScaling(const btVector3 &local_scaling)
{
    for (std::size_t i = 0; i < 3; ++i)
	if (local_scaling[i] < 0.0)
	    bu_bomb("local_scaling components must be non-negative");

    m_box_shape->setLocalScaling(local_scaling);
}


void
RtRoiCollisionShape::setMargin(const btScalar collision_margin)
{
    if (collision_margin < 0.0)
	bu_bomb("collision_margin must be non-negative");

    m_box_shape->setMargin(collision_margin);
}


void
RtRoiCollisionShape::setRoiAabb(const btVector3 &roi_min, const btVector3 &roi_max)
{
    // Clamp ROI to global AABB bounds
    btVector3 clamped_min, clamped_max;
    for (int i = 0; i < 3; ++i) {
	clamped_min[i] = btMax(roi_min[i], m_global_aabb_min[i]);
	clamped_max[i] = btMin(roi_max[i], m_global_aabb_max[i]);

	// Enforce minimum extents to prevent degenerate shapes
	// If the clamped region is too small, expand it symmetrically
	btScalar extent = clamped_max[i] - clamped_min[i];
	if (extent < MIN_ROI_EXTENT) {
	    btScalar center = (clamped_min[i] + clamped_max[i]) / 2.0;
	    btScalar half_extent = MIN_ROI_EXTENT / 2.0;
	    clamped_min[i] = btMax(center - half_extent, m_global_aabb_min[i]);
	    clamped_max[i] = btMin(center + half_extent, m_global_aabb_max[i]);

	    // If we still can't satisfy minimum extents due to global bounds,
	    // clamp to global bounds (this handles edge cases with small global AABBs)
	    extent = clamped_max[i] - clamped_min[i];
	    if (extent < MIN_ROI_EXTENT) {
		clamped_min[i] = m_global_aabb_min[i];
		clamped_max[i] = m_global_aabb_max[i];
	    }
	}
    }

    m_roi_min = clamped_min;
    m_roi_max = clamped_max;

    updateBoxShape();
}


void
RtRoiCollisionShape::updateBoxShape()
{
    // Calculate ROI extents (full extents, not half)
    btVector3 roi_extents = m_roi_max - m_roi_min;

    // Update the box shape with half-extents
    // Note: btBoxShape stores half-extents internally
    btVector3 half_extents = roi_extents / 2.0;

    // Validate the ROI dimensions before updating
    if (!bullet_dimensions_valid(roi_extents))
	throw InvalidSimulationError("ROI dimensions are too extreme for Bullet at '"
				    + m_name + "'");

    // Allocate new shape before releasing old one for exception safety
    btBoxShape * const new_shape = new btBoxShape(half_extents);
    delete m_box_shape;
    m_box_shape = new_shape;
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
