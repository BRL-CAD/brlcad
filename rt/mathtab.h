/*
 *			M A T H T A B . H
 *
 *  Definitions to implement fast (but approximate) math library functions
 *  using table lookups.
 *
 *  Authors -
 *	Phillip Dykstra
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
 *
 *  $Header$
 */

#define RANDTABSIZE	2047	/* Powers of two give streaking */
#define	SINTABSIZE	2048

extern float rand_tab[];

extern double sin_scale;
extern float sin_table[];

#define rand_half(_p)	\
	( (++(_p) >= &rand_tab[RANDTABSIZE] || (_p) < rand_tab) ? \
		*((_p) = rand_tab) : *(_p))

#define rand_init(_p, _seed)	\
	(_p) = &rand_tab[(int)((rand_tab[(_seed)%RANDTABSIZE] + \
		0.5)*(RANDTABSIZE-1))]

#define rand0to1(_q)	(rand_half(_q)+0.5)

#define tab_sin(_a)	(((_a) > 0) ? \
	( sin_table[(int)((0.5 + (_a) * sin_scale))&(SINTABSIZE-1)] ) :\
	(-sin_table[(int)((0.5 - (_a) * sin_scale))&(SINTABSIZE-1)] ) )
