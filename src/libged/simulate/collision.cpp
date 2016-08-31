/*                   C O L L I S I O N . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2016 United States Government as represented by
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


namespace
{


HIDDEN bool
path_match(const char *full_path, const char *toplevel_path)
{
    if (*full_path++ != '/') return false;

    full_path = strchr(full_path, '/');

    if (!full_path++) return false;

    return bu_strncmp(full_path, toplevel_path, strlen(toplevel_path)) == 0;
}


HIDDEN bool
check_path(const btCollisionObjectWrapper &body_a_wrap,
	   const btCollisionObjectWrapper &body_b_wrap,
	   const bu_ptbl &region_table)
{
    const std::string body_a_name = static_cast<const simulate::RtCollisionShape *>
				    (body_a_wrap.getCollisionShape())->get_db_path();
    const std::string body_b_name = static_cast<const simulate::RtCollisionShape *>
				    (body_b_wrap.getCollisionShape())->get_db_path();

    const char * const region1_name = reinterpret_cast<const region *>(BU_PTBL_GET(
					  &region_table, 0))->reg_name;
    const char * const region2_name = reinterpret_cast<const region *>(BU_PTBL_GET(
					  &region_table, 1))->reg_name;

    if (!path_match(region1_name, body_a_name.c_str())
	|| !path_match(region2_name, body_b_name.c_str()))
	if (!path_match(region2_name, body_a_name.c_str())
	    || !path_match(region1_name, body_b_name.c_str()))
	    return false;

    return true;
}


struct OverlapHandlerArgs {
    btManifoldResult &result;
    const btCollisionObjectWrapper &body_a_wrap;
    const btCollisionObjectWrapper &body_b_wrap;
    const btVector3 &normal_world_on_b;
};


HIDDEN void
on_multioverlap(application *app, partition *partition1, bu_ptbl *region_table,
		partition *partition2)
{
    OverlapHandlerArgs &args = *static_cast<OverlapHandlerArgs *>(app->a_uptr);

    if (!check_path(args.body_a_wrap, args.body_b_wrap, *region_table)) {
	rt_default_multioverlap(app, partition1, region_table, partition2);
	return;
    }

    btVector3 point_on_a, point_on_b;
    VJOIN1(point_on_a, app->a_ray.r_pt, partition1->pt_inhit->hit_dist,
	   app->a_ray.r_dir);
    VJOIN1(point_on_b, app->a_ray.r_pt, partition1->pt_outhit->hit_dist,
	   app->a_ray.r_dir);
    btScalar depth = -DIST_PT_PT(point_on_a, point_on_b);
    args.result.addContactPoint(args.normal_world_on_b, point_on_b, depth);

    // handle the overlap
    rt_default_multioverlap(app, partition1, region_table, partition2);
}


HIDDEN void
calculate_contact_points(btManifoldResult &result,
			 const btCollisionObjectWrapper &body_a_wrap,
			 const btCollisionObjectWrapper &body_b_wrap)
{
    const btRigidBody &body_a_rb = *btRigidBody::upcast(
				       body_a_wrap.getCollisionObject());
    const btRigidBody &body_b_rb = *btRigidBody::upcast(
				       body_b_wrap.getCollisionObject());

    // calculate the normal of the contact points as the resultant of the velocities -A and B
    btVector3 normal_world_on_b = body_b_rb.getLinearVelocity();
    VUNITIZE(normal_world_on_b);
    btVector3 v = body_a_rb.getLinearVelocity();
    VUNITIZE(v);
    normal_world_on_b -= v;
    VUNITIZE(normal_world_on_b);

    // shoot a circular grid of rays about `normal_world_on_b`
    xrays *rays;
    {
	BU_ALLOC(rays, xrays);
	BU_LIST_INIT(&rays->l);

	xray &center_ray = rays->ray;
	center_ray.magic = RT_RAY_MAGIC;
	center_ray.index = 0;
	VMOVE(center_ray.r_dir, normal_world_on_b);

	// calculate the overlap volume between the AABBs
	btVector3 overlap_min, overlap_max;
	{
	    btVector3 body_a_aabb_min, body_a_aabb_max, body_b_aabb_min, body_b_aabb_max;
	    body_a_rb.getAabb(body_a_aabb_min, body_a_aabb_max);
	    body_b_rb.getAabb(body_b_aabb_min, body_b_aabb_max);

	    overlap_max = body_a_aabb_max;
	    overlap_max.setMin(body_b_aabb_max);
	    overlap_min = body_a_aabb_min;
	    overlap_min.setMax(body_b_aabb_min);
	}

	// radius of the circle of rays
	// half of the diagonal of the overlap rpp so that rays will cover
	// the entire volume from all orientations
	btScalar radius = (overlap_max - overlap_min).length() / 2.0;

	// calculate the origin of the center ray
	{
	    // center of the overlap volume
	    btVector3 overlap_center = (overlap_min + overlap_max) / 2.0;

	    // step back from overlap_center, along the normal by `radius`,
	    // to ensure that rays start from outside of the overlap region
	    btVector3 center_ray_point = overlap_center - radius * normal_world_on_b;
	    VMOVE(center_ray.r_pt, center_ray_point);
	}

	// calculate the 'up' vector
	vect_t up_vect;
	{
	    btVector3 up = normal_world_on_b.cross(btVector3(1.0, 0.0, 0.0));

	    // use the y-axis if parallel to x-axis
	    if (up.isZero())
		up = normal_world_on_b.cross(btVector3(0.0, 1.0, 0.0));

	    VMOVE(up_vect, up);
	}

	// equivalent to Bullet's collision tolerance (4mm after scaling is enabled)
	const btScalar grid_size = 4.0;

	rt_gen_circular_grid(rays, &center_ray, radius, up_vect, grid_size);
    }

    // shoot the rays
    OverlapHandlerArgs args = {result, body_a_wrap, body_b_wrap, normal_world_on_b};
    application app;
    {
	const simulate::RtCollisionShape &body_a_collision_shape =
	    *static_cast<const simulate::RtCollisionShape *>
	    (body_a_wrap.getCollisionShape());

	RT_APPLICATION_INIT(&app);
	app.a_rt_i = &body_a_collision_shape.get_rt_instance();
	app.a_logoverlap = rt_silent_logoverlap;
	app.a_multioverlap = on_multioverlap;
	app.a_uptr = &args;
    }

    xrays *entry;

    // shoot center ray
    VMOVE(app.a_ray.r_pt, rays->ray.r_pt);
    VMOVE(app.a_ray.r_dir, rays->ray.r_dir);
    rt_shootray(&app);

    while (BU_LIST_WHILE(entry, xrays, &rays->l)) {
	VMOVE(app.a_ray.r_pt, entry->ray.r_pt);
	VMOVE(app.a_ray.r_dir, entry->ray.r_dir);
	rt_shootray(&app);

	BU_LIST_DEQUEUE(&entry->l);
	bu_free(entry, "xrays entry");
    }
}


}


namespace simulate
{


RtCollisionShape::RtCollisionShape(const TreeUpdater &tree_updater,
				   const std::string &db_path, const btVector3 &half_extents) :
    btBoxShape(half_extents),
    m_tree_updater(tree_updater),
    m_db_path(db_path)
{
    m_shapeType = RT_SHAPE_TYPE;
}


const char *
RtCollisionShape::getName() const
{
    return "RtCollisionShape";
}


void
RtCollisionShape::calculateLocalInertia(btScalar mass, btVector3 &inertia) const
{
    // in most cases we can approximate the inertia tensor with that of a bounding box
    btBoxShape::calculateLocalInertia(mass, inertia);
    return;
}


std::string
RtCollisionShape::get_db_path() const
{
    return m_db_path;
}


rt_i &
RtCollisionShape::get_rt_instance() const
{
    return m_tree_updater.get_rt_instance();
}


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

    calculate_contact_points(*result, *body_a_wrap, *body_b_wrap);

#ifdef USE_PERSISTENT_CONTACTS

    if (m_owns_manifold)
	result->refreshContactPoints();

#endif
}


btScalar
RtCollisionAlgorithm::calculateTimeOfImpact(btCollisionObject *,
	btCollisionObject *, const btDispatcherInfo &, btManifoldResult *)
{
    return 1.0;
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
