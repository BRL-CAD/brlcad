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
 *	This software is Copyright (C) 1985-2004 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"



#include <stdio.h>
#include <stdlib.h>

unsigned char pix[3];		/* RGB of one pixel */

char map[18] = "0123456789ABCDEFx";

int
main(void)
{
	while( !feof(stdin) &&
	    fread( (char *)pix, sizeof(pix), 1, stdin) == 1 )  {
		putc( map[pix[0]>>4], stdout );
		putc( map[pix[0]&0xF], stdout );
		putc( map[pix[1]>>4], stdout );
		putc( map[pix[1]&0xF], stdout );
		putc( map[pix[2]>>4], stdout );
		putc( map[pix[2]&0xF], stdout );
		putc( '\n', stdout );
	}
	exit(0);
}
