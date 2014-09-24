/*                        W O R K E R . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2014 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file rt/worker.c
 *
 * Routines to handle initialization of the grid, and dispatch of the
 * first rays from the eye.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "bu/log.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "fb.h"		/* Added because RGBpixel is now needed in do_pixel() */

#include "./rtuif.h"
#include "./ext.h"


/* for fork/pipe linux timing hack */
#ifdef USE_FORKED_THREADS
#  include <sys/select.h>
#  include <sys/types.h>
#  ifdef HAVE_SYS_WAIT_H
#    include <sys/wait.h>
#  endif
#endif


#define CRT_BLEND(v)	(0.26*(v)[X] + 0.66*(v)[Y] + 0.08*(v)[Z])
#define NTSC_BLEND(v)	(0.30*(v)[X] + 0.59*(v)[Y] + 0.11*(v)[Z])

extern fastf_t** timeTable_init(int x, int y);
extern int timeTable_input(int x, int y, fastf_t t, fastf_t **timeTable);

extern int query_x;
extern int query_y;
extern int Query_one_pixel;
extern int query_rdebug;
extern int query_debug;

extern unsigned char *pixmap;	/* pixmap for rerendering of black pixels */

int per_processor_chunk = 0;	/* how many pixels to do at once */

fastf_t gift_grid_rounding = 0;	/* set to 25.4 for inches */

point_t viewbase_model;	/* model-space location of viewplane corner */

int fullfloat_mode = 0;
int reproject_mode = 0;
struct floatpixel *curr_float_frame; /* buffer of full frame */
struct floatpixel *prev_float_frame;
int reproj_cur;	/* number of pixels reprojected this frame */
int reproj_max;	/* out of total number of pixels */

/* Local communication with worker() */
int cur_pixel;			/* current pixel number, 0..last_pixel */
int last_pixel;			/* last pixel number */

int stop_worker = 0;

/**
 * For certain hypersample values there is a particular advantage to
 * subdividing the pixel and shooting a ray in each sub-pixel.  This
 * structure keeps track of those patterns
 */
struct jitter_pattern {
    int num_samples;/* number of samples, or coordinate pairs in coords[] */
    double rand_scale[2]; /* amount to scale bn_rand_half value */
    double coords[32]; /* center of each sub-pixel */
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


/**
 * Compute the origin for this ray, based upon the number of samples
 * per pixel and the number of the current sample.  For certain
 * ray-counts, it is highly advantageous to subdivide the pixel and
 * fire each ray in a specific sub-section of the pixel.
 */
static void
jitter_start_pt(vect_t point, struct application *a, int samplenum, int pat_num)
{
    fastf_t dx, dy;

    if (pat_num >= 0) {
	dx = a->a_x + pt_pats[pat_num].coords[samplenum*2] +
	    (bn_rand_half(a->a_resource->re_randptr) *
	     pt_pats[pat_num].rand_scale[X]);

	dy = a->a_y + pt_pats[pat_num].coords[samplenum*2 + 1] +
	    (bn_rand_half(a->a_resource->re_randptr) *
	     pt_pats[pat_num].rand_scale[Y]);
    } else {
	dx = a->a_x + bn_rand_half(a->a_resource->re_randptr);
	dy = a->a_y + bn_rand_half(a->a_resource->re_randptr);
    }
    VJOIN2(point, viewbase_model, dx, dx_model, dy, dy_model);
}


void
do_pixel(int cpu, int pat_num, int pixelnum)
{
    struct application a;
    struct pixel_ext pe;
    vect_t stereo_point;		/* Ref point on eye or view plane */
    vect_t point;		/* Ref point on eye or view plane */
    vect_t colorsum = {(fastf_t)0.0, (fastf_t)0.0, (fastf_t)0.0};
    int samplenum = 0;
    static const double one_over_255 = 1.0 / 255.0;
    const int pindex = (pixelnum * sizeof(RGBpixel));


    if (lightmodel == 8) {
	/* Add timer here to start pixel-time for heat
	 * graph, when asked.
	 */
	rt_prep_timer();
    }

    /* Obtain fresh copy of global application struct */
    a = APP;				/* struct copy */
    a.a_resource = &resource[cpu];

    if (incr_mode) {
	register int i = 1<<incr_level;
	a.a_y = pixelnum/i;
	a.a_x = pixelnum - (a.a_y * i);
	/* a.a_x = pixelnum%i; */
	if (incr_level != 0) {
	    /* See if already done last pass */
	    if (((a.a_x & 1) == 0) &&
		((a.a_y & 1) == 0))
		return;
	}
	a.a_x <<= (incr_nlevel-incr_level);
	a.a_y <<= (incr_nlevel-incr_level);
    } else {
	a.a_y = pixelnum/width;
	a.a_x = pixelnum - (a.a_y * width);
	/* a.a_x = pixelnum%width; */
    }

    if (Query_one_pixel) {
	if (a.a_x == query_x && a.a_y == query_y) {
	    rdebug = query_rdebug;
	    RTG.debug = query_debug;
	} else {
	    RTG.debug = rdebug = 0;
	}
    }

    if (sub_grid_mode) {
	if (a.a_x < sub_xmin || a.a_x > sub_xmax)
	    return;
	if (a.a_y < sub_ymin || a.a_y > sub_ymax)
	    return;
    }
    if (fullfloat_mode) {
	register struct floatpixel *fp;
	fp = &curr_float_frame[a.a_y*width + a.a_x];
	if (fp->ff_frame >= 0) {
	    return;	/* pixel was reprojected */
	}
    }

    /* Check the pixel map to determine if this image should be
     * rendered or not.
     */
    if (pixmap) {
	a.a_user= 1;	/* Force Shot Hit */

	if (pixmap[pindex + RED] + pixmap[pindex + GRN] + pixmap[pindex + BLU]) {
	    /* non-black pixmap pixel */

	    a.a_color[RED]= (double)(pixmap[pindex + RED]) * one_over_255;
	    a.a_color[GRN]= (double)(pixmap[pindex + GRN]) * one_over_255;
	    a.a_color[BLU]= (double)(pixmap[pindex + BLU]) * one_over_255;

	    /* we're done */
	    view_pixel(&a);
	    if ((size_t)a.a_x == width-1) {
		view_eol(&a);		/* End of scan line */
	    }
	    return;
	}
    }

    /* our starting point, used for non-jitter */
    VJOIN2 (point, viewbase_model, a.a_x, dx_model, a.a_y, dy_model);

    /* not tracing the corners of a prism by default */
    a.a_pixelext=(struct pixel_ext *)NULL;

    /* black or no pixmap, so compute the pixel(s) */

    /* LOOP BELOW IS UNROLLED ONE SAMPLE SINCE THAT'S THE COMMON CASE.
     *
     * XXX - If you edit the unrolled or non-unrolled section, be sure
     * to edit the other section.
     */
    if (hypersample == 0) {
	/* not hypersampling, so just do it */

	/****************/
	/* BEGIN UNROLL */
	/****************/

	if (jitter & JITTER_CELL) {
	    jitter_start_pt(point, &a, samplenum, pat_num);
	}

	if (a.a_rt_i->rti_prismtrace) {
	    /* compute the four corners */
	    pe.magic = PIXEL_EXT_MAGIC;
	    VJOIN2(pe.corner[0].r_pt, viewbase_model, a.a_x, dx_model, a.a_y, dy_model);
	    VJOIN2(pe.corner[1].r_pt, viewbase_model, (a.a_x+1), dx_model, a.a_y, dy_model);
	    VJOIN2(pe.corner[2].r_pt, viewbase_model, (a.a_x+1), dx_model, (a.a_y+1), dy_model);
	    VJOIN2(pe.corner[3].r_pt, viewbase_model, a.a_x, dx_model, (a.a_y+1), dy_model);
	    a.a_pixelext = &pe;
	}

	if (rt_perspective > 0.0) {
	    VSUB2(a.a_ray.r_dir, point, eye_model);
	    VUNITIZE(a.a_ray.r_dir);
	    VMOVE(a.a_ray.r_pt, eye_model);
	    if (a.a_rt_i->rti_prismtrace) {
		VSUB2(pe.corner[0].r_dir, pe.corner[0].r_pt, eye_model);
		VSUB2(pe.corner[1].r_dir, pe.corner[1].r_pt, eye_model);
		VSUB2(pe.corner[2].r_dir, pe.corner[2].r_pt, eye_model);
		VSUB2(pe.corner[3].r_dir, pe.corner[3].r_pt, eye_model);
	    }
	} else {
	    VMOVE(a.a_ray.r_pt, point);
	    VMOVE(a.a_ray.r_dir, APP.a_ray.r_dir);

	    if (a.a_rt_i->rti_prismtrace) {
		VMOVE(pe.corner[0].r_dir, a.a_ray.r_dir);
		VMOVE(pe.corner[1].r_dir, a.a_ray.r_dir);
		VMOVE(pe.corner[2].r_dir, a.a_ray.r_dir);
		VMOVE(pe.corner[3].r_dir, a.a_ray.r_dir);
	    }
	}
	if (report_progress) {
	    report_progress = 0;
	    bu_log("\tframe %d, xy=%d, %d on cpu %d, samp=%d\n", curframe, a.a_x, a.a_y, cpu, samplenum);
	}

	a.a_level = 0;		/* recursion level */
	a.a_purpose = "main ray";
	(void)rt_shootray(&a);

	if (stereo) {
	    fastf_t right, left;

	    right = CRT_BLEND(a.a_color);

	    VSUB2(stereo_point, point, left_eye_delta);
	    if (rt_perspective > 0.0) {
		VSUB2(a.a_ray.r_dir, stereo_point, eye_model);
		VUNITIZE(a.a_ray.r_dir);
		VADD2(a.a_ray.r_pt, eye_model, left_eye_delta);
	    } else {
		VMOVE(a.a_ray.r_pt, stereo_point);
	    }
	    a.a_level = 0;		/* recursion level */
	    a.a_purpose = "left eye ray";
	    (void)rt_shootray(&a);

	    left = CRT_BLEND(a.a_color);
	    VSET(a.a_color, left, 0, right);
	}
	VADD2(colorsum, colorsum, a.a_color);

	/**************/
	/* END UNROLL */
	/**************/

    } else {
	/* hypersampling, so iterate */

	for (samplenum=0; samplenum<=hypersample; samplenum++) {
	    /* shoot at a point based on the jitter pattern number */

	    /**********************/
	    /* BEGIN NON-UNROLLED */
	    /**********************/

	    if (jitter & JITTER_CELL) {
		jitter_start_pt(point, &a, samplenum, pat_num);
	    }

	    if (a.a_rt_i->rti_prismtrace) {
		/* compute the four corners */
		pe.magic = PIXEL_EXT_MAGIC;
		VJOIN2(pe.corner[0].r_pt, viewbase_model, a.a_x, dx_model, a.a_y, dy_model);
		VJOIN2(pe.corner[1].r_pt, viewbase_model, (a.a_x+1), dx_model, a.a_y, dy_model);
		VJOIN2(pe.corner[2].r_pt, viewbase_model, (a.a_x+1), dx_model, (a.a_y+1), dy_model);
		VJOIN2(pe.corner[3].r_pt, viewbase_model, a.a_x, dx_model, (a.a_y+1), dy_model);
		a.a_pixelext = &pe;
	    }

	    if (rt_perspective > 0.0) {
		VSUB2(a.a_ray.r_dir, point, eye_model);
		VUNITIZE(a.a_ray.r_dir);
		VMOVE(a.a_ray.r_pt, eye_model);
		if (a.a_rt_i->rti_prismtrace) {
		    VSUB2(pe.corner[0].r_dir, pe.corner[0].r_pt, eye_model);
		    VSUB2(pe.corner[1].r_dir, pe.corner[1].r_pt, eye_model);
		    VSUB2(pe.corner[2].r_dir, pe.corner[2].r_pt, eye_model);
		    VSUB2(pe.corner[3].r_dir, pe.corner[3].r_pt, eye_model);
		}
	    } else {
		VMOVE(a.a_ray.r_pt, point);
		VMOVE(a.a_ray.r_dir, APP.a_ray.r_dir);

		if (a.a_rt_i->rti_prismtrace) {
		    VMOVE(pe.corner[0].r_dir, a.a_ray.r_dir);
		    VMOVE(pe.corner[1].r_dir, a.a_ray.r_dir);
		    VMOVE(pe.corner[2].r_dir, a.a_ray.r_dir);
		    VMOVE(pe.corner[3].r_dir, a.a_ray.r_dir);
		}
	    }
	    if (report_progress) {
		report_progress = 0;
		bu_log("\tframe %d, xy=%d, %d on cpu %d, samp=%d\n", curframe, a.a_x, a.a_y, cpu, samplenum);
	    }

	    a.a_level = 0;		/* recursion level */
	    a.a_purpose = "main ray";
	    (void)rt_shootray(&a);

	    if (stereo) {
		fastf_t right, left;

		right = CRT_BLEND(a.a_color);

		VSUB2(stereo_point, point, left_eye_delta);
		if (rt_perspective > 0.0) {
		    VSUB2(a.a_ray.r_dir, stereo_point, eye_model);
		    VUNITIZE(a.a_ray.r_dir);
		    VADD2(a.a_ray.r_pt, eye_model, left_eye_delta);
		} else {
		    VMOVE(a.a_ray.r_pt, stereo_point);
		}
		a.a_level = 0;		/* recursion level */
		a.a_purpose = "left eye ray";
		(void)rt_shootray(&a);

		left = CRT_BLEND(a.a_color);
		VSET(a.a_color, left, 0, right);
	    }
	    VADD2(colorsum, colorsum, a.a_color);

	    /********************/
	    /* END NON-UNROLLED */
	    /********************/
	} /* for samplenum <= hypersample */

	{
	    /* scale the hypersampled results */
	    fastf_t f;
	    f = 1.0 / (hypersample+1);
	    VSCALE(a.a_color, colorsum, f);
	}
    } /* end unrolling else case */

    /* bu_log("2: [%d, %d] : [%.2f, %.2f, %.2f]\n", pixelnum%width, pixelnum/width, a.a_color[0], a.a_color[1], a.a_color[2]); */

    /* FIXME: this should work on windows after the bu_timer() is
     * created to replace the librt timing mechanism.
     */
#if !defined(_WIN32) || defined(__CYGWIN__)
    /* Add get_pixel_timer here to get total time taken to get pixel, when asked */
    if (lightmodel == 8) {
	fastf_t pixelTime;
	fastf_t **timeTable;

	pixelTime = rt_get_timer(NULL,NULL);
	/* bu_log("PixelTime = %lf X:%d Y:%d\n", pixelTime, a.a_x, a.a_y); */
	bu_semaphore_acquire(RT_SEM_LAST-2);
	timeTable = timeTable_init(width, height);
	timeTable_input(a.a_x, a.a_y, pixelTime, timeTable);
	bu_semaphore_release(RT_SEM_LAST-2);
    }
#endif
    /* we're done */
    view_pixel(&a);
    if ((size_t)a.a_x == width-1) {
	view_eol(&a);		/* End of scan line */
    }
    return;
}


/**
 * Compute some pixels, and store them.
 *
 * This uses a "self-dispatching" parallel algorithm.  Executes until
 * there is no more work to be done, or is told to stop.
 *
 * In order to reduce the traffic through the res_worker critical
 * section, a multiple pixel block may be removed from the work queue
 * at once.
 *
 * For a general-purpose version, see LIBRT rt_shoot_many_rays()
 */
void
worker(int cpu, void *UNUSED(arg))
{
    int pixel_start;
    int pixelnum;
    int pat_num = -1;

    /* The more CPUs at work, the bigger the bites we take */
    if (per_processor_chunk <= 0) per_processor_chunk = npsw;

    if (cpu >= MAX_PSW) {
	bu_log("rt/worker() cpu %d > MAX_PSW %d, array overrun\n", cpu, MAX_PSW);
	bu_exit(EXIT_FAILURE, "rt/worker() cpu > MAX_PSW, array overrun\n");
    }
    RT_CK_RESOURCE(&resource[cpu]);

    pat_num = -1;
    if (hypersample) {
	int i, ray_samples;

	ray_samples = hypersample + 1;
	for (i=0; pt_pats[i].num_samples != 0; i++) {
	    if (pt_pats[i].num_samples == ray_samples) {
		pat_num = i;
		goto pat_found;
	    }
	}
    }
pat_found:

    if (transpose_grid) {
	int tmp;

	/* switch cur_pixel and last_pixel */
	tmp = cur_pixel;
	cur_pixel = last_pixel;
	last_pixel = tmp;

	while (1) {
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
    } else if (random_mode) {

	while (1) {
	    /* Generate a random pixel id between 0 and last_pixel
	       inclusive - TODO: check if there is any issue related
	       with multi-threaded RNG */
	    pixelnum = rand()*1.0/RAND_MAX*(last_pixel + 1);
	    if (pixelnum >= last_pixel) pixelnum = last_pixel;
	    do_pixel(cpu, pat_num, pixelnum);
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


/**
 * In theory, the grid can be specified by providing any two of
 * these sets of parameters:
 *
 * number of pixels (width, height)
 * viewsize (in model units, mm)
 * number of grid cells (cell_width, cell_height)
 *
 * however, for now, it is required that the view size always be
 * specified, and one or the other parameter be provided.
 */
void
grid_setup(void)
{
    vect_t temp;
    mat_t toEye;

    if (viewsize <= 0.0)
	bu_exit(EXIT_FAILURE, "viewsize <= 0");
    /* model2view takes us to eye_model location & orientation */
    MAT_IDN(toEye);
    MAT_DELTAS_VEC_NEG(toEye, eye_model);
    Viewrotscale[15] = 0.5*viewsize;	/* Viewscale */
    bn_mat_mul(model2view, Viewrotscale, toEye);
    bn_mat_inv(view2model, model2view);

    /* Determine grid cell size and number of pixels */
    if (cell_newsize) {
	if (cell_width <= 0.0) cell_width = cell_height;
	if (cell_height <= 0.0) cell_height = cell_width;
	width = (viewsize / cell_width) + 0.99;
	height = (viewsize / (cell_height*aspect)) + 0.99;
	cell_newsize = 0;
    } else {
	/* Chop -1.0..+1.0 range into parts */
	cell_width = viewsize / width;
	cell_height = viewsize / (height*aspect);
    }

    /*
     * Optional GIFT compatibility, mostly for RTG3.  Round coordinates
     * of lower left corner to fall on integer- valued coordinates, in
     * "gift_grid_rounding" units.
     */
    if (gift_grid_rounding > 0.0) {
	point_t v_ll;		/* view, lower left */
	point_t m_ll;		/* model, lower left */
	point_t hv_ll;		/* hv, lower left*/
	point_t hv_wanted;
	vect_t hv_delta;
	vect_t m_delta;
	mat_t model2hv;
	mat_t hv2model;

	/* Build model2hv matrix, including mm2inches conversion */
	MAT_COPY(model2hv, Viewrotscale);
	model2hv[15] = gift_grid_rounding;
	bn_mat_inv(hv2model, model2hv);

	VSET(v_ll, -1, -1, 0);
	MAT4X3PNT(m_ll, view2model, v_ll);
	MAT4X3PNT(hv_ll, model2hv, m_ll);
	VSET(hv_wanted, floor(hv_ll[X]), floor(hv_ll[Y]), floor(hv_ll[Z]));
	VSUB2(hv_delta, hv_ll, hv_wanted);

	MAT4X3PNT(m_delta, hv2model, hv_delta);
	VSUB2(eye_model, eye_model, m_delta);
	MAT_DELTAS_VEC_NEG(toEye, eye_model);
	bn_mat_mul(model2view, Viewrotscale, toEye);
	bn_mat_inv(view2model, model2view);
    }

    /* Create basis vectors dx and dy for emanation plane (grid) */
    VSET(temp, 1, 0, 0);
    MAT3X3VEC(dx_unit, view2model, temp);	/* rotate only */
    VSCALE(dx_model, dx_unit, cell_width);

    VSET(temp, 0, 1, 0);
    MAT3X3VEC(dy_unit, view2model, temp);	/* rotate only */
    VSCALE(dy_model, dy_unit, cell_height);

    if (stereo) {
	/* Move left 2.5 inches (63.5mm) */
	VSET(temp, -63.5*2.0/viewsize, 0, 0);
	bu_log("red eye: moving %f relative screen (left)\n", temp[X]);
	MAT4X3VEC(left_eye_delta, view2model, temp);
	VPRINT("left_eye_delta", left_eye_delta);
    }

    /* "Lower left" corner of viewing plane */
    if (rt_perspective > 0.0) {
	fastf_t zoomout;
	zoomout = 1.0 / tan(DEG2RAD * rt_perspective / 2.0);
	VSET(temp, -1, -1/aspect, -zoomout);	/* viewing plane */

	/*
	 * divergence is perspective angle divided by the number of
	 * pixels in that angle. Extra factor of 0.5 is because
	 * perspective is a full angle while divergence is the tangent
	 * (slope) of a half angle.
	 */
	APP.a_diverge = tan(DEG2RAD * rt_perspective * 0.5 / width);
	APP.a_rbeam = 0;
    } else {
	/* all rays go this direction */
	VSET(temp, 0, 0, -1);
	MAT4X3VEC(APP.a_ray.r_dir, view2model, temp);
	VUNITIZE(APP.a_ray.r_dir);

	VSET(temp, -1, -1/aspect, 0);	/* eye plane */
	APP.a_rbeam = 0.5 * viewsize / width;
	APP.a_diverge = 0;
    }
    if (ZERO(APP.a_rbeam) && ZERO(APP.a_diverge))
	bu_exit(EXIT_FAILURE, "zero-radius beam");
    MAT4X3PNT(viewbase_model, view2model, temp);

    if (jitter & JITTER_FRAME) {
	/* Move the frame in a smooth circular rotation in the plane */
	fastf_t ang;	/* radians */
	fastf_t dx, dy;

	ang = curframe * frame_delta_t * M_2PI / 10;	/* 10 sec period */
	dx = cos(ang) * 0.5;	/* +/- 1/4 pixel width in amplitude */
	dy = sin(ang) * 0.5;
	VJOIN2(viewbase_model, viewbase_model,
	       dx, dx_model,
	       dy, dy_model);
    }

    if (cell_width <= 0 || cell_width >= INFINITY ||
	cell_height <= 0 || cell_height >= INFINITY) {
	bu_log("grid_setup: cell size ERROR (%g, %g) mm\n",
	       cell_width, cell_height);
	bu_exit(EXIT_FAILURE, "cell size");
    }
    if (width <= 0 || height <= 0) {
	bu_log("grid_setup: ERROR bad image size (%zu, %zu)\n",
	       width, height);
	bu_exit(EXIT_FAILURE, "bad size");
    }
}


/**
 * Compute a run of pixels, in parallel if the hardware permits it.
 *
 * For a general-purpose version, see LIBRT rt_shoot_many_rays().
 */
void
do_run(int a, int b)
{
    int cpu;

#ifdef USE_FORKED_THREADS
    int pid, wpid;
    int waitret;
    void *buffer = (void*)0;
    int p[2] = {0, 0};
    struct resource *tmp_res;

    if (RTG.rtg_parallel) {
	buffer = bu_calloc(npsw, sizeof(resource[0]), "buffer");
	if (pipe(p) == -1) {
	    perror("pipe failed");
	}
    }
#endif

    cur_pixel = a;
    last_pixel = b;

    if (!RTG.rtg_parallel) {
	/*
	 * SERIAL case -- one CPU does all the work.
	 */
	npsw = 1;
	worker(0, NULL);
    } else {
	/*
	 * Parallel case.
	 */

	/* hack to bypass a bug in the Linux 2.4 kernel pthreads
	 * implementation. cpu statistics are only traceable on a
	 * process level and the timers will report effectively no
	 * elapsed cpu time.  this allows the stats of all threads to
	 * be gathered up by an encompassing process that may be
	 * timed.
	 *
	 * XXX this should somehow only apply to a build on a 2.4
	 * linux kernel.
	 */
#ifdef USE_FORKED_THREADS
	pid = fork();
	if (pid < 0) {
	    perror("fork failed");
	    bu_exit(1, NULL);
	} else if (pid == 0) {
#endif

	    bu_parallel(worker, npsw, NULL);

#ifdef USE_FORKED_THREADS
	    /* send raytrace instance data back to the parent */
	    if (write(p[1], resource, sizeof(resource[0]) * npsw) == -1) {
		perror("Unable to write to the communication pipe");
		bu_exit(1, NULL);
	    }
	    /* flush the pipe */
	    if (close(p[1]) == -1) {
		perror("Unable to close the communication pipe");
		sleep(1); /* give the parent time to read */
	    }
	    bu_exit(0, NULL);
	} else {
	    if (read(p[0], buffer, sizeof(resource[0]) * npsw) == -1) {
		perror("Unable to read from the communication pipe");
	    }

	    /* do not use the just read info to overwrite the resource
	     * structures.  doing so will hose the resources
	     * completely
	     */

	    /* parent ends up waiting on his child (and his child's
	     * threads) to terminate.  we can get valid usage
	     * statistics on a child process.
	     */
	    while ((wpid = wait(&waitret)) != pid && wpid != -1)
		; /* do nothing */
	} /* end fork() */
#endif

    } /* end parallel case */

#ifdef USE_FORKED_THREADS
    if (RTG.rtg_parallel) {
	tmp_res = (struct resource *)buffer;
    } else {
	tmp_res = resource;
    }
    for (cpu=0; cpu < npsw; cpu++) {
	if (tmp_res[cpu].re_magic != RESOURCE_MAGIC) {
	    bu_log("ERROR: CPU %d resources corrupted, statistics bad\n", cpu);
	    continue;
	}
	rt_add_res_stats(APP.a_rt_i, &tmp_res[cpu]);
	rt_zero_res_stats(&resource[cpu]);
    }
#else
    /* Tally up the statistics */
    for (cpu=0; cpu < npsw; cpu++) {
	if (resource[cpu].re_magic != RESOURCE_MAGIC) {
	    bu_log("ERROR: CPU %d resources corrupted, statistics bad\n", cpu);
	    continue;
	}
	rt_add_res_stats(APP.a_rt_i, &resource[cpu]);
    }
#endif
    return;
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
