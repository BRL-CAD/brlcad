/*                 R T _ I N S T A N C E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2022 United States Government as represented by
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
/** @file rt_instance.cpp
 *
 * Manage rt instances.
 *
 */


#include "common.h"


#ifdef HAVE_BULLET


#include "rt_instance.hpp"
#include "utility.hpp"

#include "rt/overlap.h"
#include "rt/rt_instance.h"
#include "rt/shoot.h"


namespace
{


struct MultiOverlapHandlerArgs {
    std::vector<std::pair<btVector3, btVector3> > &result;
    const std::string path_a, path_b;
};


HIDDEN void
multioverlap_handler(application * const app, partition * const partition1,
		     bu_ptbl * const region_table, partition * const partition_list)
{
    if (!app || !partition1 || !region_table || !partition_list)
	bu_bomb("missing argument");

    RT_CK_APPLICATION(app);
    RT_CK_PARTITION(partition1);
    BU_CK_PTBL(region_table);
    RT_CK_PT_HD(partition_list);

    MultiOverlapHandlerArgs &args = *static_cast<MultiOverlapHandlerArgs *>
    (app->a_uptr);

    if (BU_PTBL_LEN(region_table) != 2)
	bu_bomb("unexpected region table length");

    btVector3 point_on_a(0.0, 0.0, 0.0), point_on_b(0.0, 0.0, 0.0);
    VJOIN1(point_on_a, app->a_ray.r_pt, partition1->pt_inhit->hit_dist,
	   app->a_ray.r_dir);
    VJOIN1(point_on_b, app->a_ray.r_pt, partition1->pt_outhit->hit_dist,
	   app->a_ray.r_dir);

    const region &region0 = *reinterpret_cast<const region *>(BU_PTBL_GET(
								  region_table, 0));
    const region &region1 = *reinterpret_cast<const region *>(BU_PTBL_GET(
								  region_table, 1));
    RT_CK_REGION(&region0);
    RT_CK_REGION(&region1);

    if (region0.reg_name == args.path_b && region1.reg_name == args.path_a)
	std::swap(point_on_a, point_on_b);
    else if (region0.reg_name != args.path_a || region1.reg_name != args.path_b)
	bu_bomb("unexpected hit regions");

    args.result.push_back(std::make_pair(point_on_a, point_on_b));

    // handle the overlap
    rt_default_multioverlap(app, partition1, region_table, partition_list);
}


}


namespace simulate
{


RtInstance::RtInstance(db_i &db) :
    m_db(db)
{
    RT_CK_DBI(&m_db);
}


std::vector<std::pair<btVector3, btVector3> >
RtInstance::get_overlaps(const db_full_path &path_a, const db_full_path &path_b,
			 const xrays &rays) const
{
    RT_CK_FULL_PATH(&path_a);
    RT_CK_FULL_PATH(&path_b);
    BU_CK_LIST_HEAD(&rays);
    RT_CK_RAY(&rays.ray);

    const AutoPtr<rt_i, rt_free_rti> rti(rt_new_rti(&m_db));

    if (!rti.ptr)
	bu_bomb("rt_new_rti() failed");

    const TemporaryRegionHandle region_a(m_db, path_a), region_b(m_db, path_b);
    const AutoPtr<char> path_a_str(db_path_to_string(&path_a));
    const AutoPtr<char> path_b_str(db_path_to_string(&path_b));
    const char *paths[] = {path_a_str.ptr, path_b_str.ptr};

    if (rt_gettrees(rti.ptr, sizeof(paths) / sizeof(paths[0]), paths, 1))
	bu_bomb("rt_gettrees() failed");

    rt_prep_parallel(rti.ptr, 1);

    std::vector<std::pair<btVector3, btVector3> > result;

    MultiOverlapHandlerArgs handler_args = {result, path_a_str.ptr, path_b_str.ptr};
    application app;
    RT_APPLICATION_INIT(&app);
    app.a_rt_i = rti.ptr;
    app.a_logoverlap = rt_silent_logoverlap;
    app.a_multioverlap = multioverlap_handler;
    app.a_uptr = &handler_args;

    // shoot center ray
    VMOVE(app.a_ray.r_pt, rays.ray.r_pt);
    VMOVE(app.a_ray.r_dir, rays.ray.r_dir);
    rt_shootray(&app);

    const xrays *entry = NULL;

    for (BU_LIST_FOR(entry, xrays, &rays.l)) {
	VMOVE(app.a_ray.r_pt, entry->ray.r_pt);
	VMOVE(app.a_ray.r_dir, entry->ray.r_dir);
	rt_shootray(&app);
    }

    return result;
}


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
