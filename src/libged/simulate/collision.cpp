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

#include <BulletCollision/CollisionDispatch/btCollisionObjectWrapper.h>
#include <BulletCollision/CollisionDispatch/btCollisionDispatcher.h>


namespace simulate
{


namespace collision
{


RtCollisionShape::RtCollisionShape(const btVector3 &half_extents) :
    btBoxShape(half_extents)
{}


RtCollisionShape::~RtCollisionShape()
{}


btCollisionAlgorithm *
RtCollisionAlgorithm::CreateFunc::CreateCollisionAlgorithm(
    btCollisionAlgorithmConstructionInfo &cinfo,
    const btCollisionObjectWrapper *body_a_wrap,
    const btCollisionObjectWrapper *body_b_wrap)
{
    int bbsize = sizeof(RtCollisionAlgorithm);
    void *ptr = cinfo.m_dispatcher1->allocateCollisionAlgorithm(bbsize);
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
RtCollisionAlgorithm::processCollision(const btCollisionObjectWrapper *,
				       const btCollisionObjectWrapper *,
				       const btDispatcherInfo &, btManifoldResult *result)
{
    if (!m_manifold)
	return;

    result->setPersistentManifold(m_manifold);
#ifndef USE_PERSISTENT_CONTACTS
    m_manifold->clearManifold();
#endif

    // calculations

    const std::size_t num_contacts = 1;

    for (std::size_t i = 0; i < num_contacts; ++i) {
	const btVector3 pt_b(0, 0, 0);
	const btVector3 normal_world_on_b(0, 0, 0);
	const btScalar depth = 0;
	result->addContactPoint(normal_world_on_b, pt_b, depth);
    }

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
