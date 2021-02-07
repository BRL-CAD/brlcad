/*             R T _ M O T I O N _ S T A T E . H P P
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
/** @file rt_motion_state.hpp
 *
 * Motion state for objects in an rt instance.
 *
 */


#ifndef SIMULATE_RT_MOTION_STATE_H
#define SIMULATE_RT_MOTION_STATE_H


#include "common.h"

#include "utility.hpp"

#include <btBulletDynamicsCommon.h>


namespace simulate
{


class RtMotionState : public btMotionState
{
public:
    explicit RtMotionState(db_i &db, const db_full_path &path,
			   const btVector3 &center_of_mass);

    const db_full_path &get_path() const;

    virtual void getWorldTransform(btTransform &dest) const;
    virtual void setWorldTransform(const btTransform &transform);


private:
    db_i &m_db;
    db_full_path m_path;
    const AutoPtr<db_full_path, db_free_full_path> m_autofree_m_path;
    btTransform m_transform;
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
