/*              S I M C O L L I S I O N A L G O . C P P
 * BRL-CAD
 *
 * Copyright (c) 2011 United States Government as represented by
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
/*
 * Routines related to performing collision detection using rt.  This
 * is a custom algorithm that replaces the box-box collision algorithm
 * in Bullet.
 */

#include "common.h"

#ifdef HAVE_BULLET

/* system headers */
#include <BulletCollision/CollisionDispatch/btCollisionDispatcher.h>
#include <BulletCollision/CollisionShapes/btBoxShape.h>
#include <BulletCollision/CollisionDispatch/btCollisionObject.h>
#include <BulletCollision/CollisionDispatch/btBoxBoxDetector.h>

/* private headers */
#include "./simcollisionalgo.h"


//#define USE_PERSISTENT_CONTACTS 1



btRTCollisionAlgorithm::btRTCollisionAlgorithm(btPersistentManifold* mf,
					       const btCollisionAlgorithmConstructionInfo& ci,
					       btCollisionObject* obj0,
					       btCollisionObject* obj1)
    : btActivatingCollisionAlgorithm(ci, obj0, obj1),
      m_ownManifold(false),
      m_manifoldPtr(mf)
{
    if (!m_manifoldPtr && m_dispatcher->needsCollision(obj0, obj1)) {
	m_manifoldPtr = m_dispatcher->getNewManifold(obj0, obj1);
	m_ownManifold = true;
    }
}


btRTCollisionAlgorithm::~btRTCollisionAlgorithm()
{
    if (m_ownManifold) {
	if (m_manifoldPtr)
	    m_dispatcher->releaseManifold(m_manifoldPtr);
    }
}


void
btRTCollisionAlgorithm::processCollision(btCollisionObject* body0,
					 btCollisionObject* body1,
					 const btDispatcherInfo& dispatchInfo,
					 btManifoldResult* resultOut)
{
    if (!m_manifoldPtr)
	return;

    //btCollisionObject* col0 = body0;
    //btCollisionObject* col1 = body1;



    //quellage
    bu_log("%d", dispatchInfo.m_stepCount);

    //Report a contact. internally this will be kept persistent, and contact reduction is done
    resultOut->setPersistentManifold(m_manifoldPtr);
#ifndef USE_PERSISTENT_CONTACTS
    m_manifoldPtr->clearManifold();
#endif //USE_PERSISTENT_CONTACTS

    //btDiscreteCollisionDetectorInterface::ClosestPointInput input;
    //input.m_maximumDistanceSquared = BT_LARGE_FLOAT;
    //input.m_transformA = body0->getWorldTransform();
    //input.m_transformB = body1->getWorldTransform();

    //This part will get replaced with a call to rt
    //btBoxBoxDetector detector(box0, box1);
    //detector.getClosestPoints(input, *resultOut, dispatchInfo.m_debugDraw);

    //------------------- DEBUG ---------------------------







    //------------------------------------------------------

#ifdef USE_PERSISTENT_CONTACTS
    // refreshContactPoints is only necessary when using persistent contact points.
    // otherwise all points are newly added
    if (m_ownManifold) {
	resultOut->refreshContactPoints();
    }
#endif //USE_PERSISTENT_CONTACTS

}


btScalar
btRTCollisionAlgorithm::calculateTimeOfImpact(btCollisionObject* /*body0*/, btCollisionObject* /*body1*/, const btDispatcherInfo& /*dispatchInfo*/, btManifoldResult* /*resultOut*/)
{
    //not yet
    return 1.f;
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
