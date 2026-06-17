/*                    P O I N T S _ E V A L . C
 * BRL-CAD
 *
 * Copyright (c) 1997-2026 United States Government as represented by
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
/** @addtogroup libged */
/** @{ */
/** @file libged/draw/points_eval.c
 *
 *  Given a .g object, raytrace that object pseudo-randomly to generate a point
 *  cloud for visualization
 *
 */
/** @} */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#include "bu/malloc.h"
#include "vmath.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "analyze.h"

#include "./ged_private.h"

int
ged_draw_scene_ref_eval_points(bsg_scene_ref ref)
{
    if (bsg_scene_ref_is_null(ref))
	return BRLCAD_OK; /* nothing to do is fine */

    struct ged_draw_source_state *d = ged_draw_scene_ref_source_data(ref);
    if (!d)
	return BRLCAD_OK; /* nothing to do is fine */

    struct directory *dp = ged_draw_scene_ref_leaf_dp(ref);
    if (!dp)
	return BRLCAD_OK; /* nothing to do is fine */

    struct rt_db_internal intern;
    if (rt_db_get_internal(&intern, dp, d->dbip, NULL) < 0)
	return BRLCAD_ERROR;

    struct rt_primitive_indexed_face_set face_set;
    memset(&face_set, 0, sizeof(face_set));

    int ret = rt_obj_sampled_face_set(&face_set, &intern);
    if (ret == BRLCAD_OK) {
	if (!ged_draw_scene_ref_publish_indexed_face_set(ref,
		(const point_t *)face_set.points, face_set.point_count,
		(const vect_t *)face_set.normals, face_set.normal_count,
		face_set.indices, face_set.index_count))
	    ret = BRLCAD_ERROR;
    }
    if (face_set.points)
	bu_free(face_set.points, "sample point face-set points");
    if (face_set.normals)
	bu_free(face_set.normals, "sample point face-set normals");
    if (face_set.indices)
	bu_free(face_set.indices, "sample point face-set indices");

    rt_db_free_internal(&intern);

    return ret;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
