/*      R T _ C O L L I S I O N _ A L G O R I T H M . H P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2021 United States Government as represented by
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
/** @file rt_collision_algorithm.hpp
 *
 * A librt-based collision algorithm for RtCollisionShape pairs.
 *
 */


#ifndef SIMULATE_RT_COLLISION_ALGORITHM_H
#define SIMULATE_RT_COLLISION_ALGORITHM_H


#include "common.h"

#include "rt_instance.hpp"


namespace simulate
{


class RtCollisionAlgorithm : public btCollisionAlgorithm
{
public:
    class CreateFunc : public btCollisionAlgorithmCreateFunc
    {
    public:
	explicit CreateFunc(const RtInstance &rt_instance, unsigned grid_radius,
			    btIDebugDraw &debug_draw);

	virtual btCollisionAlgorithm *CreateCollisionAlgorithm(
	    btCollisionAlgorithmConstructionInfo &cinfo,
	    const btCollisionObjectWrapper *body_a_wrap,
	    const btCollisionObjectWrapper *body_b_wrap);


    private:
	const RtInstance &m_rt_instance;
	const unsigned m_grid_radius;
	btIDebugDraw &m_debug_draw;
    };


    explicit RtCollisionAlgorithm(btPersistentManifold *manifold,
				  const btCollisionAlgorithmConstructionInfo &cinfo,
				  const btCollisionObjectWrapper *body_a_wrap,
				  const btCollisionObjectWrapper *body_b_wrap,
				  const RtInstance &rt_instance,
				  unsigned grid_radius, btIDebugDraw &debug_draw);
    virtual ~RtCollisionAlgorithm();

    virtual void processCollision(const btCollisionObjectWrapper *body_a_wrap,
				  const btCollisionObjectWrapper *body_b_wrap,
				  const btDispatcherInfo &dispatch_info, btManifoldResult *result);
    virtual btScalar calculateTimeOfImpact(btCollisionObject *body_a,
					   btCollisionObject *body_b, const btDispatcherInfo &dispatch_info,
					   btManifoldResult *result);
    virtual void getAllContactManifolds(btManifoldArray &manifold_array);


private:
    RtCollisionAlgorithm(const RtCollisionAlgorithm &source);
    RtCollisionAlgorithm &operator=(const RtCollisionAlgorithm &source);

    bool m_owns_manifold;
    btPersistentManifold *m_manifold;
    const RtInstance &m_rt_instance;
    const unsigned m_grid_radius;
    btIDebugDraw &m_debug_draw;
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
