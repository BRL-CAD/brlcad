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

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "./mathtab.h"
#include "./rdebug.h"

#ifdef HEP
# include <synch.h>
#endif

/***** view.c variables imported from rt.c *****/
extern mat_t	view2model;
extern mat_t	model2view;

/***** worker.c variables imported from rt.c *****/
extern void	worker();
extern struct application ap;
extern int	stereo;			/* stereo viewing */
extern vect_t	left_eye_delta;
extern int	hypersample;		/* number of extra rays to fire */
extern int	jitter;			/* jitter ray starting positions */
extern fastf_t	rt_perspective;		/* persp (degrees X) 0 => ortho */
extern fastf_t	aspect;			/* view aspect ratio X/Y */
extern vect_t	dx_model;		/* view delta-X as model-space vect */
extern vect_t	dy_model;		/* view delta-Y as model-space vect */
extern point_t	eye_model;		/* model-space location of eye */
extern int	width;			/* # of pixels in X */
extern int	height;			/* # of lines in Y */
extern mat_t	Viewrotscale;
extern fastf_t	viewsize;
extern int	incr_mode;		/* !0 for incremental resolution */
extern int	incr_level;		/* current incremental level */
extern int	incr_nlevel;		/* number of levels */
extern int	npsw;
extern struct resource resource[];

/* Local communication with worker() */
HIDDEN point_t	viewbase_model;	/* model-space location of viewplane corner */
HIDDEN int cur_pixel;		/* current pixel number, 0..last_pixel */
HIDDEN int last_pixel;		/* last pixel number */
HIDDEN int nworkers;		/* number of workers now running */

/*
 *			G R I D _ S E T U P
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
	toEye[MDX] = -eye_model[X];
	toEye[MDY] = -eye_model[Y];
	toEye[MDZ] = -eye_model[Z];
	Viewrotscale[15] = 0.5*viewsize;	/* Viewscale */
	mat_mul( model2view, Viewrotscale, toEye );
	mat_inv( view2model, model2view );

	/* Chop -1.0..+1.0 range into parts */
	VSET( temp, 2.0/width, 0, 0 );
	MAT4X3VEC( dx_model, view2model, temp );
	VSET( temp, 0, 2.0/(height*aspect), 0 );
	MAT4X3VEC( dy_model, view2model, temp );
	if( stereo )  {
		/* Move left 2.5 inches (63.5mm) */
		VSET( temp, -63.5*2.0/viewsize, 0, 0 );
		rt_log("red eye: moving %f relative screen (left)\n", temp[X]);
		MAT4X3VEC( left_eye_delta, view2model, temp );
		VPRINT("left_eye_delta", left_eye_delta);
	}

	/* "Lower left" corner of viewing plane */
	if( rt_perspective )  {
		fastf_t	zoomout;
		extern double mat_degtorad;
		zoomout = 1.0 / tan( mat_degtorad * rt_perspective / 2.0 );
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
	MAT4X3PNT( viewbase_model, view2model, temp );
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
	int	x;

	cur_pixel = a;
	last_pixel = b;

	if( !rt_g.rtg_parallel )  {
		/*
		 * SERIAL case -- one CPU does all the work.
		 */
		worker(0);
		return;
	}

	/*
	 *  Parallel case.
	 */
	nworkers = 0;
	rt_parallel( worker, npsw );

	/* Ensure that all the workers are REALLY dead */
	x = 0;
	while( nworkers > 0 )  x++;
	if( x > 0 )  rt_log("do_run(%d,%d): termination took %d extra loops\n", a, b, x);	
}

#define CRT_BLEND(v)	(0.26*(v)[X] + 0.66*(v)[Y] + 0.08*(v)[Z])
#define NTSC_BLEND(v)	(0.30*(v)[X] + 0.59*(v)[Y] + 0.11*(v)[Z])

/*
 *  			W O R K E R
 *  
 *  Compute one pixel, and store it.
 */
void
worker(cpu)
int cpu;
{
	LOCAL struct application a;
	LOCAL vect_t point;		/* Ref point on eye or view plane */
	LOCAL vect_t colorsum;
	register int com;

	RES_ACQUIRE( &rt_g.res_worker );
	com = nworkers++;
	RES_RELEASE( &rt_g.res_worker );

	resource[cpu].re_cpu = cpu;

	while(1)  {
		RES_ACQUIRE( &rt_g.res_worker );
		com = cur_pixel++;
		RES_RELEASE( &rt_g.res_worker );

		if( com > last_pixel )
			break;
		/* Note: ap.... may not be valid until first time here */
		a = ap;				/* struct copy */
		a.a_resource = &resource[cpu];
		if( incr_mode )  {
			register int i = 1<<incr_level;
			a.a_x = com%i;
			a.a_y = com/i;
			if( incr_level != 0 )  {
				/* See if already done last pass */
				if( ((a.a_x & 1) == 0 ) &&
				    ((a.a_y & 1) == 0 ) )
					continue;
			}
			a.a_x <<= (incr_nlevel-incr_level);
			a.a_y <<= (incr_nlevel-incr_level);
		} else {
			a.a_x = com%width;
			a.a_y = com/width;
		}
		VSETALL( colorsum, 0 );
		for( com=0; com<=hypersample; com++ )  {
			if( jitter )  {
				FAST fastf_t dx, dy;
				dx = a.a_x + rand_half();
				dy = a.a_y + rand_half();
				VJOIN2( point, viewbase_model,
					dx, dx_model, dy, dy_model );
			}  else  {
				VJOIN2( point, viewbase_model,
					a.a_x, dx_model,
					a.a_y, dy_model );
			}
			if( rt_perspective )  {
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
				if( rt_perspective )  {
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
	RES_ACQUIRE( &rt_g.res_worker );
	nworkers--;
	RES_RELEASE( &rt_g.res_worker );
}
