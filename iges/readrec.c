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
 *	This software is Copyright (C) 1990 by the United States Army.
 *	All rights reserved.
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

