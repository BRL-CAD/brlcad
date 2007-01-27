/*                       P I X 2 A S C . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file pix2asc.c
 *
 *  Author -
 *  	Michael John Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
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

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
