/*
 *			W O R K E R . C
 *
 *  Routines to handle initialization of the grid,
 *  and dispatch of the first rays from the eye.
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1985 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSworker[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "./ext.h"
#include "./mathtab.h"
#include "./rdebug.h"

int		per_processor_chunk = 0;	/* how many pixels to do at once */

fastf_t		gift_grid_rounding = 0;		/* set to 25.4 for inches */

point_t		viewbase_model;	/* model-space location of viewplane corner */

/* Local communication with worker() */
HIDDEN int cur_pixel;		/* current pixel number, 0..last_pixel */
HIDDEN int last_pixel;		/* last pixel number */
HIDDEN int nworkers_started;	/* number of workers started */
HIDDEN int nworkers_finished;	/* number of workers properly finished */

/*
 *			G R I D _ S E T U P
 *
 *  In theory, the grid can be specified by providing any two of
 *  these sets of parameters:
 *	number of pixels (width, height)
 *	viewsize (in model units, mm)
 *	number of grid cells (cell_width, cell_height)
 *  however, for now, it is required that the view size always be specified,
 *  and one or the other parameter be provided.
 */
void
grid_setup()
{
	vect_t temp;
	mat_t toEye;

	if( viewsize <= 0.0 )
		rt_bomb("viewsize <= 0");
	/* model2view takes us to eye_model location & orientation */
	mat_idn( toEye );
	MAT_DELTAS_VEC_NEG( toEye, eye_model );
	Viewrotscale[15] = 0.5*viewsize;	/* Viewscale */
	mat_mul( model2view, Viewrotscale, toEye );
	mat_inv( view2model, model2view );

	/* Determine grid cell size and number of pixels */
	if( cell_newsize ) {
		if( cell_width <= 0.0 ) cell_width = cell_height;
		if( cell_height <= 0.0 ) cell_height = cell_width;
		width = (viewsize / cell_width) + 0.99;
		height = (viewsize / (cell_height*aspect)) + 0.99;
		cell_newsize = 0;
	} else {
		/* Chop -1.0..+1.0 range into parts */
		cell_width = viewsize / width;
		cell_height = viewsize / (height*aspect);
	}

	/*
	 *  Optional GIFT compatabilty, mostly for RTG3.
	 *  Round coordinates of lower left corner to fall on integer-
	 *  valued coordinates, in "gift_grid_rounding" units.
	 */
	if( gift_grid_rounding > 0.0 )  {
		point_t		v_ll;		/* view, lower left */
		point_t		m_ll;		/* model, lower left */
		point_t		hv_ll;		/* hv, lower left*/
		point_t		hv_wanted;
		vect_t		hv_delta;
		vect_t		m_delta;
		mat_t		model2hv;
		mat_t		hv2model;

		/* Build model2hv matrix, including mm2inches conversion */
		mat_copy( model2hv, Viewrotscale );
		model2hv[15] = gift_grid_rounding;
		mat_inv( hv2model, model2hv );

		VSET( v_ll, -1, -1, 0 );
		MAT4X3PNT( m_ll, view2model, v_ll );
		MAT4X3PNT( hv_ll, model2hv, m_ll );
		VSET( hv_wanted, floor(hv_ll[X]), floor(hv_ll[Y]),floor(hv_ll[Z]) );
		VSUB2( hv_delta, hv_ll, hv_wanted );

		MAT4X3PNT( m_delta, hv2model, hv_delta );
		VSUB2( eye_model, eye_model, m_delta );
		MAT_DELTAS_VEC_NEG( toEye, eye_model );
		mat_mul( model2view, Viewrotscale, toEye );
		mat_inv( view2model, model2view );
	}

	/* Create basis vectors dx and dy for emanation plane (grid) */
	VSET( temp, 1, 0, 0 );
	MAT3X3VEC( dx_model, view2model, temp );	/* rotate only */
	VSCALE( dx_model, dx_model, cell_width );

	VSET( temp, 0, 1, 0 );
	MAT3X3VEC( dy_model, view2model, temp );	/* rotate only */
	VSCALE( dy_model, dy_model, cell_height );

	if( stereo )  {
		/* Move left 2.5 inches (63.5mm) */
		VSET( temp, -63.5*2.0/viewsize, 0, 0 );
		rt_log("red eye: moving %f relative screen (left)\n", temp[X]);
		MAT4X3VEC( left_eye_delta, view2model, temp );
		VPRINT("left_eye_delta", left_eye_delta);
	}

	/* "Lower left" corner of viewing plane */
	if( rt_perspective > 0.0 )  {
		fastf_t	zoomout;
		zoomout = 1.0 / tan( bn_degtorad * rt_perspective / 2.0 );
		VSET( temp, -1, -1/aspect, -zoomout );	/* viewing plane */
		/*
		 * Divergance is (0.5 * viewsize / width) mm at
		 * a ray distance of (viewsize * zoomout) mm.
		 */
		ap.a_diverge = (0.5 / width) / zoomout;
		ap.a_rbeam = 0;
	}  else  {
		/* all rays go this direction */
		VSET( temp, 0, 0, -1 );
		MAT4X3VEC( ap.a_ray.r_dir, view2model, temp );
		VUNITIZE( ap.a_ray.r_dir );

		VSET( temp, -1, -1/aspect, 0 );	/* eye plane */
		ap.a_rbeam = 0.5 * viewsize / width;
		ap.a_diverge = 0;
	}
	if( NEAR_ZERO(ap.a_rbeam, SMALL) && NEAR_ZERO(ap.a_diverge, SMALL) )
		rt_bomb("zero-radius beam");
	MAT4X3PNT( viewbase_model, view2model, temp );

	if( jitter & JITTER_FRAME )  {
		/* Move the frame in a smooth circular rotation in the plane */
		fastf_t		ang;	/* radians */
		fastf_t		dx, dy;

		ang = curframe * frame_delta_t * rt_twopi / 10;	/* 10 sec period */
		dx = cos(ang) * 0.5;	/* +/- 1/4 pixel width in amplitude */
		dy = sin(ang) * 0.5;
		VJOIN2( viewbase_model, viewbase_model,
			dx, dx_model,
			dy, dy_model );
	}

	if( cell_width <= 0 || cell_width >= INFINITY ||
	    cell_height <= 0 || cell_height >= INFINITY )  {
		rt_log("grid_setup: cell size ERROR (%g, %g) mm\n",
			cell_width, cell_height );
	    	rt_bomb("cell size");
	}
	if( width <= 0 || height <= 0 )  {
		rt_log("grid_setup: ERROR bad image size (%d, %d)\n",
			width, height );
		rt_bomb("bad size");
	}
}

/*
 *			D O _ R U N
 *
 *  Compute a run of pixels, in parallel if the hardware permits it.
 *
 */
void
do_run( a, b )
{
	int		cpu;

	cur_pixel = a;
	last_pixel = b;

	nworkers_started = 0;
	nworkers_finished = 0;
	if( !rt_g.rtg_parallel )  {
		/*
		 * SERIAL case -- one CPU does all the work.
		 */
		npsw = 1;
		worker();
	} else {
		/*
		 *  Parallel case.
		 */
		rt_parallel( worker, npsw );
	}
	/*
	 *  Ensure that all the workers are REALLY finished.
	 *  On some systems, if threads core dump, the rest of
	 *  the gang keeps going, so this can actually happen (sigh).
	 */
	if( nworkers_finished != npsw )  {
		rt_log("\n***ERROR: %d workers did not finish!\n\n",
			npsw - nworkers_finished);
	}
	if( nworkers_started != npsw )  {
		rt_log("\nNOTICE:  only %d workers started, expected %d\n",
			nworkers_started, npsw );
	}

	/* Tally up the statistics */
	for( cpu=0; cpu < npsw; cpu++ )  {
		if( resource[cpu].re_magic != RESOURCE_MAGIC )  {
			rt_log("ERROR: CPU %d resources corrupted, statistics bad\n", cpu);
			continue;
		}
		rt_add_res_stats( ap.a_rt_i, &resource[cpu] );
	}
	return;
}

#define CRT_BLEND(v)	(0.26*(v)[X] + 0.66*(v)[Y] + 0.08*(v)[Z])
#define NTSC_BLEND(v)	(0.30*(v)[X] + 0.59*(v)[Y] + 0.11*(v)[Z])

/*
 *  			W O R K E R
 *  
 *  Compute some pixels, and store them.
 *  A "self-dispatching" parallel algorithm.
 *
 *  In order to reduce the traffic through the res_worker critical section,
 *  a multiple pixel block may be removed from the work queue at once.
 */
void
worker()
{
	LOCAL struct application a;
	LOCAL vect_t point;		/* Ref point on eye or view plane */
	LOCAL vect_t colorsum;
	int	pixel_start;
	int	pixelnum;
	int	samplenum;
	int	cpu;			/* our CPU (PSW) number */

	RES_ACQUIRE( &rt_g.res_worker );
	cpu = nworkers_started++;
	RES_RELEASE( &rt_g.res_worker );

	/* The more CPUs at work, the bigger the bites we take */
	if( per_processor_chunk <= 0 )  per_processor_chunk = npsw;

	if( cpu >= MAX_PSW )  rt_bomb("rt/worker() cpu > MAXPSW, array overrun\n");
	resource[cpu].re_cpu = cpu;
	resource[cpu].re_magic = RESOURCE_MAGIC;
	rand_init( resource[cpu].re_randptr, cpu );

	while(1)  {
		RES_ACQUIRE( &rt_g.res_worker );
		pixel_start = cur_pixel;
		cur_pixel += per_processor_chunk;
		RES_RELEASE( &rt_g.res_worker );

		for( pixelnum = pixel_start; pixelnum < pixel_start+per_processor_chunk; pixelnum++ )  {

			if( pixelnum > last_pixel )
				goto out;

			/* Obtain fresh copy of application struct */
			a = ap;				/* struct copy */
			a.a_resource = &resource[cpu];

			if( incr_mode )  {
				register int i = 1<<incr_level;
				a.a_x = pixelnum%i;
				a.a_y = pixelnum/i;
				if( incr_level != 0 )  {
					/* See if already done last pass */
					if( ((a.a_x & 1) == 0 ) &&
					    ((a.a_y & 1) == 0 ) )
						continue;
				}
				a.a_x <<= (incr_nlevel-incr_level);
				a.a_y <<= (incr_nlevel-incr_level);
			} else {
				a.a_x = pixelnum%width;
				a.a_y = pixelnum/width;
			}
			VSETALL( colorsum, 0 );
			for( samplenum=0; samplenum<=hypersample; samplenum++ )  {
				if( jitter & JITTER_CELL )  {
					FAST fastf_t dx, dy;
					dx = a.a_x + rand_half(a.a_resource->re_randptr);
					dy = a.a_y + rand_half(a.a_resource->re_randptr);
					VJOIN2( point, viewbase_model,
						dx, dx_model, dy, dy_model );
				}  else  {
					VJOIN2( point, viewbase_model,
						a.a_x, dx_model,
						a.a_y, dy_model );
				}
				if( rt_perspective > 0.0 )  {
					VSUB2( a.a_ray.r_dir,
						point, eye_model );
					VUNITIZE( a.a_ray.r_dir );
					VMOVE( a.a_ray.r_pt, eye_model );
				} else {
					VMOVE( a.a_ray.r_pt, point );
				 	VMOVE( a.a_ray.r_dir, ap.a_ray.r_dir );
				}
				a.a_level = 0;		/* recursion level */
				a.a_purpose = "main ray";
				rt_shootray( &a );

				if( stereo )  {
					FAST fastf_t right,left;

					right = CRT_BLEND(a.a_color);

					VSUB2(  point, point,
						left_eye_delta );
					if( rt_perspective > 0.0 )  {
						VSUB2( a.a_ray.r_dir,
							point, eye_model );
						VUNITIZE( a.a_ray.r_dir );
						VADD2( a.a_ray.r_pt, eye_model, left_eye_delta );
					} else {
						VMOVE( a.a_ray.r_pt, point );
					}
					a.a_level = 0;		/* recursion level */
					a.a_purpose = "left eye ray";
					rt_shootray( &a );

					left = CRT_BLEND(a.a_color);
					VSET( a.a_color, left, 0, right );
				}
				VADD2( colorsum, colorsum, a.a_color );
			}
			if( hypersample )  {
				FAST fastf_t f;
				f = 1.0 / (hypersample+1);
				VSCALE( a.a_color, colorsum, f );
			}
			view_pixel( &a );
			if( a.a_x == width-1 )
				view_eol( &a );		/* End of scan line */
		}
	}
out:
	RES_ACQUIRE( &rt_g.res_worker );
	nworkers_finished++;
	RES_RELEASE( &rt_g.res_worker );
}
