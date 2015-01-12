/*                W O R L D _ O B J E C T . H P P
 * BRL-CAD
 *
 * Copyright (c) 2015 United States Government as represented by
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
/** @file world_object.hpp
 *
 * Brief description
 *
 */


#include "collision.hpp"
#include "rt_instance.hpp"
#include "physics_world.hpp"

#include "vmath.h"


namespace simulate
{


class MatrixMotionState : public btMotionState
{
public:
    MatrixMotionState(mat_t matrix, const btVector3 &bounding_box_center,
		      TreeUpdater &tree_updater);

    virtual void getWorldTransform(btTransform &dest) const;
    virtual void setWorldTransform(const btTransform &transform);

    rt_i &get_rt_instance() const;


private:
    MatrixMotionState(const MatrixMotionState &source);
    MatrixMotionState &operator=(const MatrixMotionState &source);

    const matp_t m_matrix;
    btVector3 m_bounding_box_center;
    TreeUpdater &m_tree_updater;
};


class WorldObject
{
public:
    WorldObject(mat_t matrix, const btVector3 &bounding_box_center,
		TreeUpdater &tree_updater, btScalar mass,
		const btVector3 &bounding_box_dimensions, const btVector3 &linear_velocity,
		const btVector3 &angular_velocity);

    void add_to_world(PhysicsWorld &world);


private:
    bool m_in_world;
    MatrixMotionState m_motion_state;
    RtCollisionShape m_collision_shape;
    btRigidBody m_rigid_body;
};


}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
