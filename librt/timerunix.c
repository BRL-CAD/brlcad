/*
 *			T I M E R 5 2 . C
 *
 * Function -
 *	To provide timing information for RT.
 *	This version for System V, Release TWO.
 *	(This merely determines elapsed time, not CPU time, alas)
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
static char RCStimer[] = "@(#)$Header $ (BRL)";
#endif

#include <stdio.h>
#include <memory.h>
#include <sys/types.h>
#include <sys/times.h>
#include <sys/param.h>

#ifndef HZ
/* It's not always in sys/param.h */
#define HZ	60
#endif

bzero( str, n )
{
	memset( str, '\0', n );
}

/* Standard System V stuff */
extern long time();
static long time0;
static struct tms tms0;

/*
 *			P R E P _ T I M E R
 */
void
rt_prep_timer()
{
	(void)time(&time0);
	(void)times(&tms0);
}


/*
 *			R E A D _ T I M E R
 * 
 */
double
rt_read_timer(str,len)
char *str;
{
	long now;
	double usert;
	double realt;
	struct tms tmsnow;
	char line[132];

	(void)time(&now);
	realt = now-time0;
	(void)times(&tmsnow);
	usert = tmsnow.tms_utime - tms0.tms_utime;
	usert /= HZ;
	if( usert < 0.00001 )  usert = 0.01;
	if( realt < 0.00001 )  realt = usert;
	sprintf(line,"%g CPU secs in %g elapsed secs (%g%%)",
		usert, realt,
		usert/realt*100 );
	(void)strncpy( str, line, len );
	return( usert );
}

bcopy(from, to, count)  {
	memcpy( to, from, count );
}
