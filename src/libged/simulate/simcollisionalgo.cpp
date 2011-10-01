/*                          S I M C O L L I S I O N A L G O . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
/** @file libged/simulate/simcollisionalgo.cpp
 *
 *
 * Routines related to performing collision detection using rt
 * This is a custom algorithm that replaces the box-box collision algorithm
 * in bullet
 *
 */

#include "common.h"

#ifdef HAVE_BULLET

#include "simcollisionalgo.h"


#include "BulletCollision/CollisionDispatch/btCollisionDispatcher.h"
#include "BulletCollision/CollisionShapes/btBoxShape.h"
#include "BulletCollision/CollisionDispatch/btCollisionObject.h"
#include "BulletCollision/CollisionDispatch/btBoxBoxDetector.h"

#define USE_PERSISTENT_CONTACTS 1

btRTCollisionAlgorithm::btRTCollisionAlgorithm(
		btPersistentManifold* mf,
		const btCollisionAlgorithmConstructionInfo& ci,
		btCollisionObject* obj0,
		btCollisionObject* obj1)
: btActivatingCollisionAlgorithm(ci,obj0,obj1),
m_ownManifold(false),
m_manifoldPtr(mf)
{
	if (!m_manifoldPtr && m_dispatcher->needsCollision(obj0,obj1))
	{
		m_manifoldPtr = m_dispatcher->getNewManifold(obj0,obj1);
		m_ownManifold = true;
	}
}

btRTCollisionAlgorithm::~btRTCollisionAlgorithm()
{
	if (m_ownManifold)
	{
		if (m_manifoldPtr)
			m_dispatcher->releaseManifold(m_manifoldPtr);
	}
}

void btRTCollisionAlgorithm::processCollision (
		btCollisionObject* body0,
		btCollisionObject* body1,
		const btDispatcherInfo& dispatchInfo,
		btManifoldResult* resultOut)
{
	if (!m_manifoldPtr)
		return;

	btCollisionObject*	col0 = body0;
	btCollisionObject*	col1 = body1;
	btBoxShape* box0 = (btBoxShape*)col0->getCollisionShape();
	btBoxShape* box1 = (btBoxShape*)col1->getCollisionShape();

	/// report a contact. internally this will be kept persistent, and contact reduction is done
	resultOut->setPersistentManifold(m_manifoldPtr);
#ifndef USE_PERSISTENT_CONTACTS
	m_manifoldPtr->clearManifold();
#endif //USE_PERSISTENT_CONTACTS

	btDiscreteCollisionDetectorInterface::ClosestPointInput input;
	input.m_maximumDistanceSquared = BT_LARGE_FLOAT;
	input.m_transformA = body0->getWorldTransform();
	input.m_transformB = body1->getWorldTransform();

	//This part will get replaced with a call to rt
	btBoxBoxDetector detector(box0,box1);
	detector.getClosestPoints(input,*resultOut,dispatchInfo.m_debugDraw);



	//------------------- DEBUG ---------------------------

    //Get the user pointers to struct rigid_body, for printing the body name
	struct rigid_body *upA = (struct rigid_body *)col0->getUserPointer();
	struct rigid_body *upB = (struct rigid_body *)col1->getUserPointer();

	if(upA != NULL && upB != NULL){

		btPersistentManifold* contactManifold = resultOut->getPersistentManifold();

		bu_log("processCollision(box/box): %s & %s \n", upA->rb_namep, upB->rb_namep);

		//Get the number of points in this manifold
		int numContacts = contactManifold->getNumContacts();
		int j;

		bu_log("processCollision : Manifold contacts : %d\n", numContacts);

		//Iterate over the points for this manifold
		for (j=0;j<numContacts;j++){
			btManifoldPoint& pt = contactManifold->getContactPoint(j);

			btVector3 ptA = pt.getPositionWorldOnA();
			btVector3 ptB = pt.getPositionWorldOnB();

			bu_log("processCollision: contact %d of %d, %s(%f, %f, %f) , %s(%f, %f, %f)\n",
					j+1, numContacts,
					upA->rb_namep, ptA[0], ptA[1], ptA[2],
					upB->rb_namep, ptB[0], ptB[1], ptB[2]);
		}
	}


	//------------------------------------------------------

#ifdef USE_PERSISTENT_CONTACTS
	//  refreshContactPoints is only necessary when using persistent contact points. otherwise all points are newly added
	if (m_ownManifold)
	{
		resultOut->refreshContactPoints();
	}
#endif //USE_PERSISTENT_CONTACTS

}

btScalar btRTCollisionAlgorithm::calculateTimeOfImpact(btCollisionObject* /*body0*/,btCollisionObject* /*body1*/,const btDispatcherInfo& /*dispatchInfo*/,btManifoldResult* /*resultOut*/)
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
