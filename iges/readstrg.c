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

/* This routine reads the next field in "card" buffer
	It expects the field to contain a character string
	of the form "nHstring" where n is the length
	of "string". If "id" is not the null string, then
	"id" is printed followed by "string".  If "id" is null,
	then the only action taken is to read the string
	(effectively skipping the field).

	"eof" is the "end-of-field" delimiter
	"eor" is the "end-of-record" delimiter	*/

#include "./iges_struct.h"
#include "./iges_extern.h"

void
Readstrg( id )
char *id;
{
	int i=(-1),length=0,done=0,lencard;
	char num[80];

	if( card[counter] == eof ) /* This is an empty field */
	{
		counter++;
		return;
	}
	else if( card[counter] == eor ) /* Up against the end of record */
		return;

	if( card[72] == 'P' )
		lencard = PARAMLEN;
	else
		lencard = CARDLEN;

	if( counter > lencard )
		Readrec( ++currec );

	if( *id != '\0' )
		bu_log( "%s" , id );

	while( !done )
	{
		while( (num[++i] = card[counter++]) != 'H' &&
				counter <= lencard);
		if( counter > lencard )
			Readrec( ++currec );
		if( num[i] == 'H' )
			done = 1;
	}
	num[++i] = '\0';
	length = atoi( num );
	for( i=0 ; i<length ; i++ )
	{
		if( counter > lencard )
			Readrec( ++currec );
		if( *id != '\0' )
			bu_log( "%c", card[counter] );
		counter++;
	}
	if( *id != '\0' )
		bu_log( "%c", '\n' );

	while( card[counter] != eof && card[counter] != eor )
	{
		if( counter < lencard )
			counter++;
		else
			Readrec( ++currec );
	}

	if( card[counter] == eof )
	{
		counter++;
		if( counter > lencard )
			Readrec( ++ currec );
	}
}

