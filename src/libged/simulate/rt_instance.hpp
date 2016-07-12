/*                 R T _ I N S T A N C E . H P P
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
/** @file rt_instance.hpp
 *
 * Brief description
 *
 */


#ifndef RT_INSTANCE_H
#define RT_INSTANCE_H


#include "raytrace.h"


namespace simulate
{


class TreeUpdater
{
public:
    TreeUpdater(db_i &db_instance, directory &vdirectory);
    ~TreeUpdater();

    void mark_modified();
    tree *get_tree();
    rt_i &get_rt_instance() const;


private:
    TreeUpdater(const TreeUpdater &source);
    TreeUpdater &operator=(const TreeUpdater &source);

    db_i &m_db_instance;
    directory &m_directory;
    rt_db_internal m_comb_internal;

    mutable bool m_is_modified;
    mutable rt_i *m_rt_instance;
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
