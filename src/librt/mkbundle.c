/*                      M K B U N D L E . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2021 United States Government as represented by
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
/** @addtogroup librt */
/** @{ */
/** @file librt/mkbundle.c
 *
 */
/** @} */

#include "common.h"

#include <math.h>
#include "bio.h"

#include "vmath.h"

#include "bn.h"
#include "raytrace.h"


/**
 * Make a bundle of rays around a main ray, with a circular exterior,
 * and spiral filling of the interior.  The outer periphery is sampled
 * with rays_per_ring additional rays, preferably at least 3.
 *
 * rp[] must be of size (rays_per_ring*nring)+1.  Entry 0 is the main
 * ray, and must be provided by the caller.  The remaining entries
 * will be filled in with extra rays.  The radius of the bundle is
 * given in mm.
 *
 * rp[0].r_dir must have unit length.
 *
 * XXX Should we require a and b as inputs, for efficiency?
 */

int
rt_raybundle_maker(struct xray *rp, double radius, const fastf_t *avec, const fastf_t *bvec, int rays_per_ring, int nring)
{
    register struct xray *rayp = rp+1;
    int ring = 0;
    double fraction = 1.0;
    double theta = 0.0;
    double delta = 0.0;
    double radial_scale = 0.0;
    int count = 0;

    rp[0].index = count++;
    rp[0].magic =RT_RAY_MAGIC;

    for (ring=0; ring < nring; ring++) {
	register int i;

	delta = M_2PI / rays_per_ring;
	fraction = ((double)(ring+1)) / nring;
	theta = delta * fraction;	/* spiral skew */
	radial_scale = radius * fraction;
	for (i=0; i < rays_per_ring; i++) {
	    register double ct, st;
	    /* pt = V + cos(theta) * A + sin(theta) * B */
	    ct = cos(theta) * radial_scale;
	    st = sin(theta) * radial_scale;
	    VJOIN2(rayp->r_pt, rp[0].r_pt, ct, avec, st, bvec);
	    VMOVE(rayp->r_dir, rp[0].r_dir);
	    rayp->index = count++;
	    rayp->magic = RT_RAY_MAGIC;
	    theta += delta;
	    rayp++;
	}
    }
    return count;
}


int
rt_gen_circular_grid(struct xrays *rays, const struct xray *center_ray, fastf_t radius, const fastf_t *up_vector, fastf_t gridsize)
{
    vect_t dir;
    vect_t avec;
    vect_t bvec;
    vect_t uvec;

    VMOVE(dir, center_ray->r_dir);
    VMOVE(uvec, up_vector);
    VUNITIZE(uvec);
    VSCALE(bvec, uvec, radius);

    VCROSS(avec, dir, up_vector);
    VUNITIZE(avec);
    VSCALE(avec, avec, radius);

    return rt_gen_elliptical_grid(rays, center_ray, avec, bvec, gridsize);
}


int
rt_gen_elliptical_grid(struct xrays *rays, const struct xray *center_ray, const fastf_t *avec, const fastf_t *bvec, fastf_t gridsize)
{
    register struct xrays *xrayp;
    int count = 0;
    point_t C;
    vect_t dir;
    vect_t a_dir;
    vect_t b_dir;

    fastf_t a = MAGNITUDE(avec);
    fastf_t b = MAGNITUDE(bvec);
    fastf_t x, y;

    int acpr = a / gridsize;
    int bcpr = b / gridsize;

    VMOVE(a_dir, avec);
    VUNITIZE(a_dir);

    VMOVE(b_dir, bvec);
    VUNITIZE(b_dir);

    VMOVE(C, center_ray->r_pt);
    VMOVE(dir, center_ray->r_dir);
    /* make sure avec perpendicular to bvec perpendicular to ray direction */
    BU_ASSERT(NEAR_ZERO(VDOT(avec, bvec), RT_DOT_TOL));
    BU_ASSERT(NEAR_ZERO(VDOT(avec, dir), RT_DOT_TOL));

    for (y=gridsize * (-bcpr); y <= b; y=y+gridsize) {
	for (x= gridsize * (-acpr); x <= a; x=x+gridsize) {
	    if (((x*x)/(a*a) + (y*y)/(b*b)) < 1) {
		BU_ALLOC(xrayp, struct xrays);
		VJOIN2(xrayp->ray.r_pt, C, x, a_dir, y, b_dir);
		VMOVE(xrayp->ray.r_dir, dir);
		xrayp->ray.index = count++;
		xrayp->ray.magic = RT_RAY_MAGIC;
		BU_LIST_APPEND(&rays->l, &xrayp->l);
	    }
	}
    }
    return count;
}

int
rt_gen_conic(struct xrays *rays, const struct xray *center_ray,
	     fastf_t theta, vect_t up_vector, int rays_per_radius)
{
    int count = 0;

    point_t start;
    vect_t orig_dir;

    fastf_t x, y;

    /* Setting radius to tan(theta) works because, as shown in the
     * following diagram, the ray that starts at the given point and
     * passes through orig_dir + (radius in any orthogonal direction)
     * has an angle of theta with the original ray; when the
     * resulting vector is normalized, the angle is preserved.
     */
    fastf_t radius = tan(theta);
    fastf_t rsq = radius * radius; /* radius-squared, for use in the loop */

    fastf_t gridsize = 2 * radius / (rays_per_radius - 1);

    vect_t a_dir, b_dir;

    register struct xrays *xrayp;

    VMOVE(start, center_ray->r_pt);
    VMOVE(orig_dir, center_ray->r_dir);

    /* Create vectors a_dir, b_dir that are orthogonal to orig_dir. */
    VMOVE(b_dir, up_vector);
    VUNITIZE(b_dir);

    VCROSS(a_dir, orig_dir, up_vector);
    VUNITIZE(a_dir);

    for (y = -radius; y <= radius; y += gridsize) {
	vect_t tmp;
	printf("y:%f\n", y);
	VSCALE(tmp, b_dir, y);
	printf("y_partofit: %f,%f,%f\n", V3ARGS(tmp));
	for (x = -radius; x <= radius; x += gridsize) {
	    if (((x*x)/rsq + (y*y)/rsq) <= 1) {
		BU_ALLOC(xrayp, struct xrays);
		VMOVE(xrayp->ray.r_pt, start);
		VJOIN2(xrayp->ray.r_dir, orig_dir, x, a_dir, y, b_dir);
		VUNITIZE(xrayp->ray.r_dir);
		xrayp->ray.index = count++;
		xrayp->ray.magic = RT_RAY_MAGIC;
		BU_LIST_APPEND(&rays->l, &xrayp->l);
	    }
	}
    }
    return count;
}


int
rt_gen_frustum(struct xrays *rays, const struct xray *center_ray,
	       const vect_t a_vec, const vect_t b_vec,
	       const fastf_t a_theta, const fastf_t b_theta,
	       const fastf_t a_num, const fastf_t b_num)
{
    int count = 0;

    point_t start;
    vect_t orig_dir;

    fastf_t x, y;

    fastf_t a_length = tan(a_theta);
    fastf_t b_length = tan(b_theta);
    fastf_t a_inc = 2 * a_length / (a_num - 1);
    fastf_t b_inc = 2 * b_length / (b_num - 1);

    vect_t a_dir, b_dir;

    register struct xrays *xrayp;

    VMOVE(start, center_ray->r_pt);
    VMOVE(orig_dir, center_ray->r_dir);

    VMOVE(a_dir, a_vec);
    VUNITIZE(a_dir);
    VMOVE(b_dir, b_vec);
    VUNITIZE(b_dir);

    /* This adds BN_TOL_DIST to the *_length variables in the
     * condition because in some cases, floating-point problems can
     * make extremely close numbers compare incorrectly.
     *
     */
    for (y = -b_length; y <= b_length + BN_TOL_DIST; y += b_inc) {
	for (x = -a_length; x <= a_length + BN_TOL_DIST; x += a_inc) {
	    BU_ALLOC(xrayp, struct xrays);
	    VMOVE(xrayp->ray.r_pt, start);
	    VJOIN2(xrayp->ray.r_dir, orig_dir, x, a_dir, y, b_dir);
	    VUNITIZE(xrayp->ray.r_dir);
	    xrayp->ray.index = count++;
	    xrayp->ray.magic = RT_RAY_MAGIC;
	    BU_LIST_APPEND(&rays->l, &xrayp->l);
	}
    }
    return count;
}

int rt_gen_rect(struct xrays *rays, const struct xray *center_ray,
		const vect_t a_vec, const vect_t b_vec,
		const fastf_t da, const fastf_t db)
{
    int count = 0;

    point_t orig_start;
    vect_t dir;

    fastf_t x, y;

    fastf_t a_length = MAGNITUDE(a_vec);
    fastf_t b_length = MAGNITUDE(b_vec);

    vect_t a_dir;
    vect_t b_dir;

    register struct xrays *xrayp;

    VMOVE(orig_start, center_ray->r_pt);
    VMOVE(dir, center_ray->r_dir);

    VMOVE(a_dir, a_vec);
    VUNITIZE(a_dir);

    VMOVE(b_dir, b_vec);
    VUNITIZE(b_dir);

    for (y = -b_length; y <= b_length; y += db) {
	for (x = -a_length; x <= a_length; x += da) {
	    BU_ALLOC(xrayp, struct xrays);
	    VJOIN2(xrayp->ray.r_pt, orig_start, x, a_dir, y, b_dir);
	    VMOVE(xrayp->ray.r_dir, dir);
	    xrayp->ray.index = count++;
	    xrayp->ray.magic = RT_RAY_MAGIC;
	    BU_LIST_APPEND(&rays->l, &xrayp->l);
	}
    }
    return count;
}


HIDDEN int
rt_pattern_rect_orthogrid(fastf_t **rays, size_t *ray_cnt, const point_t center_pt, const vect_t dir,
		const vect_t a_vec, const vect_t b_vec, const fastf_t da, const fastf_t db)
{
    int count = 0;
    point_t pt;
    vect_t a_dir;
    vect_t b_dir;
    fastf_t x, y;
    fastf_t a_length = MAGNITUDE(a_vec);
    fastf_t b_length = MAGNITUDE(b_vec);

    if (!rays || !ray_cnt) return -1;

    if (NEAR_ZERO(a_length, SMALL_FASTF) || NEAR_ZERO(b_length, SMALL_FASTF)) return -1;
    if (NEAR_ZERO(da, SMALL_FASTF) || NEAR_ZERO(db, SMALL_FASTF)) return -1;

    VMOVE(a_dir, a_vec);
    VUNITIZE(a_dir);

    VMOVE(b_dir, b_vec);
    VUNITIZE(b_dir);

    /* Find out how much memory we'll need and get it */
    for (y = -b_length; y <= b_length; y += db) {
	for (x = -a_length; x <= a_length; x += da) {
	    count++;
	}
    }
    *(rays) = (fastf_t *)bu_calloc(sizeof(fastf_t) * 6, count + 1, "rays");

    /* Now that we have memory, reset count so it can
     * be used to index into the array */
    count = 0;

    /* Build the rays */
    for (y = -b_length; y <= b_length; y += db) {
	for (x = -a_length; x <= a_length; x += da) {
	    VJOIN2(pt, center_pt, x, a_dir, y, b_dir);
	    (*rays)[6*count] = pt[0];
	    (*rays)[6*count+1] = pt[1];
	    (*rays)[6*count+2] = pt[2];
	    (*rays)[6*count+3] = dir[0];
	    (*rays)[6*count+4] = dir[1];
	    (*rays)[6*count+5] = dir[2];
	    count++;
	}
    }
    *(ray_cnt) = count;
    return count;
}


HIDDEN int
rt_pattern_rect_perspgrid(fastf_t **rays, size_t *ray_cnt, const point_t center_pt, const vect_t dir,
	       const vect_t a_vec, const vect_t b_vec,
	       const fastf_t a_theta, const fastf_t b_theta,
	       const fastf_t a_num, const fastf_t b_num)
{
    int count = 0;
    vect_t rdir;
    vect_t a_dir, b_dir;
    fastf_t x, y;
    fastf_t a_length = tan(a_theta);
    fastf_t b_length = tan(b_theta);
    fastf_t a_inc = 2 * a_length / (a_num - 1);
    fastf_t b_inc = 2 * b_length / (b_num - 1);

    VMOVE(a_dir, a_vec);
    VUNITIZE(a_dir);
    VMOVE(b_dir, b_vec);
    VUNITIZE(b_dir);

    /* Find out how much memory we'll need and get it */
    for (y = -b_length; y <= b_length + BN_TOL_DIST; y += b_inc) {
	for (x = -a_length; x <= a_length + BN_TOL_DIST; x += a_inc) {
	    count++;
	}
    }
    *(rays) = (fastf_t *)bu_calloc(sizeof(fastf_t) * 6, count + 1, "rays");

    /* Now that we have memory, reset count so it can
     * be used to index into the array */
    count = 0;

    /* This adds BN_TOL_DIST to the *_length variables in the
     * condition because in some cases, floating-point problems can
     * make extremely close numbers compare incorrectly. */
    for (y = -b_length; y <= b_length + BN_TOL_DIST; y += b_inc) {
	for (x = -a_length; x <= a_length + BN_TOL_DIST; x += a_inc) {
	    VJOIN2(rdir, dir, x, a_dir, y, b_dir);
	    VUNITIZE(rdir);
	    (*rays)[6*count] = center_pt[0];
	    (*rays)[6*count+1] = center_pt[1];
	    (*rays)[6*count+2] = center_pt[2];
	    (*rays)[6*count+3] = rdir[0];
	    (*rays)[6*count+4] = rdir[1];
	    (*rays)[6*count+5] = rdir[2];
	    count++;

	}
    }
    *(ray_cnt) = count;
    return count;
}

HIDDEN int
rt_pattern_circ_spiral(fastf_t **rays, size_t *ray_cnt, const point_t center_pt, const vect_t dir,
	const double radius, const int rays_per_ring, const int nring, const double delta)
{
    int ring = 0;
    double fraction = 1.0;
    double theta = 0.0;
    double radial_scale = 0.0;
    vect_t avec, bvec;
    int ray_index = 0;

    bn_vec_ortho(avec, dir);
    VCROSS(bvec, dir, avec);
    VUNITIZE(bvec);

    if (!rays || !ray_cnt) return -1;

    *(rays) = (fastf_t *)bu_calloc(sizeof(fastf_t) * 6, (rays_per_ring * nring) + 1, "rays");

    for (ring = 0; ring < nring; ring++) {
	register int i;

	fraction = ((double)(ring+1)) / nring;
	theta = delta * fraction;       /* spiral skew */
	radial_scale = radius * fraction;
	for (i=0; i < rays_per_ring; i++) {
	    register double ct, st;
	    point_t pt;
	    /* pt = V + cos(theta) * A + sin(theta) * B */
	    ct = cos(theta) * radial_scale;
	    st = sin(theta) * radial_scale;
	    VJOIN2(pt, center_pt, ct, avec, st, bvec);
	    (*rays)[6*ray_index] = pt[0];
	    (*rays)[6*ray_index+1] = pt[1];
	    (*rays)[6*ray_index+2] = pt[2];
	    (*rays)[6*ray_index+3] = dir[0];
	    (*rays)[6*ray_index+4] = dir[1];
	    (*rays)[6*ray_index+5] = dir[2];
	    theta += delta;
	    ray_index++;
	}
    }
    *(ray_cnt) = ray_index;
    return ray_index;
}

int
rt_pattern(struct rt_pattern_data *data, rt_pattern_t type)
{
    if (!data) return -1;
    switch (type) {
	case RT_PATTERN_RECT_ORTHOGRID:
	    if (data->pn < 2 || data->vn < 2) return -1;
	    return rt_pattern_rect_orthogrid(&(data->rays), &(data->ray_cnt), data->center_pt, data->center_dir,
		    data->n_vec[0], data->n_vec[1], data->n_p[0], data->n_p[1]);
	    break;
	case RT_PATTERN_RECT_PERSPGRID:
	    return rt_pattern_rect_perspgrid(&(data->rays), &(data->ray_cnt), data->center_pt, data->center_dir,
		    data->n_vec[0], data->n_vec[1], data->n_p[0], data->n_p[1], data->n_p[2], data->n_p[3]);
	    break;
	case RT_PATTERN_CIRC_SPIRAL:
	    if (data->pn < 4) return -1;
	    return rt_pattern_circ_spiral(&(data->rays), &(data->ray_cnt), data->center_pt, data->center_dir,
		    data->n_p[0], data->n_p[1], data->n_p[2], data->n_p[3]);
	    break;
	default:
	    bu_log("Error - unknown pattern type %d\n", type);
	    return -1;
	    break;
    }
    return -1;
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
