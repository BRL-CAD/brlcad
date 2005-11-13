/*                       R E C S I Z E . C
 * BRL-CAD
 *
 * Copyright (C) 1990-2005 United States Government as represented by
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
/** @file recsize.c
 *
 * Routine to determine record size of IGES file.
 *	According to the spec, the file should be
 *	80 characters per record.  The spec does not
 *	mention anything about CR or LF at the end of records,
 *	so this routine looks for LF's and returns the actual
 *	record length.
 *	Note: this will work for files with records that are
 *	80 characters long without any CR or LF at end of records,
 *	also for files with CR-LF at end of records, and for files
 *	with just LF at end of records.  It will not work for files
 *	with just CR at end of records (but I haven't seen such an animal
 *	yet)
 *
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

#include "common.h"

#include <errno.h>

#include "./iges_struct.h"
#include "./iges_extern.h"

#define	NRECS	20	/* Maximum number of records to sample */
#define	NCHAR	256	/* Maximuim number of characters to read
				in case there are no LF's */

int
Recsize()
{

	int i,j,k=(-1),recl=0,length[NRECS],ch;

	for( j=0 ; j<NRECS ; j++ )
	{
		i = 1;
		while( (ch=getc( fd ) ) != '\n' && i < NCHAR && ch != EOF )
			i++;
		if( i == NCHAR )
		{
			recl = 80;
			break;
		}
		else if( ch == EOF )
		{
			k = j - 1;
			break;
		}
		else
			length[j] = i; /* record this record length */
	}
	if( k == (-1) )	/* We didn't encounter an early EOF */
		k = NRECS;

	if( fseek( fd , 0L , 0 ) ) /* rewind file */
	{
		bu_log( "Cannot rewind file\n" );
		perror( "Recsize" );
		exit( 1 );
	}

	if( recl == 0 )	/* then LF's were found */
	{
		recl = length[1];	/* don't use length[0] */

		/* check for consistent record lengths */
		for( j=2 ; j<k ; j++ )
		{
			if( recl != length[j] )
				return( 0 );
		}
	}
	return( recl );
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
