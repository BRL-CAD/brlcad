/*
 *			M A N Y . C
 *
 *  Wrapper routines to help fire multiple rays in parallel,
 *  without exposing the caller to the details of running in parallel.
 *
 *  Authors -
 *	Michael John Muuss
 *	Christopher T. Johnson
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Package" license agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1999-2004 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"

/* For communication between interface routine and each of the threads */
struct rt_many_internal  {
	long			magic;
	long			cur_index;		/* semaphored */
	long			max_index;
	const struct application *proto_ap;
	struct resource		*resources;
	int			(*callback) BU_ARGS((struct application *, int index));
	int			stop_worker;
	int			sem_chunk;
};
#define RT_MANY_INTERNAL_MAGIC	0x526d6970	/* Rmip */
#define RT_CK_RMI(_p)	BU_CKMAG(_p, RT_MANY_INTERNAL_MAGIC, "rt_many_internal")

/*
 *			R T _ S H O O T _ M A N Y _ R A Y S _ W O R K E R
 *
 *  Internal helper routine for rt_shoot_many_rays().
 *  Runs in PARALLEL, one instance per thread.
 *
 *  In order to reduce the traffic through the critical section,
 *  a multiple pixel block may be removed from the work queue at once.
 */
void
rt_shoot_many_rays_worker(int cpu, genptr_t arg)
{
	LOCAL struct application app;
	struct rt_many_internal *rmip = (struct rt_many_internal *)arg;

	if( cpu >= MAX_PSW )  {
		bu_log("rt_shoot_many_rays_worker() cpu %d > MAX_PSW %d, array overrun\n", cpu, MAX_PSW);
		rt_bomb("rt_shoot_many_rays_worker() cpu > MAX_PSW, array overrun\n");
	}

	RT_CK_RMI(rmip);
	RT_CK_RESOURCE( &rmip->resources[cpu] );
	RT_CK_APPLICATION( rmip->proto_ap );

	app = *rmip->proto_ap;			/* struct copy */
	app.a_resource = &rmip->resources[cpu];

	while(1)  {
		register long	index;
		register long	lim;

		if( rmip->stop_worker )  break;

		bu_semaphore_acquire( RT_SEM_WORKER );
		index = rmip->cur_index;
		rmip->cur_index += rmip->sem_chunk;
		bu_semaphore_release( RT_SEM_WORKER );

		lim = index + rmip->sem_chunk;
		for( ; index < lim; index++ )  {
			if( index >= rmip->max_index )  return;

			/*
			 * a_x is set here to get differentiated LIBRT
			 * debugging messages even from a trivial callback.
			 * The callback may, of course, override it.
			 */
			app.a_x = index;

			/* Allow our user to do per-ray init of application struct */
			if( (*rmip->callback)( &app, index ) < 0 )  {
				rmip->stop_worker = 1;
				break;
			}

			(void)rt_shootray( &app );
		}
	}
}

/*
 *			R T _ S H O O T _ M A N Y _ R A Y S
 *
 *  A convenience routine for application developers who wish to fire a
 *  large but fixed number of rays in parallel,
 *  without wanting to create a parallel "self dispatcher"
 *  routine of their own.
 *
 *  Basic setup of the application structure is done by the caller,
 *  and provided via the proto_ap pointer.
 *
 *  Per-ray setup of the application structure is done by the callback
 *  routine, which takes an index in the range 0..(nrays-1) and uses that
 *  to fill in each specific instance of application structure as required.
 *
 *  The a_hit() and a_miss() routines must save any results;
 *  their formal return codes, and the return code from rt_shootray(),
 *  are ignored.
 *
 *  a_x is changed by this wrapper, and may be overridden by the callback.
 *
 *  Note that the cost of spawning threads is sufficiently expensive
 *  that 'nrays' should be at least dozens or hundreds to get
 *  a real benefit from parallelism.
 *
 *  Return codes expected from the callback() -
 *	-1	End processing before all nrays have been fired.
 *	 0	Normal return, proceed with firing the ray.
 *
 *  Note that bu_parallel() is not re-entrant, so you can't have an
 *  a_hit() routine which is already running in parallel call into
 *  this routine and expect to get even more parallelism.
 *  This is not a limitation, as you usually can't construct more CPUs.
 */
void
rt_shoot_many_rays( proto_ap, callback, ncpus, nrays, resources )
const struct application *proto_ap;
int			callback BU_ARGS((struct application *, int index));
int			ncpus;
long			nrays;
struct resource		*resources;	/* resources[ncpus] */
{
	struct rt_many_internal	rmi;
	int	i;

	RT_CK_APPLICATION(proto_ap);
	for( i=0; i < ncpus; i++ )  {
		RT_CK_RESOURCE( &resources[i] );
	}
	rmi.resources = resources;

	rmi.magic = RT_MANY_INTERNAL_MAGIC;
	rmi.stop_worker = 0;
	rmi.cur_index = 0;
	rmi.max_index = nrays;
	rmi.proto_ap = proto_ap;
	rmi.callback = callback;
	rmi.sem_chunk = ncpus;

	if( !rt_g.rtg_parallel || ncpus <= 1 )  {
		/* The 1-cpu case is supported for testing & generality. */
		rt_shoot_many_rays_worker( 0, (genptr_t)&rmi );
	} else {
		bu_parallel( rt_shoot_many_rays_worker, ncpus, (genptr_t)&rmi );
	}
}
