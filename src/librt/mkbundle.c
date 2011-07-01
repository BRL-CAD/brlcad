/*                      M K B U N D L E . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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

#include <stdio.h>
#include <math.h>
#include "bio.h"

#include "vmath.h"
#include "bu.h"
#include "bn.h"
#include "raytrace.h"


/**
 * R T _ R A Y B U N D L E _ M A K E R
 *
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
RT_EXPORT extern int rt_raybundle_maker(struct xray *rp, double radius, const fastf_t *avec, const fastf_t *bvec, int rays_per_ring, int nring);

int
rt_raybundle_maker(struct xray *rp, double radius, const fastf_t *avec, const fastf_t *bvec, int rays_per_ring, int nring)
{
    register struct xray	*rayp = rp+1;
    int	ring;
    double	fraction = 1.0;
    double	theta;
    double	delta;
    double	radial_scale;
    int	count = 0;

    rp[0].index = count++;
    rp[0].magic =RT_RAY_MAGIC;

    for ( ring=0; ring < nring; ring++ )  {
	register int i;

	theta = 0;
	delta = bn_twopi / rays_per_ring;
	fraction = ((double)(ring+1)) / nring;
	theta = delta * fraction;	/* spiral skew */
	radial_scale = radius * fraction;
	for ( i=0; i < rays_per_ring; i++ )  {
	    register double	ct, st;
	    /* pt = V + cos(theta) * A + sin(theta) * B */
	    ct = cos(theta) * radial_scale;
	    st = sin(theta) * radial_scale;
	    VJOIN2( rayp->r_pt, rp[0].r_pt, ct, avec, st, bvec );
	    VMOVE( rayp->r_dir, rp[0].r_dir );
	    rayp->index = count++;
	    rayp->magic = RT_RAY_MAGIC;
	    theta += delta;
	    rayp++;
	}
    }
    return count;
}

/**
 * R T _ G E N _ C I R C U L A R _ G R I D
 *
 * Make a bundle of rays around a main ray using a uniform rectangular grid pattern with a circular extent.
 * The radius, gridsize is given in mm.
 *
 * rp[0].r_dir must have unit length.
 *
 */
int
rt_gen_circular_grid(struct xrays *rays, const struct xray *center_ray, fastf_t radius, const fastf_t *up_vector, fastf_t gridsize)
{
    vect_t dir;
    vect_t avec;
    vect_t bvec;
    vect_t uvec;

    VMOVE(dir,center_ray->r_dir);
    VMOVE(uvec, up_vector);
    VUNITIZE(uvec);
    VSCALE(bvec,uvec,radius);

    VCROSS(avec, dir, up_vector);
    VUNITIZE(avec);
    VSCALE(avec,avec,radius);

    return rt_gen_elliptical_grid(rays,center_ray,avec,bvec,gridsize);
}

/**
 * R T _ G E N _ E L L I P T I C A L _ G R I D
 *
 * Make a bundle of rays around a main ray using a uniform rectangular grid pattern with an elliptical extent.
 *
 * avec and bvec a.  The gridsize is
 * given in mm.
 *
 * rp[0].r_dir must have unit length.
 *
 */
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
    fastf_t x,y;

    int acpr = a / gridsize;
    int bcpr = b / gridsize;

    VMOVE(a_dir,avec);
    VUNITIZE(a_dir);

    VMOVE(b_dir,bvec);
    VUNITIZE(b_dir);

    VMOVE(C,center_ray->r_pt);
    VMOVE(dir,center_ray->r_dir);
    /* make sure avec perpendicular to bvec perpendicular to ray direction */
    BU_ASSERT(NEAR_ZERO(VDOT(avec,bvec), VUNITIZE_TOL));
    BU_ASSERT(NEAR_ZERO(VDOT(avec,dir), VUNITIZE_TOL));

    for ( y=gridsize * (-bcpr); y <= b; y=y+gridsize ) {
	for ( x= gridsize * (-acpr); x <= a; x=x+gridsize ) {
	    if (((x*x)/(a*a) + (y*y)/(b*b)) < 1) {
		xrayp = (struct xrays *)bu_calloc( sizeof( struct xrays ),
						   1,
						   "bundled ray" );
		VJOIN2( xrayp->ray.r_pt, C, x, a_dir, y, b_dir );
		VMOVE( xrayp->ray.r_dir, dir );
		xrayp->ray.index = count++;
		xrayp->ray.magic = RT_RAY_MAGIC;
		BU_LIST_APPEND(&rays->l,&xrayp->l);
	    }
	}
    }
    return count;
}

/*
 *  Test driver.
 */

#if 0
main()
{
    FILE	*fp = fopen("bundle.pl", "wb");
    int	rays_per_ring=5;
    int	nring=3;
    fastf_t bundle_radius=1000.0;
    int	i;
    vect_t avec, bvec;
    struct xray *rp;
    vect_t dir;


    VSET( dir, 0, 0, -1 );
    /* create orthogonal rays for basis of bundle */
    bn_vec_ortho( avec, dir );
    VCROSS( bvec, dir, avec );
    VUNITIZE( bvec );

    rp = (struct xray *)bu_calloc( sizeof( struct xray ),
				   (rays_per_ring * nring) + 1,
				   "ray bundle" );
    VSET( rp[0].r_pt, 0, 0, 2000);
    VMOVE( rp[0].r_dir, dir );
    rt_raybundle_maker( rp, bundle_radius, avec, bvec, rays_per_ring, nring );


    for ( i=0; i <= rays_per_ring * nring; i++ )  {
	point_t	tip;
	VJOIN1( tip, rp[i].r_pt, 3500, rp[i].r_dir );
	pdv_3line( fp, rp[i].r_pt, tip );
    }
    fclose(fp);
}
#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
