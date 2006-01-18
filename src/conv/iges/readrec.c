/*                       R E A D R E C . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file readrec.c
 *  Authors -
 *	John R. Anderson
 *	Susanne L. Muuss
 *	Earl P. Weaver
 *
 *  Source -
 *	VLD/ASB Building 1065
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 */

/*	This routine reads record number "recno" from the IGES file and places
	it in the "card" buffer (available for reading by "readstrg", "readint",
	"readflt", "readtime", "readcols", etc.  The record is located
	by calculating the offset into the file and doing an "fseek".  This
	obviously depends on the "UNIX" characteristic of "fseek" understanding
	offsets in bytes.  The record length is calculated by the routine "recsize".
	This routine id typically called before calling the other "read" routines
	to get to the desired record.  The "read" routines then call this routine
	if the buffer empties.	*/

#include "./iges_struct.h"
#include "./iges_extern.h"

int
Readrec( recno )
int recno;
{

	int i,ch;
	long offset;

	currec = recno;
	offset = (recno - 1) * reclen;
	if( fseek( fd , offset , 0 ) )
	{
		bu_log( "Error in seek\n" );
		perror( "Readrec" );
		exit( 1 );
	}
	counter = 0;

	for( i=0 ; i<reclen ; i++ )
	{
		if( (ch=getc( fd )) == EOF )
			return( 1 );
		card[i] = ch;
	}

	return( 0 );
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
