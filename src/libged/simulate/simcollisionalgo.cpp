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


#define USE_PERSISTENT_CONTACTS 1

#define DEBUG_MF 1


btRTCollisionAlgorithm::btRTCollisionAlgorithm(btPersistentManifold* mf,
					       const btCollisionAlgorithmConstructionInfo& ci,
					       btCollisionObject* col0,
					       btCollisionObject* col1)
    : btActivatingCollisionAlgorithm(ci, col0, col1),
m_ownManifold(false),
m_manifoldPtr(mf)
{
if (!m_manifoldPtr)
{
	m_manifoldPtr = m_dispatcher->getNewManifold(col0,col1);
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


void
btRTCollisionAlgorithm::processCollision(btCollisionObject* col0,
					 btCollisionObject* col1,
					 const btDispatcherInfo& dispatchInfo,
					 btManifoldResult* resultOut)
{
    if (!m_manifoldPtr)
	return;

    //quellage
    //bu_log("%d", dispatchInfo.m_stepCount);

    /// report a contact. internally this will be kept persistent, and contact reduction is done
    resultOut->setPersistentManifold(m_manifoldPtr);
#ifndef USE_PERSISTENT_CONTACTS
    m_manifoldPtr->clearManifold();
#endif //USE_PERSISTENT_CONTACTS



    //------------------- DEBUG ---------------------------

    int i;

    //Get the user pointers to struct rigid_body, for printing the body name
    struct rigid_body *rbA = (struct rigid_body *)col0->getUserPointer();
    struct rigid_body *rbB = (struct rigid_body *)col1->getUserPointer();

    if (rbA != NULL && rbB != NULL) {

	   struct sim_manifold *rt_mf = &(rbB->rt_manifold);

	   // Now add the RT contact pairs
	   for (i=0; i<rt_mf->num_contacts; i++){

		   btVector3 ptA, ptB, normalWorldOnB;

		   VMOVE(ptA, rt_mf->contacts[i].ptA);
		   VMOVE(ptB, rt_mf->contacts[i].ptB);
		   VMOVE(normalWorldOnB, rt_mf->contacts[i].normalWorldOnB);

		   //Negative depth for penetration 
		   resultOut->addContactPoint(normalWorldOnB, ptB, rt_mf->contacts[i].depth);

		   bu_log("processCollision: Added RT contact %d, A(ignore): %s(%f, %f, %f) , \
				   B: %s(%f, %f, %f), n(%f, %f, %f), depth=%f\n",
					   i+1,
					rt_mf->rbA->rb_namep, V3ARGS(ptA),
					rt_mf->rbB->rb_namep, V3ARGS(ptB),
					V3ARGS(normalWorldOnB),
					rt_mf->contacts[i].depth);
	   }

    } //end- if (rbA != NULL && rbB...

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
