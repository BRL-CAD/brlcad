/*
 *  			P I X 2 A S C . C
 *  
 *  Author -
 *  	Michael John Muuss
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
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>

unsigned long line[512];

main()
{
	register unsigned long *ip;

	while( read( 0, line, sizeof(line) ) == sizeof(line) )  {
		for( ip = line; ip < &line[512]; ip++ )
			printf("%6.6x ", *ip & 0xFFFFFF );
		printf("\n");
	}
}
