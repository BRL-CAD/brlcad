/*                        W O R K E R . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2024 United States Government as represented by
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
#include "dm.h"		/* Added because RGBpixel is now needed in do_pixel() */

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
extern int query_optical_debug;
extern int query_debug;

extern unsigned char *pixmap;	/* pixmap for rerendering of black pixels */

int per_processor_chunk = 0;	/* how many pixels to do at once */

int fullfloat_mode = 0;
int reproject_mode = 0;
struct floatpixel *curr_float_frame; /* buffer of full frame */
struct floatpixel *prev_float_frame;
int reproj_cur;	/* number of pixels reprojected this frame */
int reproj_max;	/* out of total number of pixels */

/* Local communication with worker() */
int cur_pixel = 0;			/* current pixel number, 0..last_pixel */
int last_pixel = 0;			/* last pixel number */

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
jitter_start_pnt(vect_t point, struct application *a, int samplenum, int pat_num)
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

    /* for stereo output */
    vect_t left_eye_delta = VINIT_ZERO;

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
	a.a_y = (int)(pixelnum/width);
	a.a_x = (int)(pixelnum - (a.a_y * width));
	/* a.a_x = pixelnum%width; */
    }

    if (Query_one_pixel) {
	if (a.a_x == query_x && a.a_y == query_y) {
	    optical_debug = query_optical_debug;
	    rt_debug = query_debug;
	} else {
	    rt_debug = optical_debug = 0;
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
	    jitter_start_pnt(point, &a, samplenum, pat_num);
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
	    vect_t temp;

	    right = CRT_BLEND(a.a_color);

	    /* Move left 2.5 inches (63.5mm) */
	    VSET(temp, -63.5*2.0/viewsize, 0, 0);
	    MAT4X3VEC(left_eye_delta, view2model, temp);

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
		jitter_start_pnt(point, &a, samplenum, pat_num);
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
		vect_t temp;

		right = CRT_BLEND(a.a_color);

		/* Move left 2.5 inches (63.5mm) */
		VSET(temp, -63.5*2.0/viewsize, 0, 0);
		MAT4X3VEC(left_eye_delta, view2model, temp);

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

    /* Add get_pixel_timer here to get total time taken to get pixel, when asked */
    if (lightmodel == 8) {
	fastf_t pixelTime;
	fastf_t **timeTable;

	pixelTime = rt_get_timer(NULL,NULL); /* FIXME: needs to use bu_gettime() */
	/* bu_log("PixelTime = %lf X:%d Y:%d\n", pixelTime, a.a_x, a.a_y); */
	bu_semaphore_acquire(RT_SEM_RESULTS);
	timeTable = timeTable_init(width, height);
	timeTable_input(a.a_x, a.a_y, pixelTime, timeTable);
	bu_semaphore_release(RT_SEM_RESULTS);
    }

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
 */
void
worker(int cpu, void *UNUSED(arg))
{
    int pixel_start;
    int pixelnum;
    int pat_num = -1;

    /* Figure out a reasonable chunk size that should keep most
     * workers busy all the way to the end.  We divide up the image
     * into chunks equating to tiles 1x1, 2x2, 4x4, 8x8 ... in size.
     * Work is distributed so that all CPUs work on at least 8 chunks
     * with the chunking adjusted from a maximum chunk size (512x512)
     * all the way down to 1 pixel at a time, depending on the number
     * of cores and the size of our rendering.
     *
     * TODO: actually work on image tiles instead of pixel spans.
     */
    if (per_processor_chunk <= 0) {
	size_t chunk_size;
	size_t one_eighth = (last_pixel - cur_pixel) * (hypersample + 1) / 8;
	if (UNLIKELY(one_eighth < 1))
	    one_eighth = 1;

	if (one_eighth > (size_t)npsw * 262144)
	    chunk_size = 262144; /* 512x512 */
	else if (one_eighth > (size_t)npsw * 65536)
	    chunk_size = 65536; /* 256x256 */
	else if (one_eighth > (size_t)npsw * 16384)
	    chunk_size = 16384; /* 128x128 */
	else if (one_eighth > (size_t)npsw * 4096)
	    chunk_size = 4096; /* 64x64 */
	else if (one_eighth > (size_t)npsw * 1024)
	    chunk_size = 1024; /* 32x32 */
	else if (one_eighth > (size_t)npsw * 256)
	    chunk_size = 256; /* 16x16 */
	else if (one_eighth > (size_t)npsw * 64)
	    chunk_size = 64; /* 8x8 */
	else if (one_eighth > (size_t)npsw * 16)
	    chunk_size = 16; /* 4x4 */
	else if (one_eighth > (size_t)npsw * 4)
	    chunk_size = 4; /* 2x2 */
	else
	    chunk_size = 1; /* one pixel at a time */

	bu_semaphore_acquire(RT_SEM_WORKER);
	per_processor_chunk = chunk_size;
	bu_semaphore_release(RT_SEM_WORKER);
    }

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

	int from;
	int to;
    if (random_mode) {

	// FIXME: this is a error in Parallel case
		int* pixel_ls = malloc(last_pixel * sizeof(int));
		if (pixel_ls != NULL){
			for (int i = 0; i < last_pixel; i++)
				pixel_ls[i] = i;
			//shuffle array
			if (last_pixel > 1) {
				for (int i = last_pixel - 1; i > 0; i--) {
					int j = rand() % (i + 1);
					int tmp = pixel_ls[i];
					pixel_ls[i] = pixel_ls[j];
					pixel_ls[j] = tmp;
				}
			}
			while (1) {
				int* sub_pixel_ls = malloc(per_processor_chunk * sizeof(int));
				if (sub_pixel_ls != NULL) {
					int sub_pixel_len = 0;
					if (stop_worker)
						return;

					bu_semaphore_acquire(RT_SEM_WORKER);
					pixel_start = cur_pixel;
					for (int i = cur_pixel, j = 0; i < cur_pixel + per_processor_chunk && i < last_pixel; i++, j++) {
						sub_pixel_ls[j] = pixel_ls[i];
						sub_pixel_len++;
					}
					to = cur_pixel + sub_pixel_len;
					cur_pixel += per_processor_chunk;
					//bu_log("SPAN[%d -> %d] for %d pixels\n", pixel_start, pixel_start+per_processor_chunk, per_processor_chunk); 
					bu_semaphore_release(RT_SEM_WORKER);

					for (int i = 0; i < sub_pixel_len; i++) {
						//bu_log("    PIXEL[%d]\n", sub_pixel_ls[i]);
						do_pixel(cpu, pat_num, sub_pixel_ls[i]);
					}
					if (to >= last_pixel) {
						free(sub_pixel_ls);
						free(pixel_ls);
						return;
					}
				}
				else {
					bu_log("malloc failed!");
					return;
				}
			}
		} else {
			bu_log("malloc failed!");
			return;
		}

    } else {

	while (1) {
	    if (stop_worker)
		return;

	    bu_semaphore_acquire(RT_SEM_WORKER);
	    pixel_start = cur_pixel;
	    cur_pixel += per_processor_chunk;
	    bu_semaphore_release(RT_SEM_WORKER);

	    if (top_down) {
		from = last_pixel - pixel_start;
		to = from - per_processor_chunk;
	    } else {
		from = pixel_start;
		to = pixel_start + per_processor_chunk;
	    }

	    /* bu_log("SPAN[%d -> %d] for %d pixels\n", pixel_start, pixel_start+per_processor_chunk, per_processor_chunk); */
	    for (pixelnum = from; pixelnum != to; (from < to) ? pixelnum++ : pixelnum--) {
		if (pixelnum > last_pixel || pixelnum < 0)
		    return;

		/* bu_log("    PIXEL[%d]\n", pixelnum); */
		do_pixel(cpu, pat_num, pixelnum);
	    }
	}
    }
}


/**
 * Compute a run of pixels, in parallel if the hardware permits it.
 */
void
do_run(int a, int b)
{
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
	bu_parallel(worker, (size_t)npsw, NULL);
    }

    /* Tally up the statistics */
    size_t cpu;
    for (cpu = 0; cpu < MAX_PSW; cpu++) {
	if (resource[cpu].re_magic != RESOURCE_MAGIC) {
	    bu_log("ERROR: CPU %zu resources corrupted, statistics bad\n", cpu);
	    continue;
	}
	rt_add_res_stats(APP.a_rt_i, &resource[cpu]);
	rt_zero_res_stats(&resource[cpu]);
    }

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
