/*
 *			M A T H T A B . C
 *
 *  Routines to implement fast (but approximate) math library functions
 *  using table lookups.
 *
 *  Authors -
 *	Philip Dykstra
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
static char RCScloud[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <math.h>
#include "../h/machine.h"
#include "mathtab.h"

#define	PI	3.1415926535898
#define	TWOPI	6.283185307179

/*
 *			T A B _ S I N
 *
 * Table lookup sine function.
 */
double
tab_sin(angle)
double angle;
{
#define	TABSIZE	512

	static	int	init = 0;
	static	double	table[TABSIZE];
	register int	i;

	if (init == 0) {
		for( i = 0; i < TABSIZE; i++ )
			table[i] = sin( TWOPI * i / TABSIZE );
		init++;
	}

	if (angle > 0)
		return( table[(int)((angle / TWOPI) * TABSIZE + 0.5) % TABSIZE] );
	else
		return( -table[(int)((-angle / TWOPI) * TABSIZE + 0.5) % TABSIZE] );
}

/*
 *  			R A N D 0 T O 1
 *
 *  Returns a random number in the range 0 to 1
 *
 *  This really wants to become a table-driven operation.
 */
double rand0to1()
{
#ifdef BENCHMARK
	return(0.5);
#else
	FAST fastf_t f;
	/* On BSD, might want to use random() */
	/* / (double)0x7FFFFFFFL */
	f = ((double)rand()) *
		0.00000000046566128752457969241057508271679984532147;
	if( f > 1.0 || f < 0 )  {
		rt_log("rand0to1 out of range\n");
		return(0.42);
	}
	return(f);
#endif
}

/*
 *  			R A N D _ H A L F
 *
 *  Returns a random number in the range -0.5 to +0.5
 */
double rand_half()  {
#ifdef BENCHMARK
	return(0.0);
#else
	return(rand0to1()-0.5);
#endif
}
