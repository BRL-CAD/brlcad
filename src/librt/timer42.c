/*                       T I M E R 4 2 . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2006 United States Government as represented by
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
/** @file timer42.c
 * To provide timing information for RT when running on 4.2 BSD UNIX.
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 */

#ifndef lint
static const char RCStimer[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"



#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "raytrace.h"

static struct	timeval time0;	/* Time at which timeing started */
static struct	rusage ru0;	/* Resource utilization at the start */
static struct	rusage ru0c;	/* Resource utilization at the start */

static void prusage(register struct rusage *r0, register struct rusage *r1, struct timeval *e, struct timeval *b, struct bu_vls *vp);
#if 0
static void tvadd();
#endif
static void tvsub(struct timeval *tdiff, struct timeval *t1, struct timeval *t0);
static void psecs(long int l, struct bu_vls *vp);



/*
 *			R T _ P R E P _ T I M E R
 */
void
rt_prep_timer(void)
{
	gettimeofday(&time0, (struct timezone *)0);
	getrusage(RUSAGE_SELF, &ru0);
	getrusage(RUSAGE_CHILDREN, &ru0c);
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
rt_get_timer(struct bu_vls *vp, double *elapsed)
{
	struct timeval timedol;
	struct rusage ru1;
	struct rusage ru1c;
	struct timeval td;
	double	user_cpu_secs;
	double	elapsed_secs;


	getrusage(RUSAGE_SELF, &ru1);
	getrusage(RUSAGE_CHILDREN, &ru1c);
	gettimeofday(&timedol, (struct timezone *)0);

	elapsed_secs = (timedol.tv_sec - time0.tv_sec) +
		(timedol.tv_usec - time0.tv_usec)/1000000.0;

	tvsub( &td, &ru1.ru_utime, &ru0.ru_utime );
	user_cpu_secs = td.tv_sec + ((double)td.tv_usec) / 1000000.0;

	tvsub( &td, &ru1c.ru_utime, &ru0c.ru_utime );
	user_cpu_secs += td.tv_sec + ((double)td.tv_usec) / 1000000.0;

	if( user_cpu_secs < 0.00001 )  user_cpu_secs = 0.00001;
	if( elapsed_secs < 0.00001 )  elapsed_secs = user_cpu_secs;	/* It can't be any less! */

	if( elapsed )  *elapsed = elapsed_secs;

	if( vp )  {
		bu_vls_printf( vp, "cpu = %g sec, elapsed = %g sec\n",
			user_cpu_secs, elapsed_secs );
		bu_vls_strcat( vp, "    parent: " );
		prusage(&ru0, &ru1, &timedol, &time0, vp);
		bu_vls_strcat( vp, "\n  children: ");
		prusage(&ru0c, &ru1c, &timedol, &time0, vp);
	}

	return( user_cpu_secs );
}

static void
prusage(register struct rusage *r0, register struct rusage *r1, struct timeval *e, struct timeval *b, struct bu_vls *vp)
{
	struct timeval tdiff;
	register time_t t;
	register char *cp;
	register int i;
	int ms;

	BU_CK_VLS(vp);

	t = (r1->ru_utime.tv_sec-r0->ru_utime.tv_sec)*100+
	    (r1->ru_utime.tv_usec-r0->ru_utime.tv_usec)/10000+
	    (r1->ru_stime.tv_sec-r0->ru_stime.tv_sec)*100+
	    (r1->ru_stime.tv_usec-r0->ru_stime.tv_usec)/10000;
	ms =  (e->tv_sec-b->tv_sec)*100 + (e->tv_usec-b->tv_usec)/10000;

	cp = "%Uuser %Ssys %Ereal %P %Xi+%Dd %Mmaxrss %F+%Rpf %Ccsw";
	for (; *cp; cp++)  {
		if (*cp != '%')
			bu_vls_putc( vp, *cp );
		else if (cp[1]) switch(*++cp) {

		case 'U':
			tvsub(&tdiff, &r1->ru_utime, &r0->ru_utime);
			bu_vls_printf(vp, "%ld.%01ld", (long)tdiff.tv_sec, (long)(tdiff.tv_usec/100000));
			break;

		case 'S':
			tvsub(&tdiff, &r1->ru_stime, &r0->ru_stime);
			bu_vls_printf(vp, "%ld.%01ld", (long)tdiff.tv_sec, (long)(tdiff.tv_usec/100000));
			break;

		case 'E':
			psecs(ms / 100, vp);
			break;

		case 'P':
			bu_vls_printf(vp, "%d%%", (int) (t*100 / ((ms ? ms : 1))));
			break;

		case 'W':
			i = r1->ru_nswap - r0->ru_nswap;
			bu_vls_printf(vp, "%d", i);
			break;

		case 'X':
			bu_vls_printf(vp, "%ld", (long)(t == 0 ? 0 : (r1->ru_ixrss-r0->ru_ixrss)/t));
			break;

		case 'D':
			bu_vls_printf(vp, "%ld", (long)(t == 0 ? 0 : (r1->ru_idrss+r1->ru_isrss-(r0->ru_idrss+r0->ru_isrss))/t));
			break;

		case 'K':
			bu_vls_printf(vp, "%ld", (long)(t == 0 ? 0 :  ((r1->ru_ixrss+r1->ru_isrss+r1->ru_idrss) - (r0->ru_ixrss+r0->ru_idrss+r0->ru_isrss))/t));
			break;

		case 'M':
			bu_vls_printf(vp, "%ld", (long)(r1->ru_maxrss/2));
			break;

		case 'F':
			bu_vls_printf(vp, "%ld", (long)(r1->ru_majflt-r0->ru_majflt));
			break;

		case 'R':
			bu_vls_printf(vp, "%ld", (long)(r1->ru_minflt-r0->ru_minflt));
			break;

		case 'I':
			bu_vls_printf(vp, "%ld", (long)(r1->ru_inblock-r0->ru_inblock));
			break;

		case 'O':
			bu_vls_printf(vp, "%ld", (long)(r1->ru_oublock-r0->ru_oublock));
			break;
		case 'C':
			bu_vls_printf(vp, "%ld+%ld", (long)(r1->ru_nvcsw-r0->ru_nvcsw), (long)(r1->ru_nivcsw-r0->ru_nivcsw) );
			break;
		}
	}
}

#if 0
static void
tvadd(tsum, t0)
	struct timeval *tsum, *t0;
{

	tsum->tv_sec += t0->tv_sec;
	tsum->tv_usec += t0->tv_usec;
	if (tsum->tv_usec > 1000000)
		tsum->tv_sec++, tsum->tv_usec -= 1000000;
}
#endif

static void
tvsub(struct timeval *tdiff, struct timeval *t1, struct timeval *t0)
{

	tdiff->tv_sec = t1->tv_sec - t0->tv_sec;
	tdiff->tv_usec = t1->tv_usec - t0->tv_usec;
	if (tdiff->tv_usec < 0)
		tdiff->tv_sec--, tdiff->tv_usec += 1000000;
}

static void
psecs(long int l, struct bu_vls *vp)
{
	register int i;

	i = l / 3600;
	if (i) {
		register int	j;
		bu_vls_printf(vp, "%d:", i);
		i = l % 3600;
		j = i / 60;
		bu_vls_printf(vp, "%d%d", j / 10, j % 10);
	} else {
		i = l;
		bu_vls_printf(vp, "%d", i / 60);
	}
	i = i % 60; /* GSM: bug in Alliant CE optimization prohibits "%=" here */
	bu_vls_printf(vp, ":%d%d", i / 10, i % 10);
}

/*
 *			R T _ R E A D _ T I M E R
 *
 *  Compatability routine
 */
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
