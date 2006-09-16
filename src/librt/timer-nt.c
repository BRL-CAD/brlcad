/*                      T I M E R - N T . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** @addtogroup timer */
/*@{*/
/** @file timer-nt.c
 * To provide timing information on Microsoft Windows NT.
 */

#ifndef lint
static const char RCStimer_nt[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"

/* Standard System V stuff */
static clock_t start;
time_t time0;

/*
 *			P R E P _ T I M E R
 */
void
rt_prep_timer(void)
{
	start = clock();
	time( &time0 );
}

/*
 *			R T _ G E T _ T I M E R
 *
 *  Reports on the passage of time, since rt_prep_timer() was called.
 *  Explicit return is number of CPU seconds.
 *  String return is descriptive.
 *  If "elapsed" pointer is non-null, number of elapsed seconds are returned.
 *  Times returned will never be zero.
 */
double
rt_get_timer(struct bu_vls	*vp, double *elapsed)
{
	long	now;
	double	user_cpu_secs;
	double	sys_cpu_secs;
	double	elapsed_secs;
	double	percent;
	clock_t finish;

	/* Real time.  1 second resolution. */
	(void)time(&now);
	elapsed_secs = difftime(now,time0);

	finish = clock();
	sys_cpu_secs = (double)(finish - start);
	sys_cpu_secs /= CLOCKS_PER_SEC;

	user_cpu_secs = sys_cpu_secs;

	if( user_cpu_secs < 0.00001 )  user_cpu_secs = 0.00001;
	if( elapsed_secs < 0.00001 )  elapsed_secs = user_cpu_secs;	/* It can't be any less! */

	if( elapsed )  *elapsed = elapsed_secs;

	if( vp )  {
		percent = user_cpu_secs/elapsed_secs*100.0;
		BU_CK_VLS(vp);
		bu_vls_printf( vp,
			"%g user + %g sys in %g elapsed secs (%g%%)",
			user_cpu_secs, sys_cpu_secs, elapsed_secs, percent );
	}
	return( user_cpu_secs );
}

double
rt_read_timer(char *str, int len)
{
	struct bu_vls	vls;
	double		cpu;
	int		todo;

	if( !str )  return  rt_get_timer( (struct bu_vls *)0, (double *)0 );

	bu_vls_init( &vls );
	cpu = rt_get_timer( &vls, (double *)0 );
	todo = bu_vls_strlen( &vls );
	if( todo > len )  todo = len-1;
	strncpy( str, bu_vls_addr(&vls), todo );
	str[todo] = '\0';
	return cpu;
}

/*@}*/
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
