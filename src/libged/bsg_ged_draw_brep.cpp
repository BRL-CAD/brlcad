/*              B S G _ G E D _ D R A W _ B R E P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this file; see the file named COPYING for more information.
 */
/** @file bsg_ged_draw_brep.cpp
 *
 * Private BREP/BSPLINE typed geometry publication for libged drawing paths.
 */

#include "common.h"

#include "bn/tol.h"
#include "bu/malloc.h"
#include "rt/functab.h"
#include "rt/geom.h"
#include "rt/primitives/brep.h"
#include "rt/primitives/bspline.h"
#include "./ged_private.h"
#include "./bsg_ged_draw_private.h"


static void
_ged_draw_realization_line_set_free(struct rt_primitive_lod_realization *realization)
{
    if (!realization)
	return;

    if (realization->line_points)
	bu_free(realization->line_points, "primitive LoD line-set points");
    if (realization->line_commands)
	bu_free(realization->line_commands, "primitive LoD line-set commands");
    realization->line_points = NULL;
    realization->line_commands = NULL;
    realization->line_count = 0;
    realization->line_capacity = 0;
    realization->has_line_set = 0;
}


static int
_ged_draw_scene_ref_publish_realization_line_set(bsg_scene_ref ref,
						 struct rt_primitive_lod_realization *realization)
{
    int ok = 0;

    if (!realization || !realization->has_line_set)
	return 0;

    ok = realization->line_count ?
	ged_draw_scene_ref_publish_line_set(ref,
		(const point_t *)realization->line_points,
		realization->line_commands, realization->line_count) :
	ged_draw_scene_ref_geometry_clear(ref);

    _ged_draw_realization_line_set_free(realization);
    return ok;
}


extern "C" int
ged_draw_scene_ref_publish_brep_wireframe_line_set(bsg_scene_ref ref,
						   const struct rt_brep_internal *bi,
						   const struct bn_tol *tol)
{
    struct rt_primitive_lod_realization realization = {};
    struct rt_db_internal brep_ip;
    int ret = 0;

    if (!bi)
	return 0;

    RT_BREP_CK_MAGIC(bi);
    if (!bi->brep)
	return 0;

    RT_DB_INTERNAL_INIT(&brep_ip);
    brep_ip.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    brep_ip.idb_type = ID_BREP;
    brep_ip.idb_meth = &OBJ[ID_BREP];
    brep_ip.idb_ptr = (void *)bi;

    ret = rt_brep_wireframe_line_set(&realization, &brep_ip, tol);
    if (ret < 0) {
	_ged_draw_realization_line_set_free(&realization);
	return 0;
    }

    return _ged_draw_scene_ref_publish_realization_line_set(ref, &realization);
}


extern "C" int
ged_draw_scene_ref_publish_bspline_wireframe_line_set(bsg_scene_ref ref,
						      struct rt_db_internal *ip,
						      const struct bn_tol *tol)
{
    struct rt_primitive_lod_realization realization = {};
    int ret = 0;

    if (!ip || ip->idb_type != ID_BSPLINE || !ip->idb_ptr)
	return 0;

    ret = rt_nurb_wireframe_line_set(&realization, ip, tol);
    if (ret < 0) {
	_ged_draw_realization_line_set_free(&realization);
	return 0;
    }

    return _ged_draw_scene_ref_publish_realization_line_set(ref, &realization);
}
