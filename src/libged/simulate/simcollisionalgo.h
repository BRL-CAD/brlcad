/*                          S I M C O L L I S I O N A L G O . H
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
/** @file libged/simulate/simcollisionalgo.h
 *
 *
 * Routines related to performing collision detection using rt
 * This is a custom algorithm that replaces the box-box collision algorithm
 * in bullet
 *
 */

#ifndef SIMULATE_COLL_ALGO_H_
#define SIMULATE_COLL_ALGO_H_

#include "common.h"

#ifdef HAVE_BULLET

#include <iostream>

#include "db.h"
#include "vmath.h"
#include "simulate.h"

#include <btBulletDynamicsCommon.h>


#include "BulletCollision/CollisionDispatch/btActivatingCollisionAlgorithm.h"
#include "BulletCollision/BroadphaseCollision/btBroadphaseProxy.h"
#include "BulletCollision/BroadphaseCollision/btDispatcher.h"
#include "BulletCollision/CollisionDispatch/btCollisionCreateFunc.h"

class btPersistentManifold;

///Raytrace based collision detection
class btRTCollisionAlgorithm : public btActivatingCollisionAlgorithm
{
	bool	m_ownManifold;
	btPersistentManifold*	m_manifoldPtr;

public:
	btRTCollisionAlgorithm(const btCollisionAlgorithmConstructionInfo& ci)
		: btActivatingCollisionAlgorithm(ci) {}

	virtual void processCollision (btCollisionObject* body0,btCollisionObject* body1,const btDispatcherInfo& dispatchInfo,btManifoldResult* resultOut);

	virtual btScalar calculateTimeOfImpact(btCollisionObject* body0,btCollisionObject* body1,const btDispatcherInfo& dispatchInfo,btManifoldResult* resultOut);

	btRTCollisionAlgorithm(btPersistentManifold* mf,const btCollisionAlgorithmConstructionInfo& ci,btCollisionObject* body0,btCollisionObject* body1);

	virtual ~btRTCollisionAlgorithm();

	virtual	void	getAllContactManifolds(btManifoldArray&	manifoldArray)
	{
		if (m_manifoldPtr && m_ownManifold)
		{
			manifoldArray.push_back(m_manifoldPtr);
		}
	}


	struct CreateFunc :public 	btCollisionAlgorithmCreateFunc
	{
		virtual	btCollisionAlgorithm* CreateCollisionAlgorithm(btCollisionAlgorithmConstructionInfo& ci, btCollisionObject* body0,btCollisionObject* body1)
		{
			int bbsize = sizeof(btRTCollisionAlgorithm);
			void* ptr = ci.m_dispatcher1->allocateCollisionAlgorithm(bbsize);
			return new(ptr) btRTCollisionAlgorithm(0,ci,body0,body1);
		}
	};

};


#endif

#endif /* SIMULATE_COLL_ALGO_H_ */

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
