/*                       U T I L . C
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
/** @file libanalyze/util.c
 *
 */
#include "common.h"

#include <string.h> /* for memset */

#include "vmath.h"
#include "bu/log.h"
#include "bu/ptbl.h"
#include "bn/mat.h"
#include "raytrace.h"
#include "analyze.h"

int
analyze_get_bbox_rays(fastf_t **rays, point_t min, point_t max, struct bn_tol *tol)
{
    int ret, count;
    point_t mid;
    struct rt_pattern_data *xdata, *ydata, *zdata;

    if (!rays || !tol) return 0;

    if (NEAR_ZERO(DIST_PT_PT_SQ(min, max), VUNITIZE_TOL)) return 0;

    /* We've got the bounding box - set up the grids */
    VSET(mid, (max[0] + min[0])/2, (max[1] + min[1])/2, (max[2] + min[2])/2);

    BU_GET(xdata, struct rt_pattern_data);
    VSET(xdata->center_pt, min[0] - 0.1 * fabs(min[0]), mid[1], mid[2]);
    VSET(xdata->center_dir, 1, 0, 0);
    xdata->vn = 2;
    xdata->pn = 2;
    xdata->n_vec = (vect_t *)bu_calloc(xdata->vn + 1, sizeof(vect_t), "vects array");
    xdata->n_p = (fastf_t *)bu_calloc(xdata->pn + 1, sizeof(fastf_t), "params array");
    xdata->n_p[0] = tol->dist;
    xdata->n_p[1] = tol->dist;
    VSET(xdata->n_vec[0], 0, max[1] - mid[1], 0);
    VSET(xdata->n_vec[1], 0, 0, max[2] - mid[2]);
    ret = rt_pattern(xdata, RT_PATTERN_RECT_ORTHOGRID);
    bu_free(xdata->n_vec, "x vec inputs");
    bu_free(xdata->n_p, "x p inputs");
    if (ret < 0) {
	ret = 0;
	goto memfree;
    }

    BU_GET(ydata, struct rt_pattern_data);
    VSET(ydata->center_pt, mid[0], min[1] - 0.1 * fabs(min[1]), mid[2]);
    VSET(ydata->center_dir, 0, 1, 0);
    ydata->vn = 2;
    ydata->pn = 2;
    ydata->n_vec = (vect_t *)bu_calloc(ydata->vn + 1, sizeof(vect_t), "vects array");
    ydata->n_p = (fastf_t *)bu_calloc(ydata->pn + 1, sizeof(fastf_t), "params array");
    ydata->n_p[0] = tol->dist;
    ydata->n_p[1] = tol->dist;
    VSET(ydata->n_vec[0], max[0] - mid[0], 0, 0);
    VSET(ydata->n_vec[1], 0, 0, max[2] - mid[2]);
    ret = rt_pattern(ydata, RT_PATTERN_RECT_ORTHOGRID);
    bu_free(ydata->n_vec, "y vec inputs");
    bu_free(ydata->n_p, "y p inputs");
    if (ret < 0) {
	ret = 0;
	goto memfree;
    }

    BU_GET(zdata, struct rt_pattern_data);
    VSET(zdata->center_pt, mid[0], mid[1], min[2] - 0.1 * fabs(min[2]));
    VSET(zdata->center_dir, 0, 0, 1);
    zdata->vn = 2;
    zdata->pn = 2;
    zdata->n_vec = (vect_t *)bu_calloc(zdata->vn + 1, sizeof(vect_t), "vects array");
    zdata->n_p = (fastf_t *)bu_calloc(zdata->pn + 1, sizeof(fastf_t), "params array");
    zdata->n_p[0] = tol->dist;
    zdata->n_p[1] = tol->dist;
    VSET(zdata->n_vec[0], max[0] - mid[0], 0, 0);
    VSET(zdata->n_vec[1], 0, max[1] - mid[1], 0);
    ret = rt_pattern(zdata, RT_PATTERN_RECT_ORTHOGRID);
    bu_free(zdata->n_vec, "x vec inputs");
    bu_free(zdata->n_p, "x p inputs");
    if (ret < 0) {
	ret = 0;
	goto memfree;
    }

    /* Consolidate the grids into a single ray array */
    {
	size_t si, sj;
	(*rays) = (fastf_t *)bu_calloc((xdata->ray_cnt + ydata->ray_cnt + zdata->ray_cnt + 1) * 6, sizeof(fastf_t), "rays");
	count = 0;
	for (si = 0; si < xdata->ray_cnt; si++) {
	    for (sj = 0; sj < 6; sj++) {
		(*rays)[6*count+sj] = xdata->rays[6*si + sj];
	    }
	    count++;
	}
	for (si = 0; si < ydata->ray_cnt; si++) {
	    for (sj = 0; sj < 6; sj++) {
		(*rays)[6*count+sj] = ydata->rays[6*si + sj];
	    }
	    count++;
	}
	for (si = 0; si < zdata->ray_cnt; si++) {
	    for (sj = 0; sj < 6; sj++) {
		(*rays)[6*count+sj] = zdata->rays[6*si+sj];
	    }
	    count++;
	}

    }
/*
    bu_log("ray cnt: %d\n", count);
*/
    return count;

memfree:
    /* Free memory not stored in tables */
    bu_free(xdata->rays, "x rays");
    bu_free(ydata->rays, "y rays");
    bu_free(zdata->rays, "z rays");
    BU_PUT(xdata, struct rt_pattern_data);
    BU_PUT(ydata, struct rt_pattern_data);
    BU_PUT(zdata, struct rt_pattern_data);
    return ret;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
