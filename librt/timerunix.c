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
 *			R E A D _ T I M E R
 * 
 */
double
read_timer(str,len)
char *str;
{
	long now;
	double usert;
	char line[132];

	(void)time(&now);
	sprintf(line,"%ld clock seconds\n", (long)now-time0);
	usert = now-time0;
	if( usert < 0.00001 )  usert = 0.00001;
	(void)strncpy( str, line, len );
	return( usert );
}

bcopy(from, to, count)  {
	memcpy( to, from, count );
}
