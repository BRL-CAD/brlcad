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
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "../rt/mathtab.h"

/***** view.c variables imported from rt.c *****/
extern mat_t view2model;
extern mat_t model2view;

/***** worker.c variables imported from rt.c *****/
extern void worker();
extern struct application ap;
extern int	stereo;		/* stereo viewing */
extern vect_t left_eye_delta;
extern int	hypersample;	/* number of extra rays to fire */
extern int	perspective;	/* perspective view -vs- parallel view */
extern vect_t	dx_model;	/* view delta-X as model-space vector */
extern vect_t	dy_model;	/* view delta-Y as model-space vector */
extern point_t	eye_model;	/* model-space location of eye */
extern point_t	viewbase_model;	/* model-space location of viewplane corner */
extern int npts;	/* # of points to shoot: x,y */
extern mat_t Viewrotscale;
extern fastf_t viewsize;
extern fastf_t zoomout;
extern int npsw;

#ifdef RTSRV
extern char scanbuf[];
#else
#ifdef PARALLEL
extern char *scanbuf;		/*** Output buffering, for parallelism */
#endif
#endif

/* Local communication with worker() */
HIDDEN int cur_pixel;		/* current pixel number, 0..last_pixel */
HIDDEN int last_pixel;		/* last pixel number */
HIDDEN int nworkers;		/* number of workers now running */

#ifdef cray
struct taskcontrol {
	int	tsk_len;
	int	tsk_id;
	int	tsk_value;
} taskcontrol[MAX_PSW];
#endif

/*
 *			G R I D _ S E T U P
 */
void
grid_setup()
{
	vect_t temp;
	mat_t toEye;

	/* model2view takes us to eye_model location & orientation */
	mat_idn( toEye );
	toEye[MDX] = -eye_model[X];
	toEye[MDY] = -eye_model[Y];
	toEye[MDZ] = -eye_model[Z];
	Viewrotscale[15] = 0.5*viewsize;	/* Viewscale */
	mat_mul( model2view, Viewrotscale, toEye );
	mat_inv( view2model, model2view );

	/* Chop -1.0..+1.0 range into npts parts */
	VSET( temp, 2.0/npts, 0, 0 );
	MAT4X3VEC( dx_model, view2model, temp );
	VSET( temp, 0, 2.0/npts, 0 );
	MAT4X3VEC( dy_model, view2model, temp );
	if( stereo )  {
		/* Move left 2.5 inches (63.5mm) */
		VSET( temp, 2.0*(-63.5/viewsize), 0, 0 );
		rt_log("red eye: moving %f relative screen (left)\n", temp[X]);
		MAT4X3VEC( left_eye_delta, view2model, temp );
		VPRINT("left_eye_delta", left_eye_delta);
	}

	/* "Lower left" corner of viewing plane */
	if( perspective )  {
		VSET( temp, -1, -1, -zoomout );	/* viewing plane */
		/*
		 * Divergance is (0.5 * viewsize / npts) mm at
		 * a ray distance of (viewsize * zoomout) mm.
		 */
		ap.a_diverge = (0.5 / npts) / zoomout;
		ap.a_rbeam = 0;
	}  else  {
		VSET( temp, 0, 0, -1 );
		MAT4X3VEC( ap.a_ray.r_dir, view2model, temp );
		VUNITIZE( ap.a_ray.r_dir );

		VSET( temp, -1, -1, 0 );	/* eye plane */
		ap.a_rbeam = 0.5 * viewsize / npts;
		ap.a_diverge = 0;
	}
	MAT4X3PNT( viewbase_model, view2model, temp );
}

/*
 *			D O _ R U N
 *
 *  Compute a run of pixels, in parallel if the hardware permits it.
 *
 *  Don't use registers in this function.  At least on the Alliant,
 *  register context is NOT preserved when exiting the parallel mode,
 *  because the serial portion resumes on some arbitrary processor,
 *  not necessarily the one that serial execution started on.
 *  The registers are not shared.
 */
do_run( a, b )
{
	int x;

	cur_pixel = a;
	last_pixel = b;

#ifdef PARALLEL
	/*
	 *  Parallel case.  This is different for each system.
	 *  In the case of the HEP, the workers were started in
	 *  the mainline;  for other systems, the workers are
	 *  started and terminated here.
	 */
	nworkers = 0;
#ifdef cray
	/* Create any extra worker tasks */

	for( x=0; x<npsw; x++ ) {
		taskcontrol[x].tsk_len = 3;
		taskcontrol[x].tsk_value = x;
		TSKSTART( &taskcontrol[x], worker );
	}
	/* Wait for them to finish */
	for( x=0; x<npsw; x++ )  {
		TSKWAIT( &taskcontrol[x] );
	}
#endif
#ifdef alliant
	{
		asm("	movl		_npsw,d0");
		asm("	subql		#1,d0");
		asm("	cstart		d0");
		asm("super_loop:");
		worker();
		asm("	crepeat		super_loop");
	}
#endif
	/* Ensure that all the workers are REALLY dead */
	x = 0;
	while( nworkers > 0 )  x++;
	if( x > 0 )  rt_log("do_run(%d,%d): termination took %d extra loops\n", a, b, x);
#else
	/*
	 * SERIAL case -- one CPU does all the work.
	 */
	worker();
#endif
}

#define CRT_BLEND(v)	(0.26*(v)[X] + 0.66*(v)[Y] + 0.08*(v)[Z])
#define NTSC_BLEND(v)	(0.30*(v)[X] + 0.59*(v)[Y] + 0.11*(v)[Z])

/*
 *  			W O R K E R
 *  
 *  Compute one pixel, and store it.
 */
void
worker()
{
	LOCAL struct application a;
	LOCAL vect_t point;		/* Ref point on eye or view plane */
	LOCAL vect_t colorsum;
	register int com;

	RES_ACQUIRE( &rt_g.res_worker );
	nworkers++;
	RES_RELEASE( &rt_g.res_worker );

	a.a_onehit = 1;
	while(1)  {
		RES_ACQUIRE( &rt_g.res_worker );
		com = cur_pixel++;
		RES_RELEASE( &rt_g.res_worker );

		if( com > last_pixel )
			break;
		/* Note: ap.... may not be valid until first time here */
		a.a_x = com%npts;
		a.a_y = com/npts;
		a.a_hit = ap.a_hit;
		a.a_miss = ap.a_miss;
		a.a_rt_i = ap.a_rt_i;
		a.a_rbeam = ap.a_rbeam;
		a.a_diverge = ap.a_diverge;
		VSETALL( colorsum, 0 );
		for( com=0; com<=hypersample; com++ )  {
			if( hypersample )  {
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
			if( perspective )  {
				VSUB2( a.a_ray.r_dir,
					point, eye_model );
				VUNITIZE( a.a_ray.r_dir );
				VMOVE( a.a_ray.r_pt, eye_model );
			} else {
				VMOVE( a.a_ray.r_pt, point );
			 	VMOVE( a.a_ray.r_dir, ap.a_ray.r_dir );
			}
			a.a_level = 0;		/* recursion level */
			rt_shootray( &a );

			if( stereo )  {
				FAST fastf_t right,left;

				right = CRT_BLEND(a.a_color);

				VADD2(  point, point,
					left_eye_delta );
				if( perspective )  {
					VSUB2( a.a_ray.r_dir,
						point, eye_model );
					VUNITIZE( a.a_ray.r_dir );
					VMOVE( a.a_ray.r_pt, eye_model );
				} else {
					VMOVE( a.a_ray.r_pt, point );
				}
				a.a_level = 0;		/* recursion level */
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
#if !defined(PARALLEL) && !defined(RTSRV)
		view_pixel( &a );
		if( a.a_x == npts-1 )
			view_eol( &a );		/* End of scan line */
#endif
#if defined(PARALLEL) || defined(RTSRV)
		{
			register char *pixelp;
			register int r,g,b;

			/* .pix files go bottom to top */
#ifdef RTSRV
			/* Here, the buffer is only one line long */
			pixelp = scanbuf+a.a_x*3;
#else
			pixelp = scanbuf+((a.a_y*npts)+a.a_x)*3;
#endif
			r = a.a_color[0]*255.+rand_half();
			g = a.a_color[1]*255.+rand_half();
			b = a.a_color[2]*255.+rand_half();
			/* Truncate glints, etc */
			if( r > 255 )  r=255;
			if( g > 255 )  g=255;
			if( b > 255 )  b=255;
			if( r<0 || g<0 || b<0 )  {
				rt_log("Negative RGB %d,%d,%d\n", r, g, b );
				r = 0x80;
				g = 0xFF;
				b = 0x80;
			}
			*pixelp++ = r ;
			*pixelp++ = g ;
			*pixelp++ = b ;
		}
#endif
	}
	RES_ACQUIRE( &rt_g.res_worker );
	nworkers--;
	RES_RELEASE( &rt_g.res_worker );
}
