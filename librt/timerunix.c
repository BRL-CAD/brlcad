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
static char RCSid[] = "@(#)$Header $ (BRL)";
#endif

#include <stdio.h>
#include <memory.h>

bzero( str, n )
{
	memset( str, '\0', n );
}

/* Standard System V stuff */
extern long time();
static long time0;


/*
 *			T I M E R _ P R E P
 */
void
timer_prep()
{
	(void)time(&time0);
}


/*
 *			T I M E R _ P R I N T
 * 
 */
double
timer_print(str)
char *str;
{
	long now;
	double usert;

	(void)time(&now);
	fprintf(stderr,"%s: %ld secs\n", str, (long)now-time0);
	usert = now-time0;
	return( usert );
}
