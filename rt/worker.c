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
static const char RCSworker[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <math.h>
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "./ext.h"
#include "rtprivate.h"

int		per_processor_chunk = 0;	/* how many pixels to do at once */

fastf_t		gift_grid_rounding = 0;		/* set to 25.4 for inches */

point_t		viewbase_model;	/* model-space location of viewplane corner */

int			fullfloat_mode = 0;
int			reproject_mode = 0;
struct floatpixel	*curr_float_frame;	/* buffer of full frame */
struct floatpixel	*prev_float_frame;
int		reproj_cur;	/* number of pixels reprojected this frame */
int		reproj_max;	/* out of total number of pixels */
/* XXX should record width&height, in case size changes on-the-fly */

/* Local communication with worker() */
int cur_pixel;			/* current pixel number, 0..last_pixel */
int last_pixel;			/* last pixel number */

extern int		query_x;
extern int		query_y;
extern int		Query_one_pixel;
extern int		query_rdebug;
extern int		query_debug;

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
grid_setup(void)
{
	vect_t temp;
	mat_t toEye;

	if( viewsize <= 0.0 )
		rt_bomb("viewsize <= 0");
	/* model2view takes us to eye_model location & orientation */
	MAT_IDN( toEye );
	MAT_DELTAS_VEC_NEG( toEye, eye_model );
	Viewrotscale[15] = 0.5*viewsize;	/* Viewscale */
	bn_mat_mul( model2view, Viewrotscale, toEye );
	bn_mat_inv( view2model, model2view );

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
		MAT_COPY( model2hv, Viewrotscale );
		model2hv[15] = gift_grid_rounding;
		bn_mat_inv( hv2model, model2hv );

		VSET( v_ll, -1, -1, 0 );
		MAT4X3PNT( m_ll, view2model, v_ll );
		MAT4X3PNT( hv_ll, model2hv, m_ll );
		VSET( hv_wanted, floor(hv_ll[X]), floor(hv_ll[Y]),floor(hv_ll[Z]) );
		VSUB2( hv_delta, hv_ll, hv_wanted );

		MAT4X3PNT( m_delta, hv2model, hv_delta );
		VSUB2( eye_model, eye_model, m_delta );
		MAT_DELTAS_VEC_NEG( toEye, eye_model );
		bn_mat_mul( model2view, Viewrotscale, toEye );
		bn_mat_inv( view2model, model2view );
	}

	/* Create basis vectors dx and dy for emanation plane (grid) */
	VSET( temp, 1, 0, 0 );
	MAT3X3VEC( dx_unit, view2model, temp );	/* rotate only */
	VSCALE( dx_model, dx_unit, cell_width );

	VSET( temp, 0, 1, 0 );
	MAT3X3VEC( dy_unit, view2model, temp );	/* rotate only */
	VSCALE( dy_model, dy_unit, cell_height );

	if( stereo )  {
		/* Move left 2.5 inches (63.5mm) */
		VSET( temp, -63.5*2.0/viewsize, 0, 0 );
		bu_log("red eye: moving %f relative screen (left)\n", temp[X]);
		MAT4X3VEC( left_eye_delta, view2model, temp );
		VPRINT("left_eye_delta", left_eye_delta);
	}

	/* "Lower left" corner of viewing plane */
	if( rt_perspective > 0.0 )  {
		fastf_t	zoomout;
		zoomout = 1.0 / tan( bn_degtorad * rt_perspective / 2.0 );
		VSET( temp, -1, -1/aspect, -zoomout );	/* viewing plane */
		/*
		 * divergence is perspective angle divided by the number
		 * of pixels in that angle. Extra factor of 0.5 is because
		 * perspective is a full angle while divergence is the tangent
		 * (slope) of a half angle.
		 */
		ap.a_diverge = tan( bn_degtorad * rt_perspective * 0.5 / width );
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

		ang = curframe * frame_delta_t * bn_twopi / 10;	/* 10 sec period */
		dx = cos(ang) * 0.5;	/* +/- 1/4 pixel width in amplitude */
		dy = sin(ang) * 0.5;
		VJOIN2( viewbase_model, viewbase_model,
			dx, dx_model,
			dy, dy_model );
	}

	if( cell_width <= 0 || cell_width >= INFINITY ||
	    cell_height <= 0 || cell_height >= INFINITY )  {
		bu_log("grid_setup: cell size ERROR (%g, %g) mm\n",
			cell_width, cell_height );
	    	rt_bomb("cell size");
	}
	if( width <= 0 || height <= 0 )  {
		bu_log("grid_setup: ERROR bad image size (%d, %d)\n",
			width, height );
		rt_bomb("bad size");
	}
}

/*
 *			D O _ R U N
 *
 *  Compute a run of pixels, in parallel if the hardware permits it.
 *
 *  For a general-purpose version, see LIBRT rt_shoot_many_rays()
 */
void do_run( int a, int b )
{
	int		cpu;

	cur_pixel = a;
	last_pixel = b;

	if( !rt_g.rtg_parallel )  {
		/*
		 * SERIAL case -- one CPU does all the work.
		 */
		npsw = 1;
		worker(0, NULL);
	} else {
		/*
		 *  Parallel case.
		 */
		bu_parallel( worker, npsw, NULL );
	}

	/* Tally up the statistics */
	for( cpu=0; cpu < npsw; cpu++ )  {
		if( resource[cpu].re_magic != RESOURCE_MAGIC )  {
			bu_log("ERROR: CPU %d resources corrupted, statistics bad\n", cpu);
			continue;
		}
		rt_add_res_stats( ap.a_rt_i, &resource[cpu] );
	}
	return;
}

#define CRT_BLEND(v)	(0.26*(v)[X] + 0.66*(v)[Y] + 0.08*(v)[Z])
#define NTSC_BLEND(v)	(0.30*(v)[X] + 0.59*(v)[Y] + 0.11*(v)[Z])
int	stop_worker = 0;

/*
 * For certain hypersample values there is a particular advantage to subdividing
 * the pixel and shooting a ray in each sub-pixel.  This structure keeps track of
 * those patterns
 */
struct jitter_pattern {
	int   num_samples;/* number of samples, or coordinate pairs in coords[] */
	float rand_scale[2]; /* amount to scale bn_rand_half value */
	float coords[32]; /* center of each sub-pixel */
};

static struct jitter_pattern pt_pats[] = {

	{4, {0.5, 0.5}, 	/* -H 3 */
	 { 0.25, 0.25,
	   0.25, 0.75,
	   0.75, 0.25,
	   0.75, 0.75 } },

	{5, {0.4, 0.4}, 	/* -H 4 */
	 { 0.2, 0.2,
	   0.2, 0.8,
	   0.8, 0.2,
	   0.8, 0.8,
	   0.5, 0.5} },

	{9, {0.3333, 0.3333}, /* -H 8 */
	 { 0.17, 0.17,  0.17, 0.5,  0.17, 0.82,
	   0.5, 0.17,    0.5, 0.5,   0.5, 0.82,
	   0.82, 0.17,  0.82, 0.5,  0.82, 0.82 } },

	{16, {0.25, 0.25}, 	/* -H 15 */
	 { 0.125, 0.125,  0.125, 0.375, 0.125, 0.625, 0.125, 0.875,
	   0.375, 0.125,  0.375, 0.375, 0.375, 0.625, 0.375, 0.875,
	   0.625, 0.125,  0.625, 0.375, 0.625, 0.625, 0.625, 0.875,
	   0.875, 0.125,  0.875, 0.375, 0.875, 0.625, 0.875, 0.875} },

	{ 0, {0.0, 0.0}, {0.0} } /* must be here to stop search */
};

/***********************************************************************
 *
 *  compute_point
 *
 * Compute the origin for this ray, based upon the number of samples 
 * per pixel and the number of the current sample.  For certain 
 * ray-counts, it is highly advantageous to subdivide the pixel and 
 * fire each ray in a specific sub-section of the pixel.
 */
static void 
jitter_start_pt(vect_t point, struct application *a, int samplenum, int pat_num)
{
	FAST fastf_t dx, dy;

	if (pat_num >= 0) {
		dx = a->a_x + pt_pats[pat_num].coords[samplenum*2] +
			(bn_rand_half(a->a_resource->re_randptr) * 
			 pt_pats[pat_num].rand_scale[X] );
	
		dy = a->a_y + pt_pats[pat_num].coords[samplenum*2 + 1] +
			(bn_rand_half(a->a_resource->re_randptr) * 
			 pt_pats[pat_num].rand_scale[Y] );
	} else {
		dx = a->a_x + bn_rand_half(a->a_resource->re_randptr);
		dy = a->a_y + bn_rand_half(a->a_resource->re_randptr);
	}
	VJOIN2( point, viewbase_model, dx, dx_model, dy, dy_model );
}

void do_pixel(int cpu,
	      int pat_num,
	      int pixelnum)
{
	LOCAL struct application a;
	LOCAL struct pixel_ext pe;
	LOCAL vect_t point;		/* Ref point on eye or view plane */
	LOCAL vect_t colorsum;
	int	samplenum;


	/* Obtain fresh copy of global application struct */
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
				return;
		}
		a.a_x <<= (incr_nlevel-incr_level);
		a.a_y <<= (incr_nlevel-incr_level);
	} else {
		a.a_x = pixelnum%width;
		a.a_y = pixelnum/width;
	}

	if (Query_one_pixel) {
		if (a.a_x == query_x && a.a_y == query_y) {
			rdebug = query_rdebug;
			rt_g.debug = query_debug;
		} else {
			rt_g.debug = rdebug = 0;
		}
	}

	if( sub_grid_mode )  {
		if( a.a_x < sub_xmin || a.a_x > sub_xmax )
			return;
		if( a.a_y < sub_ymin || a.a_y > sub_ymax )
			return;
	}
	if( fullfloat_mode )  {
		register struct floatpixel	*fp;
		fp = &curr_float_frame[a.a_y*width + a.a_x];
		if( fp->ff_frame >= 0 )  {
			return;	/* pixel was reprojected */
		}
	}

	VSETALL( colorsum, 0 );

	for( samplenum=0; samplenum<=hypersample; samplenum++ )  {
		if( jitter & JITTER_CELL ) {
			jitter_start_pt(point, &a, samplenum, pat_num);
		}  else  {
			VJOIN2( point, viewbase_model,
				a.a_x, dx_model,
				a.a_y, dy_model );
		}
		if (a.a_rt_i->rti_prismtrace) {
			/* compute the four corners */
			pe.magic = PIXEL_EXT_MAGIC;

			VJOIN2(pe.corner[0].r_pt,
			       viewbase_model,
			       a.a_x, dx_model,
			       a.a_y, dy_model );

			VJOIN2(pe.corner[1].r_pt,
			       viewbase_model,
			       (a.a_x+1), dx_model,
			       a.a_y, dy_model );

			VJOIN2(pe.corner[2].r_pt,
			       viewbase_model,
			       (a.a_x+1), dx_model,
			       (a.a_y+1), dy_model );

			VJOIN2(pe.corner[3].r_pt,
			       viewbase_model,
			       a.a_x, dx_model,
			       (a.a_y+1), dy_model );

			a.a_pixelext = &pe;
		} else {
			a.a_pixelext=(struct pixel_ext *)NULL;
		}

		if( rt_perspective > 0.0 )  {
			VSUB2( a.a_ray.r_dir,
			       point, eye_model );
			VUNITIZE( a.a_ray.r_dir );
			VMOVE( a.a_ray.r_pt, eye_model );
			if (a.a_rt_i->rti_prismtrace) {
				VSUB2(pe.corner[0].r_dir,
				      pe.corner[0].r_pt,
				      eye_model);
				VSUB2(pe.corner[1].r_dir,
				      pe.corner[1].r_pt,
				      eye_model);
				VSUB2(pe.corner[2].r_dir,
				      pe.corner[2].r_pt,
				      eye_model);
				VSUB2(pe.corner[3].r_dir,
				      pe.corner[3].r_pt,
				      eye_model);
			}
		} else {
			VMOVE( a.a_ray.r_pt, point );
			VMOVE( a.a_ray.r_dir, ap.a_ray.r_dir );

			if (a.a_rt_i->rti_prismtrace) {
				VMOVE(pe.corner[0].r_dir,
				      a.a_ray.r_dir);
				VMOVE(pe.corner[1].r_dir,
				      a.a_ray.r_dir);
				VMOVE(pe.corner[2].r_dir,
				      a.a_ray.r_dir);
				VMOVE(pe.corner[3].r_dir,
				      a.a_ray.r_dir);
			}
		}
		if( report_progress )  {
			report_progress = 0;
			bu_log("\tframe %d, xy=%d,%d on cpu %d, samp=%d\n", curframe, a.a_x, a.a_y, cpu, samplenum );
		}

		a.a_level = 0;		/* recursion level */
		a.a_purpose = "main ray";
		(void)rt_shootray( &a );

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
			(void)rt_shootray( &a );

			left = CRT_BLEND(a.a_color);
			VSET( a.a_color, left, 0, right );
		}
		VADD2( colorsum, colorsum, a.a_color );
	} /* for samplenum <= hypersample */
	if( hypersample )  {
		FAST fastf_t f;
		f = 1.0 / (hypersample+1);
		VSCALE( a.a_color, colorsum, f );
	}
	view_pixel( &a );
	if( a.a_x == width-1 )
		view_eol( &a );		/* End of scan line */
}

/*
 *  			W O R K E R
 *  
 *  Compute some pixels, and store them.
 *  A "self-dispatching" parallel algorithm.
 *  Executes until there is no more work to be done, or is told to stop.
 *
 *  In order to reduce the traffic through the res_worker critical section,
 *  a multiple pixel block may be removed from the work queue at once.
 *
 *  For a general-purpose version, see LIBRT rt_shoot_many_rays()
 */
void
worker(int cpu, genptr_t arg)
{
	int	pixel_start;
	int	pixelnum;
	int	pat_num = -1;

	/* The more CPUs at work, the bigger the bites we take */
	if( per_processor_chunk <= 0 )  per_processor_chunk = npsw;

	if( cpu >= MAX_PSW )  {
		bu_log("rt/worker() cpu %d > MAX_PSW %d, array overrun\n", cpu, MAX_PSW);
		rt_bomb("rt/worker() cpu > MAX_PSW, array overrun\n");
	}
	RT_CK_RESOURCE( &resource[cpu] );

	pat_num = -1;
	if (hypersample) {
		int i, ray_samples;

		ray_samples = hypersample + 1;
		for (i=0 ; pt_pats[i].num_samples != 0 ; i++) {
			if (pt_pats[i].num_samples == ray_samples) {
				pat_num = i;
				goto pat_found;
			}
		}
	}
 pat_found:

	if (transpose_grid) {
	  int     tmp;

	  /* switch cur_pixel and last_pixel */
	  tmp = cur_pixel;
	  cur_pixel = last_pixel;
	  last_pixel = tmp;

	  while (1)  {
	    if (stop_worker)
	      return;

	    bu_semaphore_acquire(RT_SEM_WORKER);
	    pixel_start = cur_pixel;
	    cur_pixel -= per_processor_chunk;
	    bu_semaphore_release(RT_SEM_WORKER);

	    for (pixelnum = pixel_start; pixelnum > pixel_start-per_processor_chunk; pixelnum--) {
	      if (pixelnum < last_pixel)
		return;

	      do_pixel(cpu, pat_num, pixelnum);
	    }
	  }
	} else {
	  while (1) {
	    if (stop_worker)
	      return;

	    bu_semaphore_acquire(RT_SEM_WORKER);
	    pixel_start = cur_pixel;
	    cur_pixel += per_processor_chunk;
	    bu_semaphore_release(RT_SEM_WORKER);

	    for (pixelnum = pixel_start; pixelnum < pixel_start+per_processor_chunk; pixelnum++) {

	      if (pixelnum > last_pixel)
		return;

	      do_pixel(cpu, pat_num, pixelnum);
	    }
	  }
	}
}
