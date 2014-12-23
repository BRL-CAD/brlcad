/*                   C O L L I S I O N . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014 United States Government as represented by
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
/** @file collision.cpp
 *
 * Brief description
 *
 */


#ifdef HAVE_BULLET


#include "common.h"

#include "collision.hpp"
#include "rt_instance.hpp"

#include "raytrace.h"


namespace
{


static void
on_multioverlap(application *app, partition *partition1, bu_ptbl *ptbl,
		partition *partition2)
{
    (void)app;
    (void)partition1;
    (void)ptbl;
    (void)partition2;
}


static void
calculate_contact_points(btManifoldResult &result, const btRigidBody &rb_a,
			 const btRigidBody &rb_b)
{
    const btScalar grid_size = 0.1;

    // calculate the normal of the contact points as the resultant of the velocities -A and B
    btVector3 normal_world_on_b = (rb_b.getLinearVelocity() -
				   rb_a.getLinearVelocity());

    if (normal_world_on_b != btVector3(0, 0, 0))
	normal_world_on_b.normalize();

    // shoot a circular grid of rays about `normal_world_on_b`
    xrays *rays;
    {
	BU_ALLOC(rays, xrays);
	BU_LIST_INIT(&rays->l);

	// calculate the overlap volume between the AABBs
	btVector3 overlap_min, overlap_max;
	{
	    btVector3 rb_a_aabb_min, rb_a_aabb_max, rb_b_aabb_min, rb_b_aabb_max;
	    rb_a.getAabb(rb_a_aabb_min, rb_a_aabb_max);
	    rb_b.getAabb(rb_b_aabb_min, rb_b_aabb_max);

	    VMOVE(overlap_max, rb_a_aabb_max);
	    VMIN(overlap_max, rb_b_aabb_max);
	    VMOVE(overlap_min, rb_a_aabb_min);
	    VMAX(overlap_min, rb_b_aabb_min);
	}

	// radius of the circle of rays
	btScalar radius = (overlap_max - overlap_min).length() / 2;

	// calculate the origin of the center ray
	btVector3 center_point;
	{
	    // center of the overlap volume
	    btVector3 overlap_center = overlap_min + 0.5 * overlap_max;

	    // step back from overlap_center, along the normal by `radius`,
	    // to ensure that rays start from outside of the overlap region
	    center_point = overlap_center - radius * normal_world_on_b;
	}

	xray &center_ray = rays->ray;
	center_ray.magic = RT_RAY_MAGIC;
	center_ray.index = 0;
	VMOVE(center_ray.r_pt, center_point);
	VMOVE(center_ray.r_dir, normal_world_on_b);

	// calculate the 'up' vector
	vect_t up_vect;
	{
	    btVector3 up = normal_world_on_b.cross(btVector3(1, 0, 0));

	    // use the y-axis if parallel to x-axis
	    if (up == btVector3(0, 0, 0))
		up = normal_world_on_b.cross(btVector3(0, 1, 0));

	    VMOVE(up_vect, up);
	}

	rt_gen_circular_grid(rays, &center_ray, radius, up_vect, grid_size);
    }

    // shoot the rays
    application app;
    {
	RT_APPLICATION_INIT(&app);
	app.a_rt_i = rt_instance_data::rt_instance;
	app.a_multioverlap = on_multioverlap;
    }

    xrays *entry;

    while (BU_LIST_WHILE(entry, xrays, &rays->l)) {
	VMOVE(app.a_ray.r_pt, entry->ray.r_pt);
	VMOVE(app.a_ray.r_dir, entry->ray.r_dir);
	rt_shootray(&app);

	BU_LIST_DEQUEUE(&entry->l);
	bu_free(entry, "xrays entry");
    }

    // TODO: collect contact points

    // for each contact point:
    const btVector3 pt_b(0, 0, 0);
    const btScalar depth = 0;
    result.addContactPoint(normal_world_on_b, pt_b, depth);
}


}


namespace simulate
{


RtCollisionShape::RtCollisionShape(const btVector3 &half_extents) :
    btBoxShape(half_extents)
{
    m_shapeType = RT_SHAPE_TYPE;
}


RtCollisionShape::~RtCollisionShape()
{}


btCollisionAlgorithm *
RtCollisionAlgorithm::CreateFunc::CreateCollisionAlgorithm(
    btCollisionAlgorithmConstructionInfo &cinfo,
    const btCollisionObjectWrapper *body_a_wrap,
    const btCollisionObjectWrapper *body_b_wrap)
{
    void *ptr = cinfo.m_dispatcher1->allocateCollisionAlgorithm(sizeof(
		    RtCollisionAlgorithm));
    return new(ptr) RtCollisionAlgorithm(NULL, cinfo, body_a_wrap, body_b_wrap);
}


RtCollisionAlgorithm::RtCollisionAlgorithm(btPersistentManifold *manifold,
	const btCollisionAlgorithmConstructionInfo &cinfo,
	const btCollisionObjectWrapper *body_a_wrap,
	const btCollisionObjectWrapper *body_b_wrap) :
    btActivatingCollisionAlgorithm(cinfo, body_a_wrap, body_b_wrap),
    m_owns_manifold(false),
    m_manifold(manifold)
{
    const btCollisionObject &body_a = *body_a_wrap->getCollisionObject();
    const btCollisionObject &body_b = *body_b_wrap->getCollisionObject();

    if (!m_manifold && m_dispatcher->needsCollision(&body_a, &body_b)) {
	m_manifold = m_dispatcher->getNewManifold(&body_a, &body_b);
	m_owns_manifold = true;
    }
}


RtCollisionAlgorithm::~RtCollisionAlgorithm()
{
    if (m_owns_manifold && m_manifold)
	m_dispatcher->releaseManifold(m_manifold);
}


void
RtCollisionAlgorithm::processCollision(const btCollisionObjectWrapper
				       *body_a_wrap,
				       const btCollisionObjectWrapper *body_b_wrap,
				       const btDispatcherInfo &, btManifoldResult *result)
{
    if (!m_manifold)
	return;

    result->setPersistentManifold(m_manifold);
#ifndef USE_PERSISTENT_CONTACTS
    m_manifold->clearManifold();
#endif

    calculate_contact_points(*result,
			     *btRigidBody::upcast(body_a_wrap->getCollisionObject()),
			     *btRigidBody::upcast(body_b_wrap->getCollisionObject()));

#ifdef USE_PERSISTENT_CONTACTS

    if (m_owns_manifold)
	result->refreshContactPoints();

#endif
}


btScalar
RtCollisionAlgorithm::calculateTimeOfImpact(btCollisionObject *,
	btCollisionObject *, const btDispatcherInfo &, btManifoldResult *)
{
    return 1;
}


void
RtCollisionAlgorithm::getAllContactManifolds(btManifoldArray &manifold_array)
{
    if (m_owns_manifold && m_manifold)
	manifold_array.push_back(m_manifold);
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
