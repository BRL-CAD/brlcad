/*                W O R L D _ O B J E C T . H P P
 * BRL-CAD
 *
 * Copyright (c) 2015-2016 United States Government as represented by
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


#ifndef WORLD_OBJECT_H
#define WORLD_OBJECT_H


#include "collision.hpp"


namespace simulate
{


class MatrixMotionState : public btMotionState
{
public:
    MatrixMotionState(mat_t matrix, const btVector3 &origin,
		      TreeUpdater &tree_updater);

    virtual void getWorldTransform(btTransform &dest) const;
    virtual void setWorldTransform(const btTransform &transform);


private:
    MatrixMotionState(const MatrixMotionState &source);
    MatrixMotionState &operator=(const MatrixMotionState &source);

    const matp_t m_matrix;
    const btVector3 m_origin;
    TreeUpdater &m_tree_updater;
};


class WorldObject
{
public:
    static WorldObject *create(db_i &db_instance, directory &vdirectory,
			       mat_t matrix, TreeUpdater &tree_updater);
    ~WorldObject();

    void add_to_world(btDiscreteDynamicsWorld &world);


private:
    WorldObject(const WorldObject &source);
    WorldObject &operator=(const WorldObject &source);

    WorldObject(directory &vdirectory, mat_t matrix, TreeUpdater &tree_updater,
		btVector3 bounding_box_pos, btVector3 bounding_box_dims, btScalar mass);


    btDiscreteDynamicsWorld *m_world;
    MatrixMotionState m_motion_state;
    RtCollisionShape m_collision_shape;
    btRigidBody m_rigid_body;
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
