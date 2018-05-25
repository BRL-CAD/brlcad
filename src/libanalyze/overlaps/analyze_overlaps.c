/*                    C H E C K _ O V E R L A P S . C
 * BRL-CAD
 *
 * Copyright (c) 2018 United States Government as represented by
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
#include "common.h"

#include <stdlib.h>
#include <string.h>

#include "bu/debug.h"
#include "bu/malloc.h"
#include "bu/log.h"
#include "bu/parallel.h"
#include "vmath.h"
#include "raytrace.h"	/* librt interface definitions */

#include "analyze.h"

/* for fork/pipe linux timing hack */
#ifdef USE_FORKED_THREADS
#  include <sys/select.h>
#  include <sys/types.h>
#  ifdef HAVE_SYS_WAIT_H
#    include <sys/wait.h>
#  endif
#endif


struct resource analov_res[MAX_PSW];	/* memory resources */


struct analyze_private_callback_data {
    analyze_overlaps_callback overlapHandler;
    void* overlapHandlerData;
};


struct worker_context {
    int per_processor_chunk;
    int cur_pixel;
    int last_pixel;
    size_t npsw;
    struct application APP;
    size_t width;
    vect_t dx_model;
    vect_t dy_model;
    point_t viewbase_model;
};


/*
 * Null function -- handle a hit
 */
/*ARGSUSED*/
HIDDEN int
analov_hit(struct application *UNUSED(ap), register struct partition *UNUSED(PartHeadp), struct seg *UNUSED(segHeadp))
{
    return 1;
}


/*
 * Null function -- handle a miss
 */
/*ARGSUSED*/
HIDDEN int
analov_miss(struct application *UNUSED(ap))
{
    return 0;
}


HIDDEN void
overlapsAdapter(struct application *ap, const struct partition *pp, const struct bu_ptbl *regiontable, const struct partition *hp)
{
    struct analyze_private_callback_data* callBack = (struct analyze_private_callback_data*) ap->a_uptr;
    callBack->overlapHandler(ap, pp, regiontable, hp, callBack->overlapHandlerData);
}


HIDDEN void
analov_do_pixel(int cpu, int pixelnum, void *workerDataPointer)
{
    struct worker_context *workerData = (struct worker_context *) workerDataPointer;
    struct application a = workerData->APP;
    vect_t point;		/* Ref point on eye or view plane */

    a.a_resource = &analov_res[cpu];
    a.a_y = (int)(pixelnum/(workerData->width));
    a.a_x = (int)(pixelnum - (a.a_y * (workerData->width)));

    VJOIN2 (point, workerData->viewbase_model, a.a_x, workerData->dx_model, a.a_y, workerData->dy_model);

    a.a_pixelext=(struct pixel_ext *)NULL;

    VMOVE(a.a_ray.r_pt, point);
    VMOVE(a.a_ray.r_dir, a.a_ray.r_dir);

    a.a_level = 0;
    a.a_purpose = "main ray";
    (void)rt_shootray(&a);
}


HIDDEN void
analov_worker(int cpu, void *arg)
{
    int pixel_start;
    int pixelnum;
    struct worker_context *workerData = (struct worker_context *) arg;

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
    if (workerData->per_processor_chunk <= 0) {
	size_t chunk_size;
	size_t one_eighth = ((workerData->last_pixel) - (workerData->cur_pixel)) / 8;
	if (UNLIKELY(one_eighth < 1))
	    one_eighth = 1;

	if (one_eighth > (workerData->npsw) * 262144)
	    chunk_size = 262144; /* 512x512 */
	else if (one_eighth > (workerData->npsw) * 65536)
	    chunk_size = 65536; /* 256x256 */
	else if (one_eighth > (workerData->npsw) * 16384)
	    chunk_size = 16384; /* 128x128 */
	else if (one_eighth > (workerData->npsw) * 4096)
	    chunk_size = 4096; /* 64x64 */
	else if (one_eighth > (workerData->npsw) * 1024)
	    chunk_size = 1024; /* 32x32 */
	else if (one_eighth > (workerData->npsw) * 256)
	    chunk_size = 256; /* 16x16 */
	else if (one_eighth > (workerData->npsw) * 64)
	    chunk_size = 64; /* 8x8 */
	else if (one_eighth > (workerData->npsw) * 16)
	    chunk_size = 16; /* 4x4 */
	else if (one_eighth > (workerData->npsw) * 4)
	    chunk_size = 4; /* 2x2 */
	else
	    chunk_size = 1; /* one pixel at a time */

	bu_semaphore_acquire(RT_SEM_WORKER);
	workerData->per_processor_chunk = chunk_size;
	bu_semaphore_release(RT_SEM_WORKER);
    }

    if (cpu >= MAX_PSW) {
	bu_log("rt/worker() cpu %d > MAX_PSW %d, array overrun\n", cpu, MAX_PSW);
	bu_exit(EXIT_FAILURE, "rt/worker() cpu > MAX_PSW, array overrun\n");
    }

    RT_CK_RESOURCE(&analov_res[cpu]);

    while (1) {
	bu_semaphore_acquire(RT_SEM_WORKER);
	pixel_start = workerData->cur_pixel;
	workerData->cur_pixel += workerData->per_processor_chunk;
	bu_semaphore_release(RT_SEM_WORKER);

	for (pixelnum = pixel_start; pixelnum < pixel_start+(workerData->per_processor_chunk); pixelnum++) {

	if (pixelnum > workerData->last_pixel)
	    return;

	analov_do_pixel(cpu, pixelnum, arg);
	}
    }
}

/*
 * Compute a run of pixels, in parallel if the hardware permits it.
 */
HIDDEN int
analov_do_run(int a, int b, void *workerDataPointer)
{
    size_t cpu;
    struct worker_context *workerData = (struct worker_context *) workerDataPointer;
    struct application *APP = &(workerData->APP);

#ifdef USE_FORKED_THREADS
    int pid, wpid;
    int waitret;
    void *buffer = (void*)0;
    int p[2] = {0, 0};
    struct resource *tmp_res;

    if (RTG.rtg_parallel) {
	buffer = bu_calloc((workerData->npsw), sizeof(analov_res[0]), "buffer");
	if (pipe(p) == -1) {
	    perror("pipe failed");
	}
    }
#endif

    (workerData->cur_pixel) = a;
    (workerData->last_pixel) = b;

    if (!RTG.rtg_parallel) {
	/*
	 * SERIAL case -- one CPU does all the work.
	 */
	(workerData->npsw) = 1;
	analov_worker(0, workerDataPointer);
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
	    return ANALYZE_ERROR;
	} else if (pid == 0) {
#endif
	    bu_parallel(analov_worker, (workerData->npsw), workerDataPointer);

#ifdef USE_FORKED_THREADS
	    /* send raytrace instance data back to the parent */
	    if (write(p[1], resource, sizeof(analov_res[0]) * (workerData->npsw)) == -1) {
		perror("Unable to write to the communication pipe");
	    	return ANALYZE_ERROR;
	    }
	    /* flush the pipe */
	    if (close(p[1]) == -1) {
		perror("Unable to close the communication pipe");
		sleep(1); /* give the parent time to read */
	    }
	    return ANALYZE_ERROR;
	} else {
	    if (read(p[0], buffer, sizeof(analov_res[0]) * (workerData->npsw)) == -1) {
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
	tmp_res = analov_res;
    }
    for (cpu=0; cpu < (workerData->npsw); cpu++) {
	if (tmp_res[cpu].re_magic != RESOURCE_MAGIC) {
	    bu_log("ERROR: CPU %d resources corrupted, statistics bad\n", cpu);
	    continue;
	}
	rt_add_res_stats(APP->a_rt_i, &tmp_res[cpu]);
	rt_zero_res_stats(&analov_res[cpu]);
    }
#else
    /* Tally up the statistics */
    for (cpu=0; cpu < (workerData->npsw); cpu++) {
	if (analov_res[cpu].re_magic != RESOURCE_MAGIC) {
	    bu_log("ERROR: CPU %d resources corrupted, statistics bad\n", cpu);
	    continue;
	}
	rt_add_res_stats(APP->a_rt_i, &analov_res[cpu]);
    }
#endif
    return ANALYZE_OK;
}


HIDDEN int
analov_grid_setup(struct application *APP,
		  fastf_t aspect,
		  fastf_t viewsize,
		  size_t width,
		  fastf_t cell_height,
		  fastf_t cell_width,
		  mat_t view2model,
		  vect_t dx_model,
		  vect_t dy_model,
		  vect_t dx_unit,
		  vect_t dy_unit,
		  point_t viewbase_model)
{
    vect_t temp;
    
    /* Create basis vectors dx and dy for emanation plane (grid) */
    VSET(temp, 1, 0, 0);
    MAT3X3VEC(dx_unit, view2model, temp);	/* rotate only */
    VSCALE(dx_model, dx_unit, cell_width);

    VSET(temp, 0, 1, 0);
    MAT3X3VEC(dy_unit, view2model, temp);	/* rotate only */
    VSCALE(dy_model, dy_unit, cell_height);

    /* all rays go this direction */
    VSET(temp, 0, 0, -1);
    MAT4X3VEC(APP->a_ray.r_dir, view2model, temp);
    VUNITIZE(APP->a_ray.r_dir);

    VSET(temp, -1, -1/aspect, 0);	/* eye plane */
    APP->a_rbeam = 0.5 * viewsize / width;
    APP->a_diverge = 0;

    if (ZERO(APP->a_rbeam) && ZERO(APP->a_diverge)) {
	bu_log("zero-radius beam");
	return ANALYZE_ERROR;
    }

    MAT4X3PNT(viewbase_model, view2model, temp);

    return ANALYZE_OK;
}


int
analyze_overlaps(struct rt_i *rtip,
		 size_t width,
		 size_t height,
		 fastf_t cell_width,
		 fastf_t cell_height,
		 fastf_t aspect,
		 fastf_t viewsize,
		 mat_t model2view,
		 size_t npsw,
		 analyze_overlaps_callback callback,
		 void *callBackData)
{
    int i;
    struct application APP;
    struct analyze_private_callback_data adapterData;

    int pix_start;		/* pixel to start at */
    int pix_end;		/* pixel to end at */
    int cur_pixel = 0;		/* current pixel number, 0..last_pixel */
    int last_pixel = 0;		/* last pixel number */
    int per_processor_chunk = 0;
    struct worker_context workerData;
    void *workerDataPointer;
    mat_t view2model;
    vect_t dx_model;		/* view delta-X as model-space vect */
    vect_t dy_model;		/* view delta-Y as model-space vect */
    vect_t dx_unit;		/* view delta-X as unit-len vect */
    vect_t dy_unit;		/* view delta-Y as unit-len vect */
    point_t viewbase_model;	/* model-space location of viewplane corner */

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);
    setmode(fileno(stderr), O_BINARY);

    bu_setlinebuf(stdout);
    bu_setlinebuf(stderr);

    /* application-specific initialization */
    RT_APPLICATION_INIT(&APP);

    /*
     *  Handle parallel initialization, if applicable.
     */
#ifndef PARALLEL
    npsw = 1;			/* force serial */
#endif

    /* allow debug builds to go higher than the max */
    if (!(bu_debug & BU_DEBUG_PARALLEL)) {
	if (npsw > MAX_PSW) {
	    npsw = MAX_PSW;
	}
    }

    if (npsw > 1) {
	RTG.rtg_parallel = 1;
    } else {
	RTG.rtg_parallel = 0;
    }

    /* Initialize adapterData before application initialization. */
    adapterData.overlapHandler = callback;
    adapterData.overlapHandlerData = callBackData;

    /* Initialize application. */
    APP.a_rt_i = rtip;
    APP.a_hit = analov_hit;
    APP.a_miss = analov_miss;
    APP.a_logoverlap = overlapsAdapter;
    APP.a_uptr = &adapterData;
    APP.a_onehit = 0;

    memset(analov_res, 0, sizeof(analov_res));
    for (i = 0; i < MAX_PSW; i++) {
	rt_init_resource(&analov_res[i], i, rtip);
    }

    rt_prep_parallel(rtip, npsw);

    /* Initialize the view2model before calling do_frame */
    bn_mat_inv(view2model, model2view);
    /* remaining parts of grid_setup */
    if (analov_grid_setup(&APP, aspect, viewsize, width, cell_height, cell_width, view2model, dx_model, dy_model, dx_unit, dy_unit, viewbase_model))
    	return ANALYZE_ERROR;

    pix_start = 0;
    pix_end = (int)(height * width - 1);

    /* Initialize workerData before calling do_run*/
    workerData.per_processor_chunk = per_processor_chunk;
    workerData.cur_pixel = cur_pixel;
    workerData.last_pixel = last_pixel;
    workerData.npsw = npsw;
    workerData.APP = APP;
    workerData.width = width;
    VMOVE(workerData.dx_model,dx_model);
    VMOVE(workerData.dy_model,dy_model);
    VMOVE(workerData.viewbase_model,viewbase_model);

    workerDataPointer = (void*)(&workerData);

    /*
     * Compute the image
     * It may prove desirable to do this in chunks
     */
    if (analov_do_run(pix_start, pix_end, workerDataPointer))
	return ANALYZE_ERROR;

    return ANALYZE_OK;
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
