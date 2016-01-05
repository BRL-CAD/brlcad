/*                   C O L L I S I O N . H P P
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
/** @file collision.hpp
 *
 * Brief description
 *
 */


#ifndef COLLISION_H
#define COLLISION_H

#include "rt_instance.hpp"

#include <string>

#include <btBulletDynamicsCommon.h>


namespace simulate
{


class RtCollisionShape : public btBoxShape
{
public:
    static const int RT_SHAPE_TYPE = CUSTOM_POLYHEDRAL_SHAPE_TYPE;


    RtCollisionShape(const TreeUpdater &tree_updater, const std::string &db_path,
		     const btVector3 &half_extents);

    virtual const char *getName() const;
    virtual void calculateLocalInertia(btScalar mass, btVector3 &inertia) const;

    std::string get_db_path() const;
    rt_i &get_rt_instance() const;


private:
    const TreeUpdater &m_tree_updater;
    const std::string m_db_path;
};


class RtCollisionAlgorithm : public btActivatingCollisionAlgorithm
{
public:
    RtCollisionAlgorithm(btPersistentManifold *manifold,
			 const btCollisionAlgorithmConstructionInfo &cinfo,
			 const btCollisionObjectWrapper *body_a_wrap,
			 const btCollisionObjectWrapper *body_b_wrap);

    virtual ~RtCollisionAlgorithm();

    virtual void processCollision(const btCollisionObjectWrapper *body_a_wrap,
				  const btCollisionObjectWrapper *body_b_wrap,
				  const btDispatcherInfo &dispatch_info, btManifoldResult *result);

    virtual btScalar calculateTimeOfImpact(btCollisionObject *body_a,
					   btCollisionObject *body_b, const btDispatcherInfo &dispatch_info,
					   btManifoldResult *result);

    virtual void getAllContactManifolds(btManifoldArray &manifold_array);


    struct CreateFunc : public btCollisionAlgorithmCreateFunc {
	virtual btCollisionAlgorithm *CreateCollisionAlgorithm(
	    btCollisionAlgorithmConstructionInfo &cinfo,
	    const btCollisionObjectWrapper *body_a_wrap,
	    const btCollisionObjectWrapper *body_b_wrap);
    };


private:
    RtCollisionAlgorithm(const RtCollisionAlgorithm &source);
    RtCollisionAlgorithm &operator=(const RtCollisionAlgorithm &source);

    bool m_owns_manifold;
    btPersistentManifold *m_manifold;
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
