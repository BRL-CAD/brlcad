/*
 *			E X A M P L E . C
 *
 *  Author -
 *	Paul R. Stay
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimited.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "nurb.h"

fastf_t grid[10][10][3];
fastf_t ngrid[10][10][3];

main()
{

	fastf_t hscale;
	fastf_t v;
	int i, j;

	hscale = 2.5;

	/* generate a 10 x 10 grid of random height data */

	pl_color(stdout,155, 55, 55);

	for( i = 0; i < 10; i++)
		for( j = 0; j < 10; j++)
		{
			v = hscale * drand48() + 10.0;
			grid[i][j][0] = i;
			grid[i][j][1] = j;
			grid[i][j][2] = v;

			pd_3move( stdout,
			    grid[i][j][0],  grid[i][j][1], grid[i][j][2] - .14);
			pd_3cont( stdout,
			    grid[i][j][0], grid[i][j][1], grid[i][j][2] + .14);

			pd_3move( stdout,
			    grid[i][j][0] - .14, grid[i][j][1], grid[i][j][2]);
			pd_3cont( stdout,
			    grid[i][j][0] + .14, grid[i][j][1], grid[i][j][2] );

			pd_3move( stdout,
			    grid[i][j][0], grid[i][j][1]-.14, grid[i][j][2]);
			pd_3cont( stdout,
			    grid[i][j][0], grid[i][j][1]+.14, grid[i][j][2]);
		}

	interpolate_data();
}

/* Interpoate the data using b-splines */

interpolate_data()
{
	struct face_g_snurb srf;
	struct face_g_snurb *srf2, *srf3;
	struct knot_vector new_kv;

	rt_nurb_sinterp( &srf, 4, grid, 10, 10 );

#if 0
	/* Draw control mesh in blue */
	pl_color( stdout, 145, 145, 255 );
	rt_nurb_plot_snurb( stdout, &srf );
#endif

#if 0
	rt_nurb_reverse_srf( &srf );
	rt_nurb_kvnorm( &srf.u );
	rt_nurb_kvnorm( &srf.v );
#endif

	/* lets take a look at it.  Refine to 100 points in both directions. */
	rt_nurb_kvknot( &new_kv, srf.order[0], 0.0, 1.0, 100);
	srf2 = (struct face_g_snurb *) rt_nurb_s_refine( &srf, 0, &new_kv);
	srf3 = (struct face_g_snurb *) rt_nurb_s_refine( srf2, 1, &new_kv);

	/* Draw refined mesh in yellow */
	pl_color( stdout, 200, 200, 50 );
	rt_nurb_plot_snurb( stdout, srf3 );
}
