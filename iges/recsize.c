/*
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
 *  Copyright Notice -
 *	This software is Copyright (C) 1990-2004 by the United States Army.
 *	All rights reserved.
 */

/* Routine to determine record size of IGES file.
	According to the spec, the file should be
	80 characters per record.  The spec does not
	mention anything about CR or LF at the end of records,
	so this routine looks for LF's and returns the actual
	record length.
	Note: this will work for files with records that are
	80 characters long without any CR or LF at end of records,
	also for files with CR-LF at end of records, and for files
	with just LF at end of records.  It will not work for files
	with just CR at end of records (but I haven't seen such an animal
	yet) */

#include "./iges_struct.h"
#include "./iges_extern.h"
#define	NRECS	20	/* Maximum number of records to sample */
#define	NCHAR	256	/* Maximuim number of characters to read
				in case there are no LF's */
extern int errno;

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

