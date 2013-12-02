/*                       U T I L . C
 * BRL-CAD
 *
 * Copyright (c) 2013 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * Copyright 2001 softSurfer, 2012 Dan Sunday
 * This code may be freely used and modified for any purpose
 * providing that this copyright notice is included with it.
 * SoftSurfer makes no warranty for this code, and cannot be held
 * liable for any real or imagined damage resulting from its use.
 * Users of this code must verify correctness for their application.
 *
 */
/** @file util.c
 *
 * This file implements utility functions internal to libbn.
 */

#include "common.h"
#include <stdlib.h>

#include "bn.h"
#include "bn_private.h"

int
bn_coplanar_2d_coord_sys(point_t *origin_pnt, vect_t *u_axis, vect_t *v_axis, const point_t *points_3d, int n)
{
    int i = 0;
    int have_normal = 0;
    plane_t plane;
    fastf_t dist_pt_pt = 0.0;
    fastf_t vdot = 1.0;
    point_t p_farthest;
    int p_farthest_index = 0;
    vect_t normal;
    const struct bn_tol tol = {BN_TOL_MAGIC, BN_TOL_DIST/2.0, BN_TOL_DIST*BN_TOL_DIST/4.0, 1.0e-6, 1.0-1.0e-6};

    /* Step 1 - find center point */
    VSETALL(*origin_pnt, 0.0);
    for (i = 0; i < n; i++) {
	VADD2(*origin_pnt, *origin_pnt, points_3d[i]);
    }
    VSCALE(*origin_pnt, *origin_pnt, 1.0/n);

    /* Step 2 - find furthest points from the center point */
    VSETALL(p_farthest, 0.0);
    for (i = 0; i < n; i++) {
	fastf_t curr_dist = DIST_PT_PT_SQ(*origin_pnt, points_3d[i]);
	if (curr_dist > dist_pt_pt) {
	    dist_pt_pt = curr_dist;
	    VMOVE(p_farthest, points_3d[i]);
	    p_farthest_index = i;
	}
    }
    VSUB2(*u_axis, p_farthest, *origin_pnt);
    VUNITIZE(*u_axis);

    /* Step 3 - find normal vector of plane holding points */
    i = 0;
    dist_pt_pt = DIST_PT_PT(*origin_pnt, p_farthest);
    while (i < n){
	if (i != p_farthest_index) {
	    vect_t temp_vect;
	    fastf_t curr_vdot;
	    VSUB2(temp_vect, points_3d[i], *origin_pnt);
	    VUNITIZE(temp_vect);
	    curr_vdot = fabs(VDOT(temp_vect, *u_axis));
	    if (curr_vdot < vdot) {
		if (!bn_mk_plane_3pts(plane, *origin_pnt, p_farthest, points_3d[i], &tol)) {
		    VSET(normal, plane[0], plane[1], plane[2]);
		    have_normal = 1;
		    vdot = curr_vdot;
		}
	    }
	}
	i++;
    }
    if (!have_normal) return -1;
    VUNITIZE(normal);

    /* Step 4 - use vectors from steps 2 and 3 to find y axis vector */
    VCROSS(*v_axis, *u_axis, normal);
    VUNITIZE(*v_axis);

    return 0;
}


int
bn_coplanar_3d_to_2d(point2d_t **points_2d, const point_t *origin_pnt,
	             const vect_t *u_axis, const vect_t *v_axis,
		     const point_t *points_3d, int n)
{
    int i = 0;
    for (i = 0; i < n; i++) {
	vect_t temp, c, d;
	fastf_t u, v;
	VSUB2(temp, points_3d[i], *origin_pnt);
	VPROJECT(temp, *u_axis, c, d);
	u = (VDOT(c, *u_axis) > 0.0) ? (MAGNITUDE(c)) : (-1.0 * MAGNITUDE(c));
	v = (VDOT(d, *v_axis) > 0.0) ? (MAGNITUDE(d)) : (-1.0 * MAGNITUDE(d));
        V2SET((*points_2d)[i], u, v);
    }

    return 0;
}



int
bn_coplanar_2d_to_3d(point_t **points_3d, const point_t *origin_pnt,
	             const vect_t *u_axis, const vect_t *v_axis,
		     const point2d_t *points_2d, int n)
{
int i;
    vect_t u_x_component, u_y_component, u_z_component;
    vect_t v_x_component, v_y_component, v_z_component;
    vect_t x_axis, y_axis, z_axis;
    vect_t temp;
    fastf_t mag_u_x, mag_u_y, mag_u_z;
    fastf_t mag_v_x, mag_v_y, mag_v_z;
    VSET(x_axis, 1.0, 0.0, 0.0);
    VSET(y_axis, 0.0, 1.0, 0.0);
    VSET(z_axis, 0.0, 0.0, 1.0);
    /* Step 1 - find the 3d X, Y and Z components of u_axis and v_axis */
    VPROJECT(*u_axis, x_axis, u_x_component, temp);
    VPROJECT(temp, y_axis, u_y_component, u_z_component);
    VPROJECT(*v_axis, x_axis, v_x_component, temp);
    VPROJECT(temp, y_axis, v_y_component, v_z_component);
    mag_u_x = (VDOT(u_x_component, x_axis) > 0.0) ? (MAGNITUDE(u_x_component)) : (-1.0 * MAGNITUDE(u_x_component));
    mag_u_y = (VDOT(u_y_component, y_axis) > 0.0) ? (MAGNITUDE(u_y_component)) : (-1.0 * MAGNITUDE(u_y_component));
    mag_u_z = (VDOT(u_z_component, z_axis) > 0.0) ? (MAGNITUDE(u_z_component)) : (-1.0 * MAGNITUDE(u_z_component));
    mag_v_x = (VDOT(v_x_component, x_axis) > 0.0) ? (MAGNITUDE(v_x_component)) : (-1.0 * MAGNITUDE(v_x_component));
    mag_v_y = (VDOT(v_y_component, y_axis) > 0.0) ? (MAGNITUDE(v_y_component)) : (-1.0 * MAGNITUDE(v_y_component));
    mag_v_z = (VDOT(v_z_component, z_axis) > 0.0) ? (MAGNITUDE(v_z_component)) : (-1.0 * MAGNITUDE(v_z_component));

    /* Step 2 - for each 2D point, calculate the (x,y,z) coordinates as follows:
     * (http://math.stackexchange.com/questions/525829/how-to-find-the-3d-coordinate-of-a-2d-point-on-a-known-plane)
     */
    for (i = 0; i < n; i++) {
	vect_t temp_2d;
	VSET(temp_2d, points_2d[i][0]*mag_u_x + (points_2d)[i][1]*mag_v_x,
		(points_2d)[i][0]*mag_u_y + (points_2d)[i][1]*mag_v_y,
		(points_2d)[i][0]*mag_u_z + (points_2d)[i][1]*mag_v_z);
	VADD2((*points_3d)[i], (*origin_pnt), temp_2d);
    }

    return 0;
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

