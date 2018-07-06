/*              A N A L Y Z E _ O V E R L A P S . C
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
    size_t npsw;
    struct application APP;
    struct grid_generator_functions *grid_function;
    void *grid_context;
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
overlapsAdapter(struct application *ap, const struct partition *pp, const struct bu_ptbl *regiontable, const struct partition *UNUSED(hp))
{
    struct analyze_private_callback_data* callBack = (struct analyze_private_callback_data*) ap->a_uptr;
    callBack->overlapHandler(&ap->a_ray, pp, regiontable, callBack->overlapHandlerData);
}


HIDDEN int
analov_do_pixel(int cpu, void *workerDataPointer)
{
    struct worker_context *workerData = (struct worker_context *) workerDataPointer;
    struct application a = workerData->APP;
    a.a_resource = &analov_res[cpu];

    a.a_pixelext=(struct pixel_ext *)NULL;
    bu_semaphore_acquire(RT_SEM_WORKER);
    if (workerData->grid_function->next_ray(&a.a_ray, workerData->grid_context) == 1){
	bu_semaphore_release(RT_SEM_WORKER);
	return ANALYZE_OK;
    }
    bu_semaphore_release(RT_SEM_WORKER);

    /* bu_log("\n%g, %g, %g", V3ARGS(a.a_ray.r_pt)); */
    a.a_level = 0;
    a.a_purpose = "main ray";
    (void)rt_shootray(&a);
    return 1;
}


HIDDEN void
analov_worker(int cpu, void *arg)
{
    if (cpu >= MAX_PSW) {
	bu_log("rt/worker() cpu %d > MAX_PSW %d, array overrun\n", cpu, MAX_PSW);
	bu_exit(EXIT_FAILURE, "rt/worker() cpu > MAX_PSW, array overrun\n");
    }

    RT_CK_RESOURCE(&analov_res[cpu]);

    while (analov_do_pixel(cpu, arg) != ANALYZE_OK);
}

/*
 * Compute a run of pixels, in parallel if the hardware permits it.
 */
HIDDEN int
analov_do_run(void *workerDataPointer)
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

int
analyze_overlaps(struct rt_i *rtip,
		 size_t npsw,
		 analyze_overlaps_callback callback,
		 void *callBackData,
		 struct grid_generator_functions *grid_generator,
		 void *grid_context)
{
    int i;
    struct application APP;
    struct analyze_private_callback_data adapterData;
    struct worker_context workerData;

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
    APP.a_rbeam = 0.5 * (grid_generator->grid_cell_width(grid_context));
    APP.a_diverge = 0;

    if (ZERO(APP.a_rbeam) && ZERO(APP.a_diverge)) {
	bu_log("zero-radius beam");
	return ANALYZE_ERROR;
    }

    memset(analov_res, 0, sizeof(analov_res));
    for (i = 0; i < MAX_PSW; i++) {
	rt_init_resource(&analov_res[i], i, rtip);
    }

    if (rtip->needprep) {
	rt_prep_parallel(rtip, npsw);
    }
    /* Initialize workerData before calling do_run*/
    workerData.grid_function = grid_generator;
    workerData.grid_context = grid_context;
    workerData.npsw = npsw;
    workerData.APP = APP;

    if (analov_do_run(&workerData))
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
