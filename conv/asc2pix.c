/*
 *  			A S C 2 P I X . C
 *
 *  Convert ASCII (hex) pixel files to the binary form.
 *  For portable images.
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
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>

main()
{
	static int r,g,b;

	while( !feof( stdin )  &&
	    scanf( "%x %x %x", &r, &g, &b ) == 3 )  {
		putc( r, stdout );
		putc( g, stdout );
		putc( b, stdout );
	}
	fflush(stdout);
	exit(0);
}
