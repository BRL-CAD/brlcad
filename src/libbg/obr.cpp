/*                         O B R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2025-2026 United States Government as represented by
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
/** @file obr.cpp
 *
 * Use Geometric Tools' oriented box containment query (OrientedBox2) to give
 * us an oriented bounding rectangle for a set of 2D points.
 */

#include "common.h"

#include <array>
#include <cmath>
#include <vector>

#include "Mathematics/ContOrientedBox2.h"
#include "Mathematics/ContOrientedBox3.h"

#include "bu/malloc.h"
#include "bn/tol.h"
#include "bg/plane.h"
#include "bg/chull.h"
#include "bg/obr.h"
#include "./bg_private.h"

using GTF = fastf_t;
using Vec2 = gte::Vector<2, GTF>;
using Vec3 = gte::Vector<3, GTF>;

static inline void pad_extent_if_degenerate(GTF &e)
{
    if (e < BN_TOL_DIST) e = BN_TOL_DIST;
}

extern "C" int
bg_2d_obr(point2d_t *center, vect2d_t *u, vect2d_t *v, const point2d_t *pnts, int pnt_cnt)
{
    if (!center || !u || !v || !pnts || pnt_cnt <= 0)
	return -1;

    std::vector<Vec2> gpoints;
    gpoints.reserve(pnt_cnt);
    for (int i = 0; i < pnt_cnt; ++i) {
	gpoints.emplace_back();
	gpoints.back()[0] = pnts[i][0];
	gpoints.back()[1] = pnts[i][1];
    }

    gte::OrientedBox2<GTF> obox;
    bool ok = gte::GetContainer((int)gpoints.size(), gpoints.data(), obox);
    if (!ok) {
	// Fallback: treat as single point
	V2SET(*center, pnts[0][0], pnts[0][1]);
	V2SET(*u, BN_TOL_DIST, 0.0);
	V2SET(*v, 0.0, BN_TOL_DIST);
	return 0;
    }

    V2SET(*center, obox.center[0], obox.center[1]);

    // Axis vectors (unit)
    vect2d_t au, av;
    V2SET(au, obox.axis[0][0], obox.axis[0][1]);
    V2SET(av, obox.axis[1][0], obox.axis[1][1]);

    GTF e0 = obox.extent[0];
    GTF e1 = obox.extent[1];

    // Pad degeneracies similar to legacy behavior
    pad_extent_if_degenerate(e0);
    pad_extent_if_degenerate(e1);

    V2SCALE(*u, au, e0);
    V2SCALE(*v, av, e1);

    return 0;
}

extern "C" int
bg_3d_coplanar_obr(point_t *center, vect_t *v1, vect_t *v2, const point_t *pnts, int pnt_cnt)
{
    if (!center || !v1 || !v2 || !pnts || pnt_cnt <= 0)
	return -1;

    int ret = 0;
    point_t origin_pnt;
    vect_t u_axis, v_axis;
    ret += coplanar_2d_coord_sys(&origin_pnt, &u_axis, &v_axis, pnts, pnt_cnt);
    if (ret)
	return -2;

    point2d_t *proj2d = (point2d_t *)bu_calloc(pnt_cnt, sizeof(point2d_t), "proj2d");
    ret = coplanar_3d_to_2d(&proj2d, (const point_t *)&origin_pnt,
			    (const vect_t *)&u_axis, (const vect_t *)&v_axis,
			    pnts, pnt_cnt);
    if (ret) {
	bu_free(proj2d, "proj2d");
	return -2;
    }

    point2d_t c2d;
    vect2d_t u2d, v2d;
    ret = bg_2d_obr(&c2d, &u2d, &v2d, (const point2d_t *)proj2d, pnt_cnt);
    if (ret < 0) {
	bu_free(proj2d, "proj2d");
	return ret;
    }

    // Points: center, center+u2d, center+v2d
    point2d_t obr_pts2d[3];
    V2MOVE(obr_pts2d[0], c2d);
    V2ADD2(obr_pts2d[1], c2d, u2d);
    V2ADD2(obr_pts2d[2], c2d, v2d);

    point_t *obr_pts3d = (point_t *)bu_calloc(3, sizeof(point_t), "obr_pts3d");
    ret = coplanar_2d_to_3d(&obr_pts3d, (const point_t *)&origin_pnt,
			    (const vect_t *)&u_axis, (const vect_t *)&v_axis,
			    (const point2d_t *)obr_pts2d, 3);
    if (ret) {
	bu_free(proj2d, "proj2d");
	bu_free(obr_pts3d, "obr_pts3d");
	return -3;
    }

    VMOVE(*center, obr_pts3d[0]);

    vect_t tmp;
    VSUB2(tmp, obr_pts3d[1], *center);
    VMOVE(*v1, tmp);
    VSUB2(tmp, obr_pts3d[2], *center);
    VMOVE(*v2, tmp);

    bu_free(proj2d, "proj2d");
    bu_free(obr_pts3d, "obr_pts3d");

    return 0;
}

extern "C" int
bg_3d_obb(point_t **pnts, const fastf_t *points_3d, int pnt_cnt)
{
    if (!pnts || !points_3d || pnt_cnt <= 0)
	return -1;
    for (int i = 0; i < 8; ++i) {
	if (!pnts[i])
	    return -1;
    }

    std::vector<Vec3> gpoints;
    gpoints.reserve(pnt_cnt);
    for (int i = 0; i < pnt_cnt; ++i) {
	const fastf_t *point = &points_3d[i * ELEMENTS_PER_POINT];
	gpoints.emplace_back();
	gpoints.back()[0] = point[0];
	gpoints.back()[1] = point[1];
	gpoints.back()[2] = point[2];
    }

    gte::OrientedBox3<GTF> obox;
    if (!gte::GetContainer(static_cast<int>(gpoints.size()), gpoints.data(), obox))
	return -1;

    std::array<Vec3, 8> vertices;
    obox.GetVertices(vertices);

    /* ARB8 ordering makes the three edges from point 0 be 0->4, 0->1,
     * and 0->3.  Preserve that convention for callers consuming an OBB. */
    const int arb_order[8] = {0, 1, 3, 2, 4, 5, 7, 6};
    for (int i = 0; i < 8; ++i) {
	const Vec3 &vertex = vertices[arb_order[i]];
	VSET(*pnts[i], vertex[0], vertex[1], vertex[2]);
    }

    const fastf_t obb_volume = DIST_PNT_PNT(*pnts[0], *pnts[4]) *
	DIST_PNT_PNT(*pnts[0], *pnts[1]) * DIST_PNT_PNT(*pnts[0], *pnts[3]);
    point_t aabb_min, aabb_max;
    VSETALL(aabb_min, INFINITY);
    VSETALL(aabb_max, -INFINITY);
    for (int i = 0; i < pnt_cnt; ++i)
	VMINMAX(aabb_min, aabb_max, &points_3d[i * ELEMENTS_PER_POINT]);
    const fastf_t aabb_volume = (aabb_max[X] - aabb_min[X]) *
	(aabb_max[Y] - aabb_min[Y]) * (aabb_max[Z] - aabb_min[Z]);
    if (aabb_volume < obb_volume) {
	VSET(*pnts[0], aabb_min[X], aabb_min[Y], aabb_min[Z]);
	VSET(*pnts[1], aabb_min[X], aabb_max[Y], aabb_min[Z]);
	VSET(*pnts[2], aabb_min[X], aabb_max[Y], aabb_max[Z]);
	VSET(*pnts[3], aabb_min[X], aabb_min[Y], aabb_max[Z]);
	VSET(*pnts[4], aabb_max[X], aabb_min[Y], aabb_min[Z]);
	VSET(*pnts[5], aabb_max[X], aabb_max[Y], aabb_min[Z]);
	VSET(*pnts[6], aabb_max[X], aabb_max[Y], aabb_max[Z]);
	VSET(*pnts[7], aabb_max[X], aabb_min[Y], aabb_max[Z]);
    }

    return 0;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
