/*
 *			C O N T O U R S . C
 *
 *  Program to read "Brain Mapping Project" data and plot it.
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1986 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>

int	x,y,z;
int	npts;
char	name[128];

main()
{
	register int i;

	pl_3space( stdout, -32768,  -32768,  -32768, 32767, 32767, 32767 );
	while( !feof(stdin) )  {
		if( scanf( "%d %d %s", &npts, &z, name ) != 3 )  break;
		for( i=0; i<npts; i++ )  {
			if( scanf( "%d %d", &x, &y ) != 2 )
				fprintf(stderr,"bad xy\n");
			if( i==0 )
				pl_3move( stdout, x, y, z );
			else
				pl_3cont( stdout, x, y, z );
		}
		/* Close curves? */
	}
}
