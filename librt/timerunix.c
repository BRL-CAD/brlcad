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
 *			P R E P _ T I M E R
 */
void
prep_timer()
{
	(void)time(&time0);
}


/*
 *			P R _ T I M E R
 * 
 */
double
pr_timer(str)
char *str;
{
	long now;
	double usert;

	(void)time(&now);
	fprintf(stderr,"%s: %ld secs\n", str, (long)now-time0);
	usert = now-time0;
	return( usert );
}
