/*
 *			M K B U N D L E . C
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Status -
 *	This file is public domain, distribution unlimited.
 */
#ifndef lint
static const char RCSmkbundle[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "bu.h"
#include "bn.h"
#include "raytrace.h"
#include "./debug.h"

/*
 *			R T _ R A Y B U N D L E _ M A K E R
 *
 *  Make a bundle of rays around a main ray, with a circular exterior,
 *  and spiral filling of the interior.
 *  The outer periphery is sampled with rays_per_ring additional rays,
 *  preferably at least 3.
 *
 *  rp[] must be of size (rays_per_ring*nring)+1.
 *  Entry 0 is the main ray, and must be provided by the caller.
 *  The remaining entries will be filled in with extra rays.
 *  The radius of the bundle is given in mm.
 *
 *  rp[0].r_dir must have unit length.
 *
 *  XXX Should we require a and b as inputs, for efficiency?
 */
void
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

	for( ring=0; ring < nring; ring++ )  {
		register int i;

		theta = 0;
		delta = bn_twopi / rays_per_ring;
		fraction = ((double)(ring+1)) / nring;
		theta = delta * fraction;	/* spiral skew */
		radial_scale = radius * fraction;
		for( i=0; i < rays_per_ring; i++ )  {
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
}

/*
 *  Test driver.
 */

#if 0
main()
{
	FILE	*fp = fopen("bundle.pl", "w");
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


	for( i=0; i <= rays_per_ring * nring; i++ )  {
		point_t	tip;
		VJOIN1( tip, rp[i].r_pt, 3500, rp[i].r_dir );
		pdv_3line( fp, rp[i].r_pt, tip );
	}
	fclose(fp);
}
#endif
